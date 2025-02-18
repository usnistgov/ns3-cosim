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

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"

#include "ns3/external-mobility-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ExternalMobilityExample");

void
UpdateMobility(NodeContainer nodes, const Vector & positionDelta, const Vector & velocityDelta, Time timeDelta)
{
    for (NodeContainer::Iterator it = nodes.Begin(); it != nodes.End(); it++)
    {
        Ptr<ExternalMobilityModel> mobility = (*it)->GetObject<ExternalMobilityModel>();
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
    LogComponentEnable("ExternalMobilityExample", LOG_LEVEL_INFO);

    NodeContainer nodesA; // nodes with mobility updates every 2 seconds 
    nodesA.Create(1);

    NodeContainer nodesB; // nodes with mobility updates every 1 second
    nodesB.Create(1);

    NodeContainer allNodes;
    allNodes.Add(nodesA);
    allNodes.Add(nodesB);

    Ptr<ListPositionAllocator> positionAllocator = CreateObject<ListPositionAllocator>();
    positionAllocator->Add(Vector(0, 0, 0)); // all nodes start at origin

    MobilityHelper mobilityHelper;
    mobilityHelper.SetMobilityModel("ns3::ExternalMobilityModel");
    mobilityHelper.SetPositionAllocator(positionAllocator);
    mobilityHelper.Install(allNodes); // all nodes use the ExternalMobilityModel

    for (NodeContainer::Iterator it = allNodes.Begin(); it != allNodes.End(); it++)
    {
        // call ReportMobility whenever there is a CourseChange event
        Ptr<ExternalMobilityModel> mobility = (*it)->GetObject<ExternalMobilityModel>();
        mobility->TraceConnectWithoutContext("CourseChange", MakeBoundCallback(&ReportMobility));
    }

    // schedule the recursive UpdateMobility function for both sets of nodes
    Simulator::Schedule(Seconds(2), &UpdateMobility, nodesA, Vector(1, 0, 0), Vector(1, 0, 0), Seconds(2));
    Simulator::Schedule(Seconds(1), &UpdateMobility, nodesB, Vector(0, 0, 1), Vector(0, 0, 1), Seconds(1));

    Simulator::Stop(Seconds(10)); // prevent infinite recursion of UpdateMobility
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
