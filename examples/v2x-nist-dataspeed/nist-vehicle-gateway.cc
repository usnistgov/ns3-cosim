#include "nist-vehicle-gateway.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NistVehicleGateway");

void NistVehicleGateway::initialize(const char *address, int portNum, ns3::ApplicationContainer onOffApplications, netsimulyzer::NodeConfigurationContainer configurations) {
    Gateway::initialize(address, portNum);

    onOffContainer = onOffApplications;
    configurationContainer = configurations;

    nodeTimers.resize(configurationContainer.GetN());
    for (int i = 0; i < nodeTimers.size(); i++) {
        nodeTimers[i].SetFunction(&NistVehicleGateway::resetNodeColor, this);
        nodeTimers[i].SetArguments(i);
    }

    remoteStop = false;
    waitingToBrake = false;
    transmitOnBrake = false;

    receivedSec = -1;
    receivedNanosec = -1;
    lastReceivedSec = -1;
    lastReceivedNanosec = -1;

    receivedData.resize(9); // {position_x, position_y, position_z, orientation_x, orientation_y, orientation_z, velocity, brake_percent, remote_stop_ms}

    configurationContainer.Get(0)->SetBaseColor(netsimulyzer::BLUE);

    smallestTimeUnit = ns3::Time::From(1, ns3::Time::GetResolution());
    Simulator::Schedule (smallestTimeUnit, &NistVehicleGateway::waitForNextUpdate, this);
}

void NistVehicleGateway::setTransmitOnBrake(bool isEnabled) {
    transmitOnBrake = isEnabled;
}

void NistVehicleGateway::handleReceive(int nodeIndex) {
    Ptr<netsimulyzer::NodeConfiguration> configuration = configurationContainer.Get(nodeIndex);

    if (nodeIndex == 0) { // NIST Vehicle
        NS_LOG_INFO ("VEHICLE RECEIVED BSM - Sending Brake Command");
        waitingToBrake = true;
        remoteStop = true;
    } else {
        configuration->SetBaseColor(netsimulyzer::GREEN);
        if (nodeTimers[nodeIndex].IsRunning()) {
            nodeTimers[nodeIndex].Cancel();
        }
        nodeTimers[nodeIndex].Schedule(Seconds(3.0));
    }
}

void NistVehicleGateway::processData(string data) {
    NS_LOG_FUNCTION ("processData @ " << Simulator::Now());

    std::stringstream dataStream(data);

    bool isFirstLine = true;
    while(dataStream.good()) {
        std::string line;
        getline(dataStream, line, '\n');

        vector<string> values;
        std::stringstream lineStream(line);
        while(lineStream.good()) {
            std::string value;
            getline(lineStream, value, ',');
            values.push_back(value);
        }

        if(isFirstLine) { // clock update
            receivedSec = stod(values[0].c_str());
            receivedNanosec = stod(values[1].c_str());
            isFirstLine = false;
            NS_LOG_INFO("RECEIVED TIME " << receivedSec << "." << receivedNanosec);
        } else { // object update
            if (values.size() != 9) {
                NS_LOG_ERROR("corrupt data format: " << line);   
            }
            for (int i = 0; i < 9; i++) {
                receivedData[i] = stod(values[i]);
            }
        }
    }
}

void NistVehicleGateway::waitForNextUpdate() {
    if (!Gateway::isServerTermination())    {
        if (receivedSec == lastReceivedSec && receivedNanosec == lastReceivedNanosec) {
            // waiting to receive a new time value from the gateway end point
            // TODO: peek at the event queue to determine if an event can be processed (maybe not possible)
            Simulator::ScheduleNow (&NistVehicleGateway::waitForNextUpdate, this);
        } else {
            if (lastReceivedSec == -1) { // occurs at NS-3 time = smallestTimeUnit
                lastReceivedSec = 0;
                lastReceivedNanosec = 0;
            }
            
            ns3::Time timeDelta = Seconds(receivedSec) - Seconds(lastReceivedSec);
            timeDelta = timeDelta + (NanoSeconds(receivedNanosec) - NanoSeconds(lastReceivedNanosec));
            
            lastReceivedSec = receivedSec;
            lastReceivedNanosec = receivedNanosec;

            NS_LOG_INFO ("advancing time from " << Simulator::Now() << " to " << Simulator::Now() + timeDelta);
            Simulator::Schedule (timeDelta, &NistVehicleGateway::handleUpdate, this);    
        }
    }
}

