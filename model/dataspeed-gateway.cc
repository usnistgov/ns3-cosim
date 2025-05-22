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

#include "dataspeed-gateway.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DataspeedGateway");

DataspeedGateway::DataspeedGateway(Ptr<Node> vehicleNode):
    Gateway(10),
    m_application(NULL),
    m_ignoreHeight(false),
    m_referencePosition(Vector(0,0,0)),
    m_referenceOrientation(Vector(0,0,0))
{
    m_mobility = vehicleNode->GetObject<ExternalMobilityModel>();
    NS_ASSERT_MSG(m_mobility, "Node passed to Dataspeed Gateway has no ExternalMobilityModel");

    for (uint32_t i = 0; i < vehicleNode->GetNApplications(); i++)
    {
        Ptr<TriggeredSendApplication> app = DynamicCast<TriggeredSendApplication>(vehicleNode->GetApplication(i));

        if (app)
        {
            NS_ASSERT_MSG(!m_application, "Node passed to Dataspeed Gateway has multiple TriggeredSendApplications");
            m_application = app;
            NS_LOG_INFO("Dataspeed Gateway found a TriggeredSendApplication - BSM will be broadcast");
        }
    }
}

void
DataspeedGateway::SetIgnoreHeight(bool value)
{
    NS_LOG_FUNCTION(this << value);
    m_ignoreHeight = value;
}

void
DataspeedGateway::SetReferencePosition(Vector position)
{
    NS_LOG_FUNCTION(this << position);
    m_referencePosition = position;
}

void
DataspeedGateway::SetReferenceOrientation(Vector orientation)
{
    NS_LOG_FUNCTION(this << orientation);
    m_referenceOrientation = orientation;
}

void
DataspeedGateway::DoInitialize(const std::vector<std::string> & data)
{
    DoUpdate(data);
}

void
DataspeedGateway::DoUpdate(const std::vector<std::string> & data)
{
    NS_LOG_FUNCTION(this << data);

    Vector position(std::stoi(data[0]), std::stoi(data[1]), std::stoi(data[2]));
    Vector orientation(std::stoi(data[3]), std::stoi(data[4]), std::stoi(data[5]));
    Vector linearTwist(std::stoi(data[6]), std::stoi(data[7]), std::stoi(data[8]));
    double brake_torque = std::stoi(data[9]);

    position = position - m_referencePosition;
    m_mobility->SetPosition(position);
    m_mobility->SetVelocity(linearTwist);

    orientation = orientation - m_referenceOrientation;
    // TODO: use orientation for visualization

    if (m_application)
    {
        if (!m_isBraking && brake_torque > 0)
        {
            NS_LOG_INFO("DETECTED BRAKING - Starting BSM Transmission");
            m_application->Send(10);
            m_isBraking = true;
        }
        else if (m_isBraking && brake_torque == 0)
        {
            m_isBraking = false;
        }
    }
}

} // namespace ns3
