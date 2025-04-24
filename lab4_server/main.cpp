#include <iostream>
#include <winsock2.h>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

enum TLVType : uint8_t {
    TYPE_MATRIX_SIZE = 1,
    TYPE_NUM_THREADS = 2,
    TYPE_MATRIX_DATA = 3,
    TYPE_COMMAND = 4
};

void printArray(int** matrix, int size);

sockaddr_in initSocketAddr(sockaddr_in sockaddrIn);

bool receiveTLV(SOCKET socket, uint8_t& type, std::vector<char>& value) {
    type = 0;
    uint32_t length = 0;
    int recvType = recv(socket, reinterpret_cast<char*>(&type), sizeof(type), 0);
    if (recvType <= 0) return false;

    int recvLen = recv(socket, reinterpret_cast<char*>(&length), sizeof(length), 0);
    if (recvLen <= 0) return false;

    value.resize(length);
    int received = 0;
    while (received < static_cast<int>(length)) {
        int chunk = recv(socket, value.data() + received, length - received, 0);
        if (chunk <= 0) return false;
        received += chunk;
    }
    return true;
}

void sendTLV(SOCKET socket, TLVType type, const void* data, uint32_t length) {
    send(socket, reinterpret_cast<const char*>(&type), sizeof(type), 0);
    send(socket, reinterpret_cast<const char*>(&length), sizeof(length), 0);
    send(socket, reinterpret_cast<const char*>(data), length, 0);
}

void mirrorMatrix(int** matrix, int size, int numThreads = 6) {
    std::vector<std::thread> threads;
    auto swapElements = [&](int start, int end) {
        for (int i = start; i < end; ++i) {
            for (int j = 0; j < i; ++j) {
                std::swap(matrix[i][j], matrix[j][i]);
            }
        }
    };

    int block_size = size / numThreads;
    int start = 0;
    int end = block_size;

    for (int i = 0; i < numThreads - 1; ++i) {
        threads.emplace_back(swapElements, start, end);
        start = end;
        end += block_size;
    }

    threads.emplace_back(swapElements, start, size);

    for (auto& thread : threads) {
        thread.join();
    }
}

void taskExecution(SOCKET socket) {
    try {
        uint8_t type;
        std::vector<char> value;

        int matrixSize = 0;
        int numThreads = 0;
        int** matrix = nullptr;

        std::cout << "Client connected! Thread id: " << std::this_thread::get_id() << std::endl;

        while (receiveTLV(socket, type, value)) {
            switch (type) {
                case TYPE_MATRIX_SIZE:
                    memcpy(&matrixSize, value.data(), sizeof(int));
                    break;
                case TYPE_NUM_THREADS:
                    memcpy(&numThreads, value.data(), sizeof(int));
                    break;
                case TYPE_MATRIX_DATA: {
                    matrix = new int*[matrixSize];
                    const int* data = reinterpret_cast<int*>(value.data());
                    for (int i = 0; i < matrixSize; ++i) {
                        matrix[i] = new int[matrixSize];
                        for (int j = 0; j < matrixSize; ++j) {
                            matrix[i][j] = data[i * matrixSize + j];
                        }
                    }
                    break;
                }
                case TYPE_COMMAND: {
                    std::string command(value.begin(), value.end());
                    if (command != "Start execution") throw std::runtime_error("Invalid command");
                    std::cout << "Start command received" << std::endl;

                    const char* ack1 = "Execution begin";
                    sendTLV(socket, TYPE_COMMAND, ack1, strlen(ack1));

                    std::thread worker(mirrorMatrix, matrix, matrixSize, numThreads);
                    worker.join();

                    std::cout << "Execution ended! Thread id: " << std::this_thread::get_id() << std::endl;

                    const char* ack2 = "Execution ended";
                    sendTLV(socket, TYPE_COMMAND, ack2, strlen(ack2));

                    // Повернення матриці у вигляді TLV
                    std::vector<int> flattened;
                    for (int i = 0; i < matrixSize; ++i) {
                        for (int j = 0; j < matrixSize; ++j) {
                            flattened.push_back(matrix[i][j]);
                        }
                    }
                    sendTLV(socket, TYPE_MATRIX_DATA, flattened.data(), flattened.size() * sizeof(int));

                    //printArray(matrix, matrixSize);

                    for (int i = 0; i < matrixSize; ++i) delete[] matrix[i];
                    delete[] matrix;

                    std::cout << "Client task completed" << std::endl;
                    return;
                }
                default:
                    throw std::runtime_error("Unknown TLV type");
            }
        }
    } catch (const std::exception& ex) {
        const char* errorMsg = "Execution error";
        sendTLV(socket, TYPE_COMMAND, errorMsg, strlen(errorMsg));
    }

    closesocket(socket);
}

int main() {
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "Can't initialize WinSock!" << std::endl;
        return -1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Can't create socket!" << std::endl;
        return -1;
    }

    sockaddr_in serverAddr{};
    serverAddr = initSocketAddr(serverAddr);
    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, SOMAXCONN);

    std::cout << "Server is running on port 7777" << std::endl;

    while (true) {
        sockaddr_in client{};
        int clientSize = sizeof(client);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&client, &clientSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Client connection failed" << std::endl;
            continue;
        }

        std::thread t(taskExecution, clientSocket);
        t.detach();
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}

void printArray(int** matrix, int size) {
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            std::cout << matrix[i][j] << " ";
        }
        std::cout << std::endl;
    }
}

sockaddr_in initSocketAddr(sockaddr_in sockaddrIn) {
    sockaddrIn.sin_family = AF_INET;
    sockaddrIn.sin_port = htons(7777);
    sockaddrIn.sin_addr.S_un.S_addr = INADDR_ANY;
    return sockaddrIn;
}
