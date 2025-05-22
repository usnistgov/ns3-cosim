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
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"

#include "ns3/dataspeed-gateway.h"
#include "ns3/external-mobility-model.h"
#include "ns3/triggered-send-application.h"
#include "ns3/triggered-send-helper.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    std::string serverAddress   = "127.0.0.1";
    uint16_t serverPort         = 8080;

    CommandLine cmd(__FILE__);
    cmd.AddValue("serverAddress", "IPv4 Address of the server", serverAddress);
    cmd.AddValue("serverPort", "Port Number of the server", serverPort);
    cmd.Parse(argc, argv);

    LogComponentEnable("DataspeedGateway", LOG_LEVEL_DEBUG);

    Ptr<Node> vehicle = CreateObject<Node>();

    NodeContainer nodes;
    nodes.Add(vehicle);

    Ptr<ListPositionAllocator> positionAllocator = CreateObject<ListPositionAllocator>();
    positionAllocator->Add(Vector(0, 0, 0)); // all nodes start at origin

    MobilityHelper mobilityHelper;
    mobilityHelper.SetMobilityModel("ns3::ExternalMobilityModel");
    mobilityHelper.SetPositionAllocator(positionAllocator);
    mobilityHelper.Install(nodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    const Ipv4Address broadcastAddress("192.168.1.255");
    const uint16_t applicationPort = 8000;
    TriggeredSendHelper sendHelper("ns3::UdpSocketFactory", InetSocketAddress(broadcastAddress, applicationPort));
    sendHelper.SetAttribute("PacketInterval", TimeValue(MilliSeconds(100)));
    ApplicationContainer clientApps = sendHelper.Install(nodes);
    clientApps.Start(Time(0));

    DataspeedGateway gateway(vehicle);
    gateway.SetIgnoreHeight(true);
    gateway.SetReferenceOrientation(Vector(0,0,90));
    gateway.Connect(serverAddress, serverPort);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
