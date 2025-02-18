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
    // trigger the application to send 5 packets
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
    LogComponentEnable("TriggeredSendExample", LOG_LEVEL_INFO);
    LogComponentEnable("TriggeredSendApplication", LOG_LEVEL_INFO);

    // Create the network topology: N0 (UDP Client) ---> N1 (UDP Server)
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    // Allocate IPv4 Addresses from 192.168.0.1/24 
    Ipv4AddressHelper address;
    address.SetBase("192.168.0.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);
    Ipv4Address serverAddress = interfaces.GetAddress(1); // N1 is the server

    // Create a packet sink application for the server using PacketSinkTrace as the callback when packets are received
    PacketSinkHelper server("ns3::UdpSocketFactory", InetSocketAddress(serverAddress, 8000));

    ApplicationContainer serverApps = server.Install(nodes.Get(1)); // N1 is the server
    serverApps.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&PacketSinkTrace));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Create a triggered send application for the client with a 200 ms interval between sent packets
    TriggeredSendHelper client("ns3::UdpSocketFactory", InetSocketAddress(serverAddress, 8000));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("PacketInterval", TimeValue(MilliSeconds(200)));

    ApplicationContainer clientApps = client.Install(nodes.Get(0)); // N0 is the client
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(12.0));

    // Case 1: the send completes without interruption
    // For this case, packets will be sent at {3.0, 3.2, 3.4, 3.6, 3.8}
    Simulator::Schedule(Seconds(3.0), &Transmit, clientApps.Get(0));

    // Case 2: the first send is interrupted before sending all of its packets
    // For this case, packets will be sent at {4.0, 4.2, 4.4} and {4.6, 4.8, 5.0, 5.2, 5.4}
    Simulator::Schedule(Seconds(4.0), &Transmit, clientApps.Get(0));  
    Simulator::Schedule(Seconds(4.5), &Transmit, clientApps.Get(0));

    // Case 3: the first send is interrupted while sending its final packet
    // For this case, packets will be sent at {6.0, 6.2, 6.4, 6.6, 6.8} and {7.0, 7.2, 7.4, 7.6, 7.8}
    Simulator::Schedule(Seconds(6.0), &Transmit, clientApps.Get(0));
    Simulator::Schedule(Seconds(6.9), &Transmit, clientApps.Get(0));

    // Case 4: the first send is interrupted at the exact time its next packet should be sent
    // For this case, packets will be sent at {8.0, 8.2, 8.4} and {8.6, 8.8, 9.0, 9.2, 9.4}
    Simulator::Schedule(Seconds(8.0), &Transmit, clientApps.Get(0));
    Simulator::Schedule(Seconds(8.4), &Transmit, clientApps.Get(0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
