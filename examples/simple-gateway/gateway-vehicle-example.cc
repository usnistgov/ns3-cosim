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

#include <sstream>

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"

#include "ns3/external-mobility-model.h"
#include "ns3/gateway.h"
#include "ns3/triggered-send-helper.h"
#include "ns3/triggered-send-application.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("GatewayVehicleExample");

void
Transmit(Ptr<Application> sendingApplication)
{
    DynamicCast<TriggeredSendApplication>(sendingApplication)->Send(1); // send 1 packet
}

void
PacketSinkTrace(std::string sinkAddress, Ptr<const Packet> packet, const Address &clientAddress)
{
    NS_LOG_INFO("Sink " << sinkAddress << " received packet at t=" << Simulator::Now().As(Time::S));
}

void
ReportMobility(Ptr<const MobilityModel> mobility)
{
    NS_LOG_INFO("At time " << Simulator::Now().As(Time::S)
        << ", Node " << mobility->GetObject<Node>()->GetId()
        << ", Position " << mobility->GetPosition()
        << ", Velocity " << mobility->GetVelocity());
}

void
DoNothing()
{
    NS_LOG_INFO("nothing is done");
}

// TODO:
//  wait for server to exist / attempt to re-connect
//  move the time sync stuff into base gateway (as 1st row of data)
//  improve send method
class GatewayImplementation : public Gateway
{
};

/*
void
GatewayImplementation::processData(std::string data)
{
    NS_LOG_INFO(data);
    std::string response = "0 0 0\r\n";
    send(clientFD, response.c_str(), response.size(), 0);
}
*/

int
main(int argc, char* argv[])
{
    bool enableLogging = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("logging", "Enable/disable detailed output logs (default=true)", enableLogging);
    cmd.Parse(argc, argv);

    if (enableLogging)
    {
        LogComponentEnable("Gateway", LOG_LEVEL_ALL);
        LogComponentEnable("GatewayVehicleExample", LOG_LEVEL_INFO);
    }

    Time::SetResolution(Time::NS);

    NodeContainer vehicles;
    vehicles.Create(3);

    Ptr<ListPositionAllocator> positionAllocator = CreateObject<ListPositionAllocator>();
    positionAllocator->Add(Vector(0, 0, 0));
    positionAllocator->Add(Vector(0, 1, 0));
    positionAllocator->Add(Vector(0, 2, 0));

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ExternalMobilityModel");
    mobility.SetPositionAllocator(positionAllocator);
    mobility.Install(vehicles);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));

    NetDeviceContainer devices;
    devices = csma.Install(vehicles);

    InternetStackHelper stack;
    stack.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(devices);

    const Ipv4Address broadcastAddress("192.168.1.255");
    const Time timeStart = Seconds(1.0);
    const Time timeStop  = Seconds(10.0);
    const uint16_t port  = 8000;

    for (uint32_t i = 0; i < vehicles.GetN(); i++)
    {
        Ptr<Node> vehicle = vehicles.Get(i);

        std::ostringstream addressStream;
        addressStream << interfaces.GetAddress(i);

        Ptr<ExternalMobilityModel> mobilityModel = vehicle->GetObject<ExternalMobilityModel>();
        mobilityModel->TraceConnectWithoutContext("CourseChange", MakeCallback(&ReportMobility));

        PacketSinkHelper serverHelper ("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer serverApps = serverHelper.Install(vehicle);
        serverApps.Get(0)->TraceConnect("Rx", addressStream.str(), MakeCallback(&PacketSinkTrace));
        serverApps.Start(timeStart);
        serverApps.Stop(timeStop);

        TriggeredSendHelper clientHelper ("ns3::UdpSocketFactory", InetSocketAddress(broadcastAddress, port));
        clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
        clientHelper.SetAttribute("PacketInterval", TimeValue(MilliSeconds(200)));

        ApplicationContainer clientApps = clientHelper.Install(vehicle);
        clientApps.Start(timeStart);
        clientApps.Stop(timeStop);
    }

    const std::string gatewayAddress = "127.0.0.1";
    const uint16_t gatewayPort = 1111;

    GatewayImplementation gateway;
    gateway.Connect(gatewayAddress, gatewayPort); // server must be running before this line (or error)

    Simulator::Schedule(Seconds(10), &DoNothing);

    Simulator::Stop(timeStop);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
