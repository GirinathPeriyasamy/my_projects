#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fstream>
#include <thread>

struct SensorDataPacket {
    float accelerometerX;
    float accelerometerY;
    float accelerometerZ;
    float roll;
    float pitch;
    float yaw;
};

void printSensorData(const SensorDataPacket& sensorData, std::ofstream& outputFile) {
    std::cout << "Received Sensor Data: " << std::endl;
    std::cout << "Accelerometer X: " << sensorData.accelerometerX << std::endl;
    std::cout << "Accelerometer Y: " << sensorData.accelerometerY << std::endl;
    std::cout << "Accelerometer Z: " << sensorData.accelerometerZ << std::endl;
    std::cout << "Roll: " << sensorData.roll << std::endl;
    std::cout << "Pitch: " << sensorData.pitch << std::endl;
    std::cout << "Yaw: " << sensorData.yaw << std::endl << std::endl;

    // Write to the output file
    outputFile << "Received Sensor Data: " << std::endl;
    outputFile << "Accelerometer X: " << sensorData.accelerometerX << std::endl;
    outputFile << "Accelerometer Y: " << sensorData.accelerometerY << std::endl;
    outputFile << "Accelerometer Z: " << sensorData.accelerometerZ << std::endl;
    outputFile << "Roll: " << sensorData.roll << std::endl;
    outputFile << "Pitch: " << sensorData.pitch << std::endl;
    outputFile << "Yaw: " << sensorData.yaw << std::endl << std::endl;
}

void fetchDataAndSaveToFile() {
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Server IP address
    serverAddr.sin_port = htons(12343);  // Server port, should match the LRU2 server

    while (true) {
        SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Failed to create socket." << std::endl;
            WSACleanup();
            return;
        }

        if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Failed to connect to the server." << std::endl;
            closesocket(clientSocket);
            WSACleanup();
            return;
        }
        while (true) {
            // Receive the output file name
            char outputFileName[64];
            int bytesReceived = recv(clientSocket, outputFileName, sizeof(outputFileName), 0);
            if (bytesReceived != sizeof(outputFileName)) {
                std::cerr << "Failed to receive the output file name." << std::endl;
                closesocket(clientSocket);
                WSACleanup();
                return;
            }

            // Open the output file for writing in append mode
            std::ofstream outputFile(outputFileName, std::ios::app);
            if (!outputFile.is_open()) {
                std::cerr << "Failed to open the output file for writing." << std::endl;
                closesocket(clientSocket);
                WSACleanup();
                return;
            }

            // Receive and write the SensorDataPacket values to the file
            SensorDataPacket sensorData;
            bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&sensorData), sizeof(sensorData), 0);
            if (bytesReceived == sizeof(sensorData)) {
                printSensorData(sensorData, outputFile);
                std::cout << "Data received and written to the file." << std::endl;
            } else {
                std::cerr << "Failed to receive SensorDataPacket values." << std::endl;
            }

            // Close the file
            outputFile.close();
        }

        closesocket(clientSocket);

        // Sleep for a while before fetching data again
        //std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}


int main() {
    while (true) {
        // Create a thread to fetch and process data
        std::thread dataThread(fetchDataAndSaveToFile);

        // Join the thread to keep the program running
        dataThread.join();

        //std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "Attempting to reconnect..." << std::endl;
    }

    return 0;
}
