#include "ns3/core-module.h"
#include "ns3/external-mobility-model.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include <ns3/node.h>
#include <ns3/simulator.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ExternalMobilityExample");

void
UpdateMobility(NodeContainer nodes, Vector positionDelta, Vector velocityDelta, Time timeDelta)
{
    // iterate over nodes and update

    for (NodeContainer::Iterator it = nodes.Begin(); it != nodes.End(); it++)
    {
        Ptr<Node> node = *it;
        Ptr<ExternalMobilityModel> mobility = node->GetObject<ExternalMobilityModel>();
        mobility->SetPosition(mobility->GetPosition() + positionDelta); // will not notify course change
        mobility->SetVelocity(mobility->GetVelocity() + velocityDelta); // will notify course change
    }
    Simulator::Schedule(timeDelta, &UpdateMobility, nodes, positionDelta, velocityDelta, timeDelta);
}

void
ReportMobility(Ptr<const MobilityModel> mobility)
{
    NS_LOG_INFO("At time " << Simulator::Now().As(Time::S)
        << ", Node " << mobility->GetObject<Node>()->GetId()
        << ", Position " << mobility->GetPosition()
        << ", Velocity " << mobility->GetVelocity());
}

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("ExternalMobilityExample", LOG_LEVEL_INFO);

    NodeContainer nodesA;
    nodesA.Create(1);

    NodeContainer nodesB;
    nodesB.Create(1);

    NodeContainer allNodes;
    allNodes.Add(nodesA);
    allNodes.Add(nodesB);

    Ptr<ListPositionAllocator> positionAllocator = CreateObject<ListPositionAllocator>();
    positionAllocator->Add(Vector(0, 0, 0));

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ExternalMobilityModel");
    mobility.SetPositionAllocator(positionAllocator);
    mobility.Install(allNodes);

    for (NodeContainer::Iterator it = allNodes.Begin(); it != allNodes.End(); it++)
    {
        Ptr<ExternalMobilityModel> model = (*it)->GetObject<ExternalMobilityModel>();
        model->TraceConnectWithoutContext("CourseChange", MakeBoundCallback(&ReportMobility));
    }

    Simulator::Schedule(Seconds(2), &UpdateMobility, nodesA, Vector(10, 0, 0), Vector(10, 0, 0), Seconds(2));
    Simulator::Schedule(Seconds(1), &UpdateMobility, nodesB, Vector(1, 1, 1), Vector(1, 1, 1), Seconds(1));

    Simulator::Stop(Seconds(10));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
