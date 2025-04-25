#include <iostream>
#include <winsock2.h>
#include <vector>
#include <random>
#include <iomanip>
#include <cstdint>
#include <thread>


#pragma comment(lib, "ws2_32.lib")

enum TLVType : uint8_t {
    TYPE_MATRIX_SIZE = 1,
    TYPE_NUM_THREADS = 2,
    TYPE_MATRIX_DATA = 3,
    TYPE_COMMAND = 4
};

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

void printMatrix(const std::vector<int>& matrix, int size) {
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            std::cout << std::setw(4) << matrix[i * size + j] << " ";
        }
        std::cout << "\n";
    }
}

int main() {
    int size = 10, numThreads = 6;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(1, 99);
    std::vector<int> matrix(size * size);
    for (auto& val : matrix) val = dist(gen);

    printMatrix(matrix, size);

    WSADATA wsData;
    WSAStartup(MAKEWORD(2, 2), &wsData);
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(7777);
    server.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

    if (connect(sock, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        std::cerr << "Connection failed\n";
        return 1;
    }

    uint8_t type;
    std::vector<char> value;
    if (receiveTLV(sock, type, value)) {
        std::string msg(value.begin(), value.end());
        std::cout << "Server: " << msg << std::endl;
    }

    sendTLV(sock, TYPE_MATRIX_SIZE, &size, sizeof(size));
    sendTLV(sock, TYPE_NUM_THREADS, &numThreads, sizeof(numThreads));
    sendTLV(sock, TYPE_MATRIX_DATA, matrix.data(), matrix.size() * sizeof(int));
    std::string command = "Start execution";
    sendTLV(sock, TYPE_COMMAND, command.data(), command.size());

    // Читання повідомлень від сервера
    while (receiveTLV(sock, type, value)) {
        if (type == TYPE_COMMAND) {
            std::string msg(value.begin(), value.end());
            std::cout << "Server: " << msg << std::endl;
            if (msg.find("Awaiting result") != std::string::npos) break;
        }
    }

    // Запит статусу
    std::string status = "Status";
    sendTLV(sock, TYPE_COMMAND, status.data(), status.size());
    if (receiveTLV(sock, type, value)) {
        std::string msg(value.begin(), value.end());
        std::cout << "Server: " << msg << std::endl;
    }

    // Запит результату
    std::string getResult = "Get result";
    sendTLV(sock, TYPE_COMMAND, getResult.data(), getResult.size());

    if (receiveTLV(sock, type, value) && type == TYPE_MATRIX_DATA) {
        std::vector<int> resultMatrix(reinterpret_cast<int*>(value.data()), reinterpret_cast<int*>(value.data() + value.size()));
        std::cout << "Mirrored matrix:\n";
        printMatrix(resultMatrix, size);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
