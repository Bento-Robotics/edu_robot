#include "edu_robot/sensor.hpp"
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>

namespace eduart {
namespace robot {

Sensor::Sensor(const std::string& name, const std::string& frame_id, const tf2::Transform sensor_transform)
  : _name(name)
  , _frame_id(frame_id)
  , _sensor_transform(sensor_transform)
{

}

geometry_msgs::msg::TransformStamped Sensor::getTransformMsg(const rclcpp::Time stamp) const
{
  geometry_msgs::msg::TransformStamped message;
  
  message.header.frame_id = _frame_id;
  message.header.stamp    = stamp;
  message.transform       = tf2::toMsg(_sensor_transform);

  return message;
}

} // end namespace eduart
} // end namespace robot