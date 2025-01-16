#ifndef EXTERNAL_MOBILITY_MODEL_H
#define EXTERNAL_MOBILITY_MODEL_H

#include "ns3/mobility-model.h"

namespace ns3
{

class ExternalMobilityModel : public MobilityModel
{
    public:
        static TypeId GetTypeId();

        ExternalMobilityModel();

        ~ExternalMobilityModel() override;

        void SetVelocity(const Vector& velocity);
    private:
        void DoSetPosition(const Vector& position) override;

        Vector DoGetPosition() const override;

        Vector DoGetVelocity() const override;

        Vector m_position;
        Vector m_velocity;
};

} // namespace ns3

#endif /* EXTERNAL_MOBILITY_MODEL_H */
