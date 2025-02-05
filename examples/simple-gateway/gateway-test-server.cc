/*
 * NIST-developed software is provided by NIST as a public service. You may use,
 * copy, and distribute copies of the software in any medium, provided that you
 * keep intact this entire notice. You may improve, modify, and create
 * derivative works of the software or any portion of the software, and you may
 * copy and distribute such modifications or works. Modified works should carry
 * a notice stating that you changed the software and should note the date and
 * nature of any such change. Please explicitly acknowledge the National
 * Institute of Standards and Technology as the source of the software. 
 *
 * NIST-developed software is expressly provided "AS IS." NIST MAKES NO WARRANTY
 * OF ANY KIND, EXPRESS, IMPLIED, IN FACT, OR ARISING BY OPERATION OF LAW,
 * INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTY OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, NON-INFRINGEMENT, AND DATA ACCURACY. NIST
 * NEITHER REPRESENTS NOR WARRANTS THAT THE OPERATION OF THE SOFTWARE WILL BE
 * UNINTERRUPTED OR ERROR-FREE, OR THAT ANY DEFECTS WILL BE CORRECTED. NIST DOES
 * NOT WARRANT OR MAKE ANY REPRESENTATIONS REGARDING THE USE OF THE SOFTWARE OR
 * THE RESULTS THEREOF, INCLUDING BUT NOT LIMITED TO THE CORRECTNESS, ACCURACY,
 * RELIABILITY, OR USEFULNESS OF THE SOFTWARE.
 * 
 * You are solely responsible for determining the appropriateness of using and
 * distributing the software and you assume all risks associated with its use,
 * including but not limited to the risks and costs of program errors,
 * compliance with applicable laws, damage to or loss of data, programs or
 * equipment, and the unavailability or interruption of operation. This software 
 * is not intended to be used in any situation where a failure could cause risk
 * of injury or damage to property. The software developed by NIST employees is
 * not subject to copyright protection within the United States.
 *
 * Author: Thomas Roth <thomas.roth@nist.gov>
*/

#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <thread>
#include <vector>
#include <sstream>

#include "ns3/core-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleGatewayServer");

int
main(int argc, char* argv[])
{
    bool enableLogging      = true;
    uint64_t timeStart      = 0;
    uint32_t timeDelta      = 1;
    uint16_t positionDeltaX = 10;
    uint16_t serverPort     = 1111;

    CommandLine cmd(__FILE__);
    cmd.AddValue("logging", "Enable/disable detailed output logs (default=true)", enableLogging);
    cmd.AddValue("timeStart", "Starting simulation time in seconds", timeStart);
    cmd.AddValue("timeDelta", "Step size of simulation time in seconds", timeDelta);
    cmd.AddValue("positionDeltaX", "Maximum increase per time step to the x-coordinate of each node", positionDeltaX);
    cmd.AddValue("serverPort", "Port number for the UDP Server", serverPort);
    cmd.Parse(argc, argv);

    if (enableLogging)
    {
        LogComponentEnable("SimpleGatewayServer", LOG_LEVEL_ALL);
    }

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        NS_LOG_ERROR("Failed to create the socket");
        return 1;
    }

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    int reuse = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
    {
        NS_LOG_ERROR("Failed to set socket options");
        return 1;
    }

    // Bind the socket to the server address
    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        NS_LOG_ERROR("Failed to bind socket");
        return 1;
    }

    if (listen(serverSocket, 1) == -1)
    {
        NS_LOG_ERROR("Failed to listen for connections");
        return 1;
    }

    int clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket == -1)
    {
        NS_LOG_ERROR("Failed to accept connection");
        return 1;
    }
    
    std::string message = std::to_string(timeStart) + " 0\r\n\r\n";
    if (send(clientSocket, message.c_str(), message.size(), 0) == -1)
    {
        NS_LOG_ERROR("Failed to send message");
    }

    const size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];

    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
    if (bytesReceived == -1)
    {
        // error
    }

    int numberOfNodes = std::atoi(buffer); // check errors
    NS_LOG_INFO("Configured to send data for " << numberOfNodes << " nodes");

/*
    // Send numbers to the client
    double carAPosX = 20;
    double carAAccelX = 0;
    double carBPosX = 40;
    double carBAccelX = 0;
    double carCPosX = 60; 
    double carCAccelX = 0; 
    double braking = 0;
    int relativeTime = 0;
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
        string numberString = to_string(relativeTime) + " 0\r\n" + to_string(carAPosX) +  " " + to_string(carAAccelX) +  "," + to_string(carBPosX) + "," + to_string(carBAccelX) +  "," + to_string(carCPosX) + ","  + to_string(carCAccelX);

        // Append newline character
        numberString += "\r\n\r\n";

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
*/

    close(clientSocket);
    close(serverSocket);

    return 0;
}
