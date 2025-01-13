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
    double carAPosX = 20;
    double carAAccelX = 0;
    double carBPosX = 40;
    double carBAccelX = 0;
    double carCPosX = 60; 
    double carCAccelX = 0; 
    double braking = 0;
    double relativeTime = 0.0;
    int timeInterval = 1;
    //Condition will be a random number between 5 and 10
    int RandomBrakingCondition = rand() % 5 + 5;
    

    for(int i = 0; i<5; ++i)   { // 15 second simulation
        if (i > 0) { // can't get a response until the 1st message is sent
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
            
            //make incomingBuffer a string vector seperated by commas and parse it
            string substr = string(incomingBuffer, 0, bytesReceived);
            std::stringstream s_stream(substr);
            vector<double> tempState;
            while(s_stream.good())  { 
                std::string substr;
                getline(s_stream,substr,',');
                tempState.push_back(static_cast<double>(atof(substr.c_str())));
            }


            if(tempState[2] == 1)    {   //keep looping until client sends 1
                carBAccelX = -1;
                cout << "carB will begin to brake" << endl;
            }
            else    {
                carBAccelX = 0;
            }
            if(tempState[0] == 1)    {   //keep looping until client sends 1
                carCAccelX = -1;
                cout << "carC will begin to brake" << endl;
            }      
            else    {
                carCAccelX = 0;
            }    

        }

        //Generate a random value between 1 and 10 
        carAPosX += rand() % 10 + 1;
        carBPosX += rand() % 10 + 1;
        carCPosX += rand() % 10 + 1;

        //check if arbitrary conditions are met for braking to be true
        if(i == RandomBrakingCondition) {
            carAAccelX  = -1;
        }

        // Convert the number to string
        string numberString = to_string(relativeTime) + "," + to_string(carAPosX) +  "," + to_string(carAAccelX) +  "," + to_string(carBPosX) + "," + to_string(carBAccelX) +  "," + to_string(carCPosX) + ","  + to_string(carCAccelX);

        // Append newline character
        numberString += '\n';

        // Send message to the client
        if (send(clientSocket, numberString.c_str(), numberString.size(), 0) == -1) {
            cerr << "Failed to send data." << endl;
            break;
        }
        cout << "#" << i << ": " << numberString << endl;


        relativeTime += timeInterval;
    }

    string TerminationMSG = "-1\n";
    if (send(clientSocket, TerminationMSG.c_str(), TerminationMSG.size(), 0) == -1) {
            cerr << "Failed to send data." << endl;
    }
    cout << "Termination Message Sent: " << TerminationMSG << endl;

    shutdown(clientSocket, SHUT_RDWR);
    // Close the client socket
    close(clientSocket);

    return 0;
}
