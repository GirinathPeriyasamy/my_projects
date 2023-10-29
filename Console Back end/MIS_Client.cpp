#include <iostream>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

struct MIS_input
{
    char input_file[64];
    char output_file[64];
    int time_interval;
};

class MISClient {
public:
    MISClient() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed." << std::endl;
            return;
        }

        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Socket creation failed." << std::endl;
            WSACleanup();
            return;
        }

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(PORT);
        serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    }

    void Connect() {
        if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Connection failed." << std::endl;
            closesocket(clientSocket);
            WSACleanup();
            return;
        }

        std::cout << "MIS Connected to server." << std::endl;
        SendAndReceive();
    }

private:
    void SendAndReceive()
    {
        MIS_input inputData; // Create an instance of MIS_input struct

        // Get input from the user
        std::cout << "Enter input file name: ";
        std::cin.getline(inputData.input_file, sizeof(inputData.input_file));

        std::cout << "Enter output file name: ";
        std::cin.getline(inputData.output_file, sizeof(inputData.output_file));

        std::cout << "Enter time interval in seconds: ";
        std::cin >> inputData.time_interval;
        std::cin.ignore(); // Clear the newline left in the buffer

        // Send the entire struct to the server
        send(clientSocket, reinterpret_cast<char*>(&inputData), sizeof(inputData), 0);

        char buffer[4096];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0)
        {
            buffer[bytesReceived] = '\0';
            std::cout << "Received: " << buffer << std::endl;
        }
    }


    SOCKET clientSocket;
    SOCKADDR_IN serverAddr;
    const int PORT = 12341;
    const char* SERVER_IP = "127.0.0.1";  // Change this to the server's IP address
};

int main() {
    MISClient client;
    client.Connect();
    system("PAUSE");
    return 0;
}
