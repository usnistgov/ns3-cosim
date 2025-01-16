#include "ns3/external-mobility-model.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(ExternalMobilityModel);

TypeId
ExternalMobilityModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ExternalMobilityModel")
            .SetParent<MobilityModel>()
            .SetGroupName("Mobility")
            .AddConstructor<ExternalMobilityModel>();
    return tid;
}

ExternalMobilityModel::ExternalMobilityModel()
{
}

ExternalMobilityModel::~ExternalMobilityModel()
{
}

void
ExternalMobilityModel::SetVelocity(const Vector& velocity)
{
    if (velocity != m_velocity)
    {
        m_velocity = velocity;
        NotifyCourseChange();
    }
    else
    {
        m_velocity = velocity;
    }
}

void
ExternalMobilityModel::DoSetPosition(const Vector& position)
{
    m_position = position; // this should check for a course change
}

Vector
ExternalMobilityModel::DoGetPosition() const
{
    return m_position;
}

Vector
ExternalMobilityModel::DoGetVelocity() const
{
    return m_velocity;
}

} // namespace ns3