void NistVehicleGateway::handleUpdate() {
    NS_LOG_FUNCTION ("handleUpdate @ " << Simulator::Now());

    int remote_stop_ms = receivedData[8];
    if (remote_stop_ms >= 0) { // 0 indicates immediate stop
        NS_LOG_DEBUG("DETECTED REMOTE STOP - Scheduling BSM Transmission");
        Simulator::Schedule (MilliSeconds(remote_stop_ms), &NistVehicleGateway::sendRemoteStop, this);
    }

    double brake_torque = receivedData[7];
    if (transmitOnBrake) {
        if (!braking && brake_torque > 0) {
            NS_LOG_INFO("DETECTED BRAKING - Starting BSM Transmission");
            Ptr<OnOffApplication> tempOnOffHolder = DynamicCast<OnOffApplication>(onOffContainer.Get(0));
            if (tempOnOffHolder) {
                tempOnOffHolder->StartNow();
            }

            Ptr<netsimulyzer::NodeConfiguration> vehicleConfiguration = configurationContainer.Get(0);
            vehicleConfiguration->Transmit(Seconds(1.0), 50.0, netsimulyzer::GREEN);
            vehicleConfiguration->SetBaseColor(netsimulyzer::RED);

            braking = true;
        }
    }
    if (waitingToBrake && brake_torque > 0) {
        Ptr<netsimulyzer::NodeConfiguration> vehicleConfiguration = configurationContainer.Get(0);
        vehicleConfiguration->SetBaseColor(netsimulyzer::RED);
        waitingToBrake = false;
        braking = true;
    }
    if (braking && brake_torque == 0.0) {
        resetNodeColor(0);
        braking = false;
    }
    
    Ptr<Node> vehicleNode = onOffContainer.Get(0)->GetNode();
    Ptr<ConstantAccelerationMobilityModel> mobilityModel = vehicleNode->GetObject<ConstantAccelerationMobilityModel>();
    mobilityModel->SetPosition(Vector(receivedData[0], receivedData[1], receivedData[2]));

    Ptr<netsimulyzer::NodeConfiguration> vehicleConfiguration = configurationContainer.Get(0);
    vehicleConfiguration->SetOrientation(Vector(receivedData[3], receivedData[4], receivedData[5]));

    // Print out position
    ns3::Vector position = mobilityModel->GetPosition();
    NS_LOG_INFO("Car Position: " << position.x << ", " << position.y << ", " << position.z);

    notify(); // Only when time updated

    Simulator::ScheduleNow(&NistVehicleGateway::waitForNextUpdate, this);
}

void NistVehicleGateway::notify() {
    string continueMessage = to_string(remoteStop);
    remoteStop = false; // always reset
    
    //read packet sinks and see which car recieves request to break
    //continueMessage = "car1shouldBrake(wont happen its lead car), car2shouldBrake, car3shouldBrake"
    send(clientFD, continueMessage.c_str(), continueMessage.size(), 0);//signal to server listener is ready
}

void NistVehicleGateway::resetNodeColor(int nodeIndex) {
    Ptr<netsimulyzer::NodeConfiguration> configuration = configurationContainer.Get(nodeIndex);

    if (nodeIndex == 0) { // NIST Vehicle
        configuration->SetBaseColor(netsimulyzer::BLUE);
    } else {
        configuration->SetBaseColor(netsimulyzer::BLACK);
    }
}

void NistVehicleGateway::sendRemoteStop() {
    NS_LOG_FUNCTION ("sendRemoteStop @ " << Simulator::Now());

    Ptr<OnOffApplication> tempOnOffHolder = DynamicCast<OnOffApplication>(onOffContainer.Get(1));
    if (tempOnOffHolder) {
        tempOnOffHolder->StartNow();
    } else {
        NS_LOG_ERROR("unable to find OnOffApplication to transmit remote stop");
    }

    configurationContainer.Get(1)->Transmit(Seconds(1.0), 50.0, netsimulyzer::GREEN); // need to set node color?
}
