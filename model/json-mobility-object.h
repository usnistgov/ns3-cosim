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
 * Authors:
 *  Raphael Barbau
*/

#ifndef JSON_MOBILITY_OBJECT_H
#define JSON_MOBILITY_OBJECT_H

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "json-object.h"
#include <ns3/json.hpp>
#include <mutex>
#include <string>

using json = nlohmann::json;

namespace ns3
{

/**
 * A subclass of MobilityModel with JSON (de)serialization capability.
 */
class JSONMobilityObject : public JSONObject, public MobilityModel
{
  public:


    // JSON keys used for (de)serialization
    // Always keep in sync with intermediate server (in Python)
    static const std::string JSONMOBILITYOBJECT_POS_X;
    static const std::string JSONMOBILITYOBJECT_POS_Y;
    static const std::string JSONMOBILITYOBJECT_POS_Z;
    static const std::string JSONMOBILITYOBJECT_VEL_X;
    static const std::string JSONMOBILITYOBJECT_VEL_Y;
    static const std::string JSONMOBILITYOBJECT_VEL_Z;

    /**
     * @brief Create a new JSON object
     */
    JSONMobilityObject();

    JSONMobilityObject(const json& data);

    /**
     * \brief Register this type.
     * \return The Object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * @brief Set the 3-dimensional velocity.
     * @param velocity the value to set
     */
    void SetVelocity(const Vector& velocity);
    

    // Needed to resolve diamond inheritance

  protected:

    /**
     * @brief Deserialize the given JSON data into the object.
     */
    virtual void DoDeserialize(const json& data);

    /**
     * @brief Serialize the object into the given JSON data.
     */
    virtual void DoSerialize(json& data) const;

    virtual void PostDeserialize();
    
  private:

    float m_pos_x;
    float m_pos_y;
    float m_pos_z;
    float m_vel_x;
    float m_vel_y;
    float m_vel_z;

    void DoSetPosition(const Vector& position) override;

    Vector DoGetPosition() const override;

    Vector DoGetVelocity() const override;

};
}
#endif /* JSON_MOBILITY_OBJECT_H */