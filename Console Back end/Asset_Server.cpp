#include <iostream>
#include <winsock2.h>
#include <windows.h>
#include <random>
#include <thread>
#include <mutex>

HANDLE event1 = CreateEvent(NULL, TRUE, FALSE, NULL);
HANDLE event2 = CreateEvent(NULL, TRUE, FALSE, NULL);
HANDLE event3 = CreateEvent(NULL, TRUE, FALSE, NULL);
HANDLE event4 = CreateEvent(NULL, TRUE, FALSE, NULL);

struct mis_input {
    char input_file[64];
    char output_file[64];
    int time_interval;
};

struct SensorDataPacket {
    float accelerometerX;
    float accelerometerY;
    float accelerometerZ;
    float roll;
    float pitch;
    float yaw;
};

// Define a structure for shared memory
struct SharedMemory {
    mis_input data;
    SensorDataPacket LRU1_data;
    bool newDataAvailable;
    bool lru1_DataAvailable;
    bool lru2_DataAvailable;
};

std::mutex sharedMemoryMutex;

void handle_mis(SharedMemory* sharedMemory) {
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "MIS: Failed to initialize Winsock." << std::endl;
        return;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "MIS: Failed to create socket." << std::endl;
        WSACleanup();
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(12341);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "MIS: Bind failed." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "MIS: Listen failed." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    std::cout << "MIS: Listening for incoming connections..." << std::endl;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "MIS: Accept failed." << std::endl;
            continue;
        }

        std::cout << "MIS: Client connected." << std::endl;

        mis_input receivedData;
        int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&receivedData), sizeof(receivedData), 0);
        if (bytesReceived == sizeof(receivedData)) {
            std::cout << "MIS: Received data from MIS client:" << std::endl;
            std::cout << "Input File: " << receivedData.input_file << std::endl;
            std::cout << "Output File: " << receivedData.output_file << std::endl;
            std::cout << "Time Interval: " << receivedData.time_interval << " seconds" << std::endl;

            std::lock_guard<std::mutex> lock(sharedMemoryMutex);
            sharedMemory->data = receivedData;
            sharedMemory->newDataAvailable = true;
            SetEvent(event1);
        }
        else {
            std::cerr << "MIS: Error receiving data from client." << std::endl;
        }

        closesocket(clientSocket);
        std::cout << "MIS: Client disconnected." << std::endl;
    }

    closesocket(serverSocket);
    WSACleanup();
}

void handle_lru1(SharedMemory* sharedMemory) {
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "LRU1: Failed to initialize Winsock." << std::endl;
        return;
    }

    SOCKET lru1ClientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (lru1ClientSocket == INVALID_SOCKET) {
        std::cerr << "LRU1: Failed to create socket." << std::endl;
        WSACleanup();
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(12342);

    if (bind(lru1ClientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "LRU1: Bind failed." << std::endl;
        WSACleanup();
        return;
    }

    if (listen(lru1ClientSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "LRU1: Listen failed." << std::endl;
        WSACleanup();
        return;
    }

    std::cout << "ASSET server is waiting for LRU1 to connect..." << std::endl;

    SOCKET clientSocket;
    while (true) {
        clientSocket = accept(lru1ClientSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "LRU1: Accept failed." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        break;
    }

    std::cout << "LRU1: Client connected to ASSET server." << std::endl;
    WaitForSingleObject(event1, INFINITE);
    int packetCount = 0;

    while (packetCount < 50) {
        if (sharedMemory->newDataAvailable) {
            std::cout << "LRU1: Received data from MIS:" << std::endl;
            std::cout << "Input File: " << sharedMemory->data.input_file << std::endl;

            std::lock_guard<std::mutex> lock(sharedMemoryMutex);
            int bytesSent = send(clientSocket, reinterpret_cast<char*>(&sharedMemory->data.input_file), sizeof(sharedMemory->data.input_file), 0);
            if (bytesSent == sizeof(sharedMemory->data.input_file)) {
                std::cout << "LRU1: Sent data to LRU1 client." << std::endl;
            }
            else {
                std::cerr << "LRU1: Error sending data to LRU1 client." << std::endl;
            }

            sharedMemory->newDataAvailable = false;
        }

        SensorDataPacket receivedSensorData;
        int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(&receivedSensorData), sizeof(receivedSensorData), 0);
        if (bytesReceived == sizeof(receivedSensorData)) {
            std::cout << "LRU1: Received sensor data from LRU1 client:" << std::endl;
            std::lock_guard<std::mutex> lock(sharedMemoryMutex);
            sharedMemory->LRU1_data = receivedSensorData;
            sharedMemory->lru1_DataAvailable = true;
            SetEvent(event2);
        }
        else {
            std::cerr << "LRU1: Error receiving sensor data from LRU1 client." << std::endl;
        }

        packetCount++;
        
        std::this_thread::sleep_for(std::chrono::seconds(sharedMemory->data.time_interval));
    }

    closesocket(clientSocket);
    
    std::cout << "LRU1: Client disconnected from ASSET server." << std::endl;

    closesocket(lru1ClientSocket);
    WSACleanup();
}

void handle_LRU2(SharedMemory* sharedMemory) {
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "LRU2: Failed to initialize Winsock." << std::endl;
        return;
    }

    SOCKET LRU2ClientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (LRU2ClientSocket == INVALID_SOCKET) {
        std::cerr << "LRU2: Failed to create socket." << std::endl;
        WSACleanup();
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(12343);

    if (bind(LRU2ClientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) 
        {
        std::cerr << "LRU2: Bind failed." << std::endl;
        WSACleanup();
        return;
        }
    
    if (listen(LRU2ClientSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "LRU2: Listen failed." << std::endl;
        WSACleanup();
        return;
        }
    
    std::cout << "ASSET server is waiting for LRU2 to connect..." << std::endl;

    while (true) {
        SOCKET clientSocket = accept(LRU2ClientSocket, NULL, NULL);

        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "LRU2: Accept failed." << std::endl;
            closesocket(LRU2ClientSocket);
            WSACleanup();
            return;
        }
        WaitForSingleObject(event2, INFINITE);
        std::cout << "LRU2: Client connected to ASSET server." << std::endl;

        int packetCount = 0;
        while (packetCount < 50) {
            {
                std::lock_guard<std::mutex> lock(sharedMemoryMutex);

                if (sharedMemory->lru1_DataAvailable) {
                    std::cout << "LRU2: Received data from LRU1 thread:" << std::endl;
                    sharedMemory->lru2_DataAvailable = true;

                    SensorDataPacket lru1Values = sharedMemory->LRU1_data;

                    int bytesSent = send(clientSocket, sharedMemory->data.output_file, sizeof(sharedMemory->data.output_file), 0);
                    if (bytesSent == sizeof(sharedMemory->data.output_file)) {
                        std::cout << "LRU2: Sent output file name to LRU2 client." << std::endl;
                    } else {
                        std::cerr << "LRU2: Error sending output file name to LRU2 client." << std::endl;
                    }

                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    bytesSent = send(clientSocket, reinterpret_cast<char*>(&lru1Values), sizeof(lru1Values), 0);
                    if (bytesSent == sizeof(lru1Values)) {
                        std::cout << "LRU2: Sent sensor data to LRU2 client." << std::endl;
                    } else {
                        std::cerr << "LRU2: Error sending sensor data to LRU2 client." << std::endl;
                    }
                }
            }

            // ... (other processing)

            packetCount++;
            SetEvent(event3);
            std::this_thread::sleep_for(std::chrono::seconds(sharedMemory->data.time_interval));
        }

        closesocket(clientSocket);
        
        std::cout << "LRU2: Client disconnected from ASSET server." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "LRU2: Attempting to reconnect..." << std::endl;
    }

    closesocket(LRU2ClientSocket);
    WSACleanup();
}

void handle_vis(SharedMemory* sharedMemory) {
    // Initialize Winsock
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "VIS: Failed to initialize Winsock" << std::endl;
        return;
    }

    // Create a socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "VIS: Failed to create socket" << std::endl;
        WSACleanup();
        return;
    }

    // Bind the socket to an IP address and port
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;  // Use any available network interface
    serverAddr.sin_port = htons(12344);       // Use port 12344

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "VIS: Bind failed." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    // Listen for incoming connections
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "VIS: Listen failed." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    std::cout << "VIS: listening for incoming connections..." << std::endl;

    while (true) {
        // Accept a client connection
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "VIS: Accept failed." << std::endl;
            closesocket(serverSocket);
            WSACleanup();
            return;
        }
        WaitForSingleObject(event3, INFINITE);
        
        std::cout << "VIS: Client for three values connected." << std::endl;

        // Sending sensor values from LRU1 or LRU2, assuming LRU1 data is available first
        SensorDataPacket dataToSend;
        if (sharedMemory->lru1_DataAvailable) {
            std::cout << "VIS: Received data from LRU1 thread." << std::endl;
            dataToSend = sharedMemory->LRU1_data;
            sharedMemory->lru1_DataAvailable = false;
        } else if (sharedMemory->lru2_DataAvailable) {
            std::cout << "VIS: Received data from LRU2 thread." << std::endl;
            dataToSend = sharedMemory->LRU1_data;  // Change to LRU2 data if needed
            sharedMemory->lru2_DataAvailable = false;
        }

        // Send the struct to the client
        if (send(clientSocket, reinterpret_cast<char*>(&dataToSend), sizeof(dataToSend), 0) == SOCKET_ERROR) {
            std::cerr << "VIS: Send failed." << std::endl;
        } else {
            std::cout << "VIS: Data with sensor values sent to the client." << std::endl;
        }
        SetEvent(event4);
        // Close the client socket (not the server socket)
        closesocket(clientSocket);
        
        std::cout << "VIS: Client disconnected." << std::endl;
    }

    // Clean up (the server will not reach this point)
    closesocket(serverSocket);
    WSACleanup();
}


void handle_csss(SharedMemory* sharedMemory) {
    // Initialize Winsock
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "CSSS: Failed to initialize Winsock." << std::endl;
        return;
    }

    // Create a socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "CSSS: Failed to create socket." << std::endl;
        WSACleanup();
        return;
    }

    // Bind the socket to an IP address and port
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;  // Use any available network interface
    serverAddr.sin_port = htons(12345);       // Use a different port (12345) for six values

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "CSSS: Bind failed." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    // Listen for incoming connections
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "CSSS: Listen failed." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    std::cout << "CSSS: listening for incoming connections..." << std::endl;

    while (true) {
        
        // Accept a client connection
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "CSSS: Accept failed." << std::endl;
            closesocket(serverSocket);
            WSACleanup();
            return;
        }

        std::cout << "CSSS: Client for six values connected." << std::endl;
        WaitForSingleObject(event4, INFINITE);

        // Sending sensor values from LRU1 or LRU2, assuming LRU1 data is available first
        SensorDataPacket dataToSend;
        /*if (sharedMemory->lru1_DataAvailable) {
            std::cout << "CSSS: Received data from LRU1 thread." << std::endl;
            dataToSend = sharedMemory->LRU1_data;
            sharedMemory->lru1_DataAvailable = false;
        } else */
        if (sharedMemory->lru2_DataAvailable) {
            std::cout << "CSSS: Received data from LRU2 thread." << std::endl;
            dataToSend = sharedMemory->LRU1_data;  // Change to LRU2 data if needed
            sharedMemory->lru2_DataAvailable = false;
        }

        // Send the struct to the client
        if (send(clientSocket, reinterpret_cast<char*>(&dataToSend), sizeof(dataToSend), 0) == SOCKET_ERROR) {
            std::cerr << "CSSS: Send failed." << std::endl;
        } else {
            std::cout << "CSSS: Data with sensor values sent to the client." << std::endl;
        }

        // Close the client socket (not the server socket)
        closesocket(clientSocket);
        std::cout << "CSSS: Client for six values disconnected." << std::endl;
    }

    // Clean up (the server will not reach this point)
    closesocket(serverSocket);
    WSACleanup();
}



int main() {

    
    // Initialize shared memory
    SharedMemory sharedMemory;
    
    std::thread misThread(handle_mis, &sharedMemory);
    
    while(true)
    {
        sharedMemory.newDataAvailable = false;
        sharedMemory.lru1_DataAvailable = false;
        sharedMemory.lru2_DataAvailable = false;
        std::thread lru1Thread(handle_lru1, &sharedMemory);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::thread LRU2Thread(handle_LRU2, &sharedMemory);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::thread visThread(handle_vis, &sharedMemory);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::thread csssThread(handle_csss, &sharedMemory);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        lru1Thread.join();
        visThread.join();
        csssThread.join();
        LRU2Thread.join();

        CloseHandle(event1);
        CloseHandle(event2);
        CloseHandle(event3);
        CloseHandle(event4);
    }
    misThread.join();
    
    return 0;
}
