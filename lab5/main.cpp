#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define BUFFER_SIZE 4096

void sendResponse(SOCKET clientSocket, const std::string& status, const std::string& contentType, const std::string& body) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;

    std::string responseStr = response.str();
    send(clientSocket, responseStr.c_str(), responseStr.size(), 0);
}

std::string loadFileContent(const std::string& filename) {
    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file) {
        return "";
    }
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

void handleClient(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];
    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);

    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::string request(buffer);

        std::istringstream requestStream(request);
        std::string method, path, version;
        requestStream >> method >> path >> version;

        std::cout << "Request: " << method << " " << path << " " << version << "\n";

        if (method == "GET") {
            std::string filePath;

            if (path == "/" || path == "/index.html") {
                filePath = "index.html";
            } else if (path == "/page2.html") {
                filePath = "page2.html";
            } else {
                filePath = ""; // unknown path
            }

            if (!filePath.empty()) {
                std::string content = loadFileContent(filePath);
                if (!content.empty()) {
                    sendResponse(clientSocket, "200 OK", "text/html", content);
                } else {
                    std::string notFound = "<html><body><h1>404 Not Found</h1></body></html>";
                    sendResponse(clientSocket, "404 Not Found", "text/html", notFound);
                }
            } else {
                std::string notFound = "<html><body><h1>404 Not Found</h1></body></html>";
                sendResponse(clientSocket, "404 Not Found", "text/html", notFound);
            }
        } else {
            std::string notAllowed = "<html><body><h1>405 Method Not Allowed</h1></body></html>";
            sendResponse(clientSocket, "405 Method Not Allowed", "text/html", notAllowed);
        }
    }

    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is running on port " << PORT << "...\n";

    while (true) {
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);

        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed\n";
            continue;
        }

        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach(); // Allow concurrent client handling
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
