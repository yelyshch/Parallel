#include <iostream>
#include <winsock2.h>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

enum TLVType : uint8_t {
    TYPE_MATRIX_SIZE = 1,
    TYPE_NUM_THREADS = 2,
    TYPE_MATRIX_DATA = 3,
    TYPE_COMMAND = 4
};

enum Status {
    IDLE,
    PROCESSING,
    COMPLETED
};

std::mutex statusMutex;
Status currentStatus = IDLE;
std::vector<int> resultMatrix;

void sendTLV(SOCKET socket, TLVType type, const void* data, uint32_t length) {
    send(socket, reinterpret_cast<const char*>(&type), sizeof(type), 0);
    send(socket, reinterpret_cast<const char*>(&length), sizeof(length), 0);
    send(socket, reinterpret_cast<const char*>(data), length, 0);
}

bool receiveTLV(SOCKET socket, uint8_t& type, std::vector<char>& value) {
    uint32_t length = 0;
    if (recv(socket, reinterpret_cast<char*>(&type), sizeof(type), 0) <= 0) return false;
    if (recv(socket, reinterpret_cast<char*>(&length), sizeof(length), 0) <= 0) return false;

    value.resize(length);
    int received = 0;
    while (received < static_cast<int>(length)) {
        int chunk = recv(socket, value.data() + received, length - received, 0);
        if (chunk <= 0) return false;
        received += chunk;
    }
    return true;
}

void mirrorMatrix(int** matrix, int size, int numThreads) {
    std::vector<std::thread> threads;
    auto swap = [&](int start, int end) {
        for (int i = start; i < end; ++i)
            for (int j = 0; j < i; ++j)
                std::swap(matrix[i][j], matrix[j][i]);
    };

    int block = size / numThreads;
    int start = 0, end = block;

    for (int i = 0; i < numThreads - 1; ++i) {
        threads.emplace_back(swap, start, end);
        start = end;
        end += block;
    }

    threads.emplace_back(swap, start, size);

    for (auto& t : threads) t.join();
}

void taskExecution(SOCKET socket) {
    try {
        uint8_t type;
        std::vector<char> value;

        int matrixSize = 0;
        int numThreads = 0;
        int** matrix = nullptr;
        Status status = IDLE;

        std::cout << "[LOG] Client connected! Thread id: " << std::this_thread::get_id() << std::endl;

        // Повідомляємо клієнту про підключення
        const char* connMsg = "Connected to server";
        sendTLV(socket, TYPE_COMMAND, connMsg, strlen(connMsg));

        while (receiveTLV(socket, type, value)) {
            switch (type) {
                case TYPE_MATRIX_SIZE:
                    memcpy(&matrixSize, value.data(), sizeof(int));
                    std::cout << "[LOG] Matrix size received: " << matrixSize << std::endl;
                    sendTLV(socket, TYPE_COMMAND, "Matrix size received", 20);
                    break;

                case TYPE_NUM_THREADS:
                    memcpy(&numThreads, value.data(), sizeof(int));
                    std::cout << "[LOG] Number of threads received: " << numThreads << std::endl;
                    sendTLV(socket, TYPE_COMMAND, "Threads received", 16);
                    break;

                case TYPE_MATRIX_DATA: {
                    std::cout << "[LOG] Receiving matrix data..." << std::endl;
                    matrix = new int*[matrixSize];
                    const int* data = reinterpret_cast<int*>(value.data());
                    for (int i = 0; i < matrixSize; ++i) {
                        matrix[i] = new int[matrixSize];
                        for (int j = 0; j < matrixSize; ++j) {
                            matrix[i][j] = data[i * matrixSize + j];
                        }
                    }
                    std::cout << "[LOG] Matrix data received" << std::endl;
                    sendTLV(socket, TYPE_COMMAND, "Matrix data received", 20);
                    break;
                }

                case TYPE_COMMAND: {
                    std::string command(value.data(), value.size());

                    if (command == "Start execution") {
                        std::cout << "[LOG] Start execution command received" << std::endl;
                        const char* ack1 = "Execution begin";
                        sendTLV(socket, TYPE_COMMAND, ack1, strlen(ack1));
                        status = PROCESSING;

                        // Обробка в окремому потоці
                        std::thread worker([&]() {
                            mirrorMatrix(matrix, matrixSize, numThreads);
                            status = COMPLETED;
                            std::cout << "[LOG] Matrix processing completed" << std::endl;
                        });

                        worker.join();
                        std::cout << "[LOG] Execution ended. Awaiting result request." << std::endl;
                        sendTLV(socket, TYPE_COMMAND, "Execution ended. Awaiting result request.", 41);

                    } else if (command == "Status") {
                        std::string statusStr = (status == IDLE ? "idle" :
                                                status == PROCESSING ? "processing" :
                                                "completed");
                        std::cout << "[LOG] Status requested -> " << statusStr << std::endl;
                        sendTLV(socket, TYPE_COMMAND, statusStr.c_str(), statusStr.size());

                    } else if (command == "Get result") {
                        std::cout << "[LOG] Client requested result" << std::endl;

                        // Повернення обробленої матриці
                        std::vector<int> flattened;
                        for (int i = 0; i < matrixSize; ++i)
                            for (int j = 0; j < matrixSize; ++j)
                                flattened.push_back(matrix[i][j]);

                        sendTLV(socket, TYPE_MATRIX_DATA, flattened.data(), flattened.size() * sizeof(int));
                        std::cout << "[LOG] Result sent to client" << std::endl;

                        // Очищення пам’яті
                        for (int i = 0; i < matrixSize; ++i) delete[] matrix[i];
                        delete[] matrix;

                        std::cout << "[LOG] Client task completed. Closing connection.\n" << std::endl;
                        return;

                    } else {
                        std::cout << "[LOG] Status: " << command << std::endl;
                        sendTLV(socket, TYPE_COMMAND, command.c_str(), command.size());
                    }
                    break;
                }

                default:
                    std::cerr << "[ERROR] Unknown TLV type received" << std::endl;
                    sendTLV(socket, TYPE_COMMAND, "Unknown TLV type", 16);
                    break;
            }
        }
    } catch (const std::bad_alloc&) {
        std::cerr << "[FATAL] Memory allocation failed (std::bad_alloc)" << std::endl;
        sendTLV(socket, TYPE_COMMAND, "Memory error", 12);
    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] Exception: " << ex.what() << std::endl;
        sendTLV(socket, TYPE_COMMAND, "Execution error", 15);
    }

    closesocket(socket);
}


sockaddr_in initSocketAddr() {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(7777);
    addr.sin_addr.S_un.S_addr = INADDR_ANY;
    return addr;
}

int main() {
    WSADATA wsData;
    WSAStartup(MAKEWORD(2, 2), &wsData);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr = initSocketAddr();
    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, SOMAXCONN);

    std::cout << "Server is running on port 7777\n";

    while (true) {
        sockaddr_in client{};
        int clientSize = sizeof(client);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&client, &clientSize);
        if (clientSocket != INVALID_SOCKET) {
            std::thread(taskExecution, clientSocket).detach();
        }
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
