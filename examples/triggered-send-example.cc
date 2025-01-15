#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/triggered-send-application.h"
#include "ns3/triggered-send-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TriggeredSendExample");

void
Transmit(Ptr<Application> sendingApplication)
{
    DynamicCast<TriggeredSendApplication>(sendingApplication)->Send(5);
}

void
PacketSinkTrace(Ptr<const Packet> pkt, const Address &addr)
{
    NS_LOG_INFO("\t received at time " << Simulator::Now().As(Time::S));
}

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("TriggeredSendExample", LOG_LEVEL_INFO);
    LogComponentEnable("TriggeredSendApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    PacketSinkHelper server("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), 8000));

    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&PacketSinkTrace));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    TriggeredSendHelper client("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), 8000));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("PacketInterval", TimeValue(MilliSeconds(200)));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // uninterrupted send
    Simulator::Schedule(Seconds(3.0), &Transmit, clientApps.Get(0));

    // interrupted at the normal send interval
    Simulator::Schedule(Seconds(5.0), &Transmit, clientApps.Get(0));  
    Simulator::Schedule(Seconds(5.4), &Transmit, clientApps.Get(0));

    // interrupted in-between send intervals
    Simulator::Schedule(Seconds(7.0), &Transmit, clientApps.Get(0));
    Simulator::Schedule(Seconds(7.5), &Transmit, clientApps.Get(0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
