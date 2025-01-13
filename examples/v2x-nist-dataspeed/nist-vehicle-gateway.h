#ifndef NIST_VEHICLE_GATEWAY
#define NIST_VEHICLE_GATEWAY

#include <vector>

#include "ns3/core-module.h"
#include "ns3/application-container.h"
#include "ns3/constant-acceleration-mobility-model.h"
#include "ns3/gateway.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/packet-sink.h"
#include "ns3/ptr.h"
#include "ns3/packet.h"

#include "ns3/onoff-application.h"
#include <ns3/netsimulyzer-module.h>

using namespace ns3;
class NistVehicleGateway : public ns3::Gateway {
    public:
        void initialize(const char *address, int portNum, ns3::ApplicationContainer onOffApplications, netsimulyzer::NodeConfigurationContainer configurations);

        void setTransmitOnBrake(bool isEnabled);

        void handleReceive(int nodeIndex);

    private:
        virtual void processData(string data);

        void waitForNextUpdate();

        void handleUpdate();

        void notify() override;

        void resetNodeColor(int nodeIndex);

        void sendRemoteStop();


        ns3::ApplicationContainer onOffContainer;
        netsimulyzer::NodeConfigurationContainer configurationContainer;

        ns3::Time smallestTimeUnit;
        vector<ns3::Timer> nodeTimers;

        bool remoteStop;
        bool waitingToBrake;
        bool transmitOnBrake;

        int receivedSec;
        int lastReceivedSec;
        unsigned int receivedNanosec;
        unsigned int lastReceivedNanosec;

        vector<double> receivedData;        
};

#endif // NIST_VEHICLE_GATEWAY
