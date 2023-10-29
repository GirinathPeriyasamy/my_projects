#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <fstream>
#include <cstdlib>
#include <ctime>

#pragma comment(lib, "ws2_32.lib")

struct LRU1Data {
    char input_file[64];
};

struct SensorDataPacket {
    float accelerometerX;
    float accelerometerY;
    float accelerometerZ;
    float roll;
    float pitch;
    float yaw;
};

void ReceiveData(SOCKET clientSocket) {
    LRU1Data receivedData;
    int bytesReceived;
    int packetCount = 0; // Counter for generated packets

    // Open the output file outside the loops
    std::ofstream outputFile;

    while (true) {
        bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&receivedData), sizeof(receivedData), 0);
        if (bytesReceived == sizeof(receivedData)) {
            std::cout << "LRU1: Received input file name from ASSET: " << receivedData.input_file << std::endl;

            // Open the output file if not already open
            if (!outputFile.is_open()) {
                outputFile.open(receivedData.input_file);
                if (!outputFile.is_open()) {
                    std::cerr << "LRU1: Failed to create or open file for writing." << std::endl;
                    continue;
                }
            }

            // Generate and send sensor data in packets
            SensorDataPacket sensorData;

            while (packetCount < 50) { // Limit to 100 packets
                sensorData.accelerometerX = (rand() / static_cast<double>(RAND_MAX)) * 2.0 - 1.0;
                sensorData.accelerometerY = (rand() / static_cast<double>(RAND_MAX)) * 2.0 - 1.0;
                sensorData.accelerometerZ = (rand() / static_cast<double>(RAND_MAX)) * 2.0 - 1.0;
                sensorData.roll = (rand() / static_cast<double>(RAND_MAX)) * 360.0 - 180.0;
                sensorData.pitch = (rand() / static_cast<double>(RAND_MAX)) * 360.0 - 180.0;
                sensorData.yaw = (rand() / static_cast<double>(RAND_MAX)) * 360.0 - 180.0;

                outputFile << "Accelerometer X: " << sensorData.accelerometerX << "\n";
                outputFile << "Accelerometer Y: " << sensorData.accelerometerY << "\n";
                outputFile << "Accelerometer Z: " << sensorData.accelerometerZ << "\n";
                outputFile << "Roll: " << sensorData.roll << "\n";
                outputFile << "Pitch: " << sensorData.pitch << "\n";
                outputFile << "Yaw: " << sensorData.yaw << "\n\n";
                outputFile.flush();

                std::cout << "Accelerometer X: " << sensorData.accelerometerX << "\n";
                std::cout << "Accelerometer Y: " << sensorData.accelerometerY << "\n";
                std::cout << "Accelerometer Z: " << sensorData.accelerometerZ << "\n";
                std::cout << "Roll: " << sensorData.roll << "\n";
                std::cout << "Pitch: " << sensorData.pitch << "\n";
                std::cout << "Yaw: " << sensorData.yaw << "\n\n";

                // Send the sensor data packet
                int bytesSent = send(clientSocket, reinterpret_cast<char*>(&sensorData), sizeof(sensorData), 0);

                if (bytesSent == sizeof(sensorData)) {
                    std::cout << "LRU1: Sent sensor data to ASSET server." << std::endl;
                }
                else {
                    std::cerr << "LRU1: Error sending sensor data to ASSET server." << std::endl;
                }

                packetCount++; // Increment the packet count

                if (packetCount >= 100) {
                    break; // Exit the inner loop after 100 packets
                }

                std::this_thread::sleep_for(std::chrono::seconds(1)); // Sleep for 5 seconds
            }

            if (packetCount >= 5) {
                break; // Exit the outer loop after 100 packets
            }
        }
        else {
            std::cerr << "LRU1: Error receiving data from ASSET server." << std::endl;
            break;
        }
    }

    // Close the output file when done
    if (outputFile.is_open()) {
        outputFile.close();
    }

    // Close the socket to disconnect from the server
    closesocket(clientSocket);
}

int main() {
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "LRU1: Failed to initialize Winsock." << std::endl;
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "LRU1: Failed to create socket." << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12342);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "LRU1: Connection to ASSET server failed." << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::thread receiveThread(ReceiveData, clientSocket);
    receiveThread.join();
    std::cout << "LRU1: LRU1 client disconnected ." << std::endl;


    closesocket(clientSocket);
    WSACleanup();
    system("PAUSE");
    return 0;
}
