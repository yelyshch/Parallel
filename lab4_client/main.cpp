#include <iostream>
#include <winsock2.h>
#include <vector>
#include <random>
#include <iomanip>
#include <cstdint>
#include <cstring>

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
    std::cout << "Generated matrix:\n";
    for (int i = 0; i < size * size; ++i) {
        std::cout << std::setw(4) << matrix[i];
        if ((i + 1) % size == 0) std::cout << std::endl;
    }
    std::cout << std::endl;
}

int main() {
    // === Налаштування розміру матриці ===
    int size=10000, numThreads = 1;

    // === Генерація випадкової матриці ===
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(1, 99);
    std::vector<int> matrix(size * size);

    for (auto& val : matrix) {
        val = dist(gen);
    }

    //printMatrix(matrix, size);

    // === WinSock setup ===
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

    // === Надсилання TLV даних ===
    sendTLV(sock, TYPE_MATRIX_SIZE, &size, sizeof(size));
    sendTLV(sock, TYPE_NUM_THREADS, &numThreads, sizeof(numThreads));
    sendTLV(sock, TYPE_MATRIX_DATA, matrix.data(), matrix.size() * sizeof(int));
    std::string command = "Start execution";
    sendTLV(sock, TYPE_COMMAND, command.data(), command.size());

    // === Очікування відповіді ===
    uint8_t type;
    std::vector<char> value;

    while (receiveTLV(sock, type, value)) {
        if (type == TYPE_COMMAND) {
            std::string msg(value.begin(), value.end());
            std::cout << "Server: " << msg << std::endl;
        } else if (type == TYPE_MATRIX_DATA) {
            const int* data = reinterpret_cast<const int*>(value.data());
            std::cout << "Received mirrored matrix:\n";
            /*for (int i = 0; i < size * size; ++i) {
                std::cout << std::setw(4) << data[i];
                if ((i + 1) % size == 0) std::cout << std::endl;
            }*/
            break;
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
