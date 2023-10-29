#include <iostream>
#include <winsock2.h>

struct input_values {
    float x;
    float y;
    float z;
};

bool receiveData(SOCKET clientSocket, input_values & receivedData) {
    int bytesReceived = recv(clientSocket, (char*)&receivedData, sizeof(receivedData), 0);
    if (bytesReceived == SOCKET_ERROR) {
        std::cerr << "Receive failed." << std::endl;
        return false;
    }
    else if (bytesReceived == 0) {
        std::cerr << "Connection closed by server." << std::endl;
        return false;
    }
    return true;
}

int main() {
    // Initialize Winsock
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        return 1;
    }

    while (true) {
        // Create a socket
        SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Failed to create socket." << std::endl;
            WSACleanup();
            return 1;
        }

        // Connect to the server
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Use the server's IP address
        serverAddr.sin_port = htons(12344);                  // Use the same port as the server

        if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Failed to connect to the server." << std::endl;
            closesocket(clientSocket);
            WSACleanup();
            return 1;
        }

        std::cout << "Connected to the server." << std::endl;

        // Receive data continuously
        input_values receivedData;
        bool receiving = true;
        while (receiving) {
            receiving = receiveData(clientSocket, receivedData);
            if (receiving) {
                std::cout << "Received data from the server:" << std::endl;
                std::cout << "x: " << receivedData.x << std::endl;
                std::cout << "y: " << receivedData.y << std::endl;
                std::cout << "z: " << receivedData.z << std::endl;
            }
        }

        // Close the client socket (not the server socket)
        closesocket(clientSocket);
        std::cout << "Client disconnected." << std::endl;

        // Sleep for a while before attempting to reconnect
        Sleep(1000); // Sleep for 1 second, you can adjust this as needed
    }

    // Clean up
    WSACleanup();

    // Program will not reach here as it runs indefinitely
}