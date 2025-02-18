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

#ifndef EXTERNAL_MOBILITY_MODEL_H
#define EXTERNAL_MOBILITY_MODEL_H

#include "ns3/mobility-model.h"

namespace ns3
{

/**
 * A constant mobility model for when an external process (outside of ns-3) controls the node position and velocity.
 * As the external process updates the node mobility (including any and all changes to position), explicit calls to
 * MobilityModel::SetPosition and ExternalMobilityModel::SetVelocity are required to reflect the values in this model.
 *
 * Due to a limitation of the current implementation, only ExternalMobilityModel::SetVelocity can cause a CourseChange 
 * trace callback (position updates are ignored). Therefore, the recommended call order for mobility updates is to set
 * the position first and then update the velocity. This will result in at most one CourseChange callback, during which
 * both position and velocity will have consistent values.
 */
class ExternalMobilityModel : public MobilityModel
{
    public:
        /**
         * @brief Get the type ID.
         * @return the object TypeId
         */
        static TypeId GetTypeId();

        /**
         * @brief Create a mobility model with (0,0,0) position and velocity.
         */
        ExternalMobilityModel();

        ~ExternalMobilityModel() override;

        /**
         * @brief Set the 3-dimensional velocity.
         * @param velocity the value to set
         */
        void SetVelocity(const Vector& velocity);
    private:
        void DoSetPosition(const Vector& position) override;

        Vector DoGetPosition() const override;

        Vector DoGetVelocity() const override;

        Vector m_position;  //!< the 3-dimensional cartesian coordinates
        Vector m_velocity;  //!< the 3-dimensional velocity
};

} // namespace ns3

#endif /* EXTERNAL_MOBILITY_MODEL_H */
