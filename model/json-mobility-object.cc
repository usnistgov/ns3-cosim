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

#include "json-mobility-object.h"
namespace ns3
{
NS_LOG_COMPONENT_DEFINE("JSONMobilityObject");

NS_OBJECT_ENSURE_REGISTERED(JSONMobilityObject);

static std::mutex idMutex;

const std::string JSONMobilityObject::JSONMOBILITYOBJECT_POS_X = "pos_x";
const std::string JSONMobilityObject::JSONMOBILITYOBJECT_POS_Y = "pos_y";
const std::string JSONMobilityObject::JSONMOBILITYOBJECT_POS_Z = "pos_z";
const std::string JSONMobilityObject::JSONMOBILITYOBJECT_VEL_X = "vel_x";
const std::string JSONMobilityObject::JSONMOBILITYOBJECT_VEL_Y = "vel_y";
const std::string JSONMobilityObject::JSONMOBILITYOBJECT_VEL_Z = "vel_z";

JSONMobilityObject::JSONMobilityObject()
{
}

JSONMobilityObject::JSONMobilityObject(const json& data):JSONObject(data)
{
}


TypeId JSONMobilityObject::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::JSONMobilityObject")
            .SetParent<MobilityModel>()
            .SetGroupName("V2X");
    return tid;
}

// Getters/setters

void JSONMobilityObject::SetVelocity(const Vector& velocity)
{
    if (velocity.x != m_vel_x || velocity.y != m_vel_y || velocity.z != m_vel_z)
    {
        m_vel_x = velocity.x;
        m_vel_y = velocity.y;
        m_vel_z = velocity.z;
        NotifyCourseChange();
    }
}

void JSONMobilityObject::DoSetPosition(const Vector& position)
{
    if (position.x != m_pos_x || position.y != m_pos_y || position.z != m_pos_z)
    {
        m_pos_x = position.x;
        m_pos_y = position.y;
        m_pos_z = position.z;
        // this should check for a course change
    }
}

Vector JSONMobilityObject::DoGetPosition() const
{
    return Vector(m_pos_x, m_pos_y, m_pos_z);
}

Vector JSONMobilityObject::DoGetVelocity() const
{
    return Vector(m_vel_x, m_vel_y, m_vel_z);
}

// Serialization/Deserialization

void JSONMobilityObject::DoDeserialize(const json& obj)
{
    JSONObject::DoDeserialize(obj);    
    for (json::const_iterator it = obj.begin(); it != obj.end(); ++it)
    {
        std::string key = it.key();
        json value = it.value();
        if (key == JSONMOBILITYOBJECT_POS_X && IsNumber(key, value))
            m_pos_x = GetFloat(value);
        else if (key == JSONMOBILITYOBJECT_POS_Y && IsNumber(key, value))
            m_pos_y = GetFloat(value);
        else if (key == JSONMOBILITYOBJECT_POS_Z && IsNumber(key, value))
            m_pos_z = GetFloat(value);
        else if (key == JSONMOBILITYOBJECT_VEL_X && IsNumber(key, value))
            m_vel_x = GetFloat(value);
        else if (key == JSONMOBILITYOBJECT_VEL_Y && IsNumber(key, value))
            m_vel_y = GetFloat(value);
        else if (key == JSONMOBILITYOBJECT_VEL_Z && IsNumber(key, value))
            m_vel_z = GetFloat(value);
        // else, could be handled by subclasses
    }
    NS_LOG_INFO("Position " << m_pos_x << "," << m_pos_y << "," << m_pos_z << "," << " velocity: " << m_vel_x << "," << m_vel_y << "," << m_vel_z);
}

void JSONMobilityObject::PostDeserialize()
{
    NotifyCourseChange();
}


void JSONMobilityObject::DoSerialize(json& obj) const
{
    JSONObject::DoSerialize(obj);
    obj.emplace(JSONMOBILITYOBJECT_POS_X, m_pos_x);
    obj.emplace(JSONMOBILITYOBJECT_POS_Y, m_pos_y);
    obj.emplace(JSONMOBILITYOBJECT_POS_Z, m_pos_z);
    obj.emplace(JSONMOBILITYOBJECT_VEL_X, m_vel_x);
    obj.emplace(JSONMOBILITYOBJECT_VEL_Y, m_vel_y);
    obj.emplace(JSONMOBILITYOBJECT_VEL_Z, m_vel_z);
}

}