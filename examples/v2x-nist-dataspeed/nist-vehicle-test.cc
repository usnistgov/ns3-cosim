#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <thread>
#include <vector>
#include <sstream>

using namespace std;

int main() {
    cout << "Running Gateway Test Server..." << endl;

    // Create a socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        cerr << "Failed to create socket." << endl;
        return 1;
    }

    // Prepare the server address
    struct sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(1111);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    int reuse = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        cerr << "Failed to set socket options." << endl;
        return 1;
    }

    // Bind the socket to the server address
    if (bind(serverSocket, reinterpret_cast<struct sockaddr*>(&serverAddress), sizeof(serverAddress)) == -1) {
        cerr << "Failed to bind socket." << endl;
        return 1;
    }

    // Listen for incoming connections
    if (listen(serverSocket, 1) == -1) {
        cerr << "Failed to listen for connections." << endl;
        return 1;
    }

    // Accept incoming connections
    struct sockaddr_in clientAddress{};
    socklen_t clientAddressSize = sizeof(clientAddress);
    int clientSocket = accept(serverSocket, reinterpret_cast<struct sockaddr*>(&clientAddress), &clientAddressSize);
    if (clientSocket == -1) {
        cerr << "Failed to accept connection." << endl;
        return 1;
    }

    // Send numbers to the client
    double relativeTime = 0.0;
    double positionX = 0;
    double positionY = 0;
    double positionZ = 0;

    int timeInterval = 1;
    
    cout << "Initialized" << endl;

    for(int i = 0; i < 15; i++)   { // 15 second simulation
        //Generate a random value between 1 and 10 
        positionX += rand() % 10 + 1;
        positionY += rand() % 10 + 1;
        positionZ += rand() % 10 + 1;

        if (positionZ < 0) { // we don't like floating
            positionZ = 0;
        }

        double braking_percent = 0;
        if (rand() % 100 > 60) {
            braking_percent = 50;
        }

        // Convert the number to string
        string numberString;
        numberString += to_string(relativeTime) + ", 0.0\n";
        numberString += to_string(positionX) + "," + to_string(positionY) +  "," + to_string(positionZ) +  "," + to_string(braking_percent);
        numberString += "\r\n";

        // Send message to the client
        if (send(clientSocket, numberString.c_str(), numberString.size(), 0) == -1) {
            cerr << "Failed to send data." << endl;
            break;
        }
        cout << numberString << endl;

        char incomingBuffer[1024];
        cout << "Waiting for data from client..." << endl;
            
        int bytesReceived = recv(clientSocket, incomingBuffer, sizeof(incomingBuffer), 0);
        if (bytesReceived == -1) {
            cerr << "Failed to read data from socket." << endl;
            break;
        }
        if (bytesReceived == 0) {
            cout << "No more data" << endl;
            break;
        }
        
        cout << "Recieved: " << string(incomingBuffer, 0, bytesReceived) << endl;

        relativeTime += timeInterval;
    }

    string TerminationMSG = "-1\r\n";
    if (send(clientSocket, TerminationMSG.c_str(), TerminationMSG.size(), 0) == -1) {
            cerr << "Failed to send data." << endl;
    }
    cout << "Termination Message Sent: " << TerminationMSG << endl;

    shutdown(clientSocket, SHUT_RDWR);
    // Close the client socket
    close(clientSocket);

    return 0;
}
