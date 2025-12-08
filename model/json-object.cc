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

#include "json-object.h"
namespace ns3
{
NS_LOG_COMPONENT_DEFINE("JSONObject");

NS_OBJECT_ENSURE_REGISTERED(JSONObject);

int JSONObject::nextId = 0;

static std::mutex idMutex;

    
const std::string JSONObject::JSONOBJECT_ID = "id";
const std::string JSONObject::JSONOBJECT_TYPE = "type";
const std::string JSONObject::JSONOBJECT_ALIVE = "alive";
const std::string JSONObject::JSONOBJECT_POS_X = "pos_x";
const std::string JSONObject::JSONOBJECT_POS_Y = "pos_y";
const std::string JSONObject::JSONOBJECT_POS_Z = "pos_z";
const std::string JSONObject::JSONOBJECT_VEL_X = "vel_x";
const std::string JSONObject::JSONOBJECT_VEL_Y = "vel_y";
const std::string JSONObject::JSONOBJECT_VEL_Z = "vel_z";

JSONObject::JSONObject()
{
    idMutex.lock();
    m_id = nextId++;
    idMutex.unlock();
}

JSONObject::JSONObject(const json& data)
{
    NS_ASSERT(data.is_object());
    idMutex.lock();
    json id = data[JSONObject::JSONOBJECT_ID];
    if (!JSONObject::IsUInt(JSONObject::JSONOBJECT_ID, id))
    {
        NS_LOG_WARN(">>> JSON object has wrong uint ID attribute");
        m_id = nextId++;
    }
    else
    {
        m_id = JSONObject::GetUInt(id);
        if (m_id > nextId)
        nextId = m_id + 1;
    }
    idMutex.unlock();
    Deserialize(data);
}


TypeId JSONObject::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::JSONObject")
            .SetParent<MobilityModel>()
            .SetGroupName("V2X");
    return tid;
}

// Getters/setters

bool JSONObject::IsAlive() const
{
    return m_alive;
}
void JSONObject::SetAlive(bool alive)
{
    m_alive = alive;
}

uint JSONObject::GetId() const
{
    return m_id;
}
void JSONObject::SetId(int id)
{
    m_id = id;
}

void JSONObject::SetVelocity(const Vector& velocity)
{
    if (velocity != m_velocity)
    {
        m_velocity = velocity;
        NotifyCourseChange();
    }
}

void JSONObject::DoSetPosition(const Vector& position)
{
    if (position != m_position)
    {
        m_position = position; // this should check for a course change
    }
}

Vector JSONObject::DoGetPosition() const
{
    return m_position;
}

Vector JSONObject::DoGetVelocity() const
{
    return m_velocity;
}

// Serialization/Deserialization

void JSONObject::Deserialize(const json& obj)
{
    NS_ASSERT(obj.is_object());
    for (json::const_iterator it = obj.begin(); it != obj.end(); ++it)
    {
        std::string key = it.key();
        json value = it.value();
        if (key == JSONOBJECT_ID && IsUInt(key, value))
            m_id = GetUInt(value);
        else if (key == JSONOBJECT_ALIVE && IsBool(key, value))
            m_alive = GetBool(value);
        else if (key == JSONOBJECT_POS_X && IsFloat(key, value))
            m_position.x = GetFloat(value);
        else if (key == JSONOBJECT_POS_Y && IsFloat(key, value))
            m_position.y = GetFloat(value);
        else if (key == JSONOBJECT_POS_Z && IsFloat(key, value))
            m_position.z = GetFloat(value);
        else if (key == JSONOBJECT_VEL_X && IsFloat(key, value))
            m_velocity.x = GetFloat(value);
        else if (key == JSONOBJECT_VEL_Y && IsFloat(key, value))
            m_velocity.y = GetFloat(value);
        else if (key == JSONOBJECT_VEL_Z && IsFloat(key, value))
            m_velocity.z = GetFloat(value);
        // else, could be handled by subclasses
    }
    NotifyCourseChange();
}


void JSONObject::Serialize(json& obj) const
{
    obj.emplace(JSONOBJECT_ID, m_id);
    obj.emplace(JSONOBJECT_ALIVE, m_alive);
    obj.emplace(JSONOBJECT_POS_X, m_position.x);
    obj.emplace(JSONOBJECT_POS_Y, m_position.y);
    obj.emplace(JSONOBJECT_POS_Z, m_position.z);
    obj.emplace(JSONOBJECT_VEL_X, m_velocity.x);
    obj.emplace(JSONOBJECT_VEL_Y, m_velocity.y);
    obj.emplace(JSONOBJECT_VEL_Z, m_velocity.z);
}

bool JSONObject::IsBool(const std::string key, const json& value)
{
    if (!value.is_boolean())
    {
        NS_LOG_WARN("Attribute '" + key + "' must be a boolean");
        return false;
    }
    return true;
}
bool JSONObject::GetBool(const json& value)
{
    return value.template get<bool>();
}
bool JSONObject::IsInt(const std::string key, const json& value)
{
    if (!value.is_number_integer())
    {
        NS_LOG_WARN("Attribute '" + key + "' must be an integer");
        return false;
    }
    return true;
}
int JSONObject::GetInt(const json& value)
{
    return value.template get<int>();
}
bool JSONObject::IsUInt(const std::string key, const json& value)
{
    if (!value.is_number_unsigned())
    {
        NS_LOG_WARN("Attribute '" + key + "' must be an unsigned integer");
        return false;
    }
    return true;
}
uint JSONObject::GetUInt(const json& value)
{
    return value.template get<uint>();
}
bool JSONObject::IsFloat(const std::string key, const json& value)
{
    if (!value.is_number_float())
    {
        NS_LOG_WARN("Attribute '" + key + "' must be a float");
        return false;
    }
    return true;
}
float JSONObject::GetFloat(const json& value)
{
    return value.template get<float>();
}
bool JSONObject::IsString(const std::string key, const json& value)
{
    if (!value.is_string())
    {
        NS_LOG_WARN("Attribute '" + key + "' must be a string");
        return false;
    }
    return true;
}
std::string JSONObject::GetString(const json& value)
{
    return value.template get<std::string>();
}
}