#include "edu_robot/sensor.hpp"

namespace eduart {
namespace robot {

Sensor::Sensor(const std::string& name, const std::string& frame_id, const std::string& reference_frame_id,
               const tf2::Transform sensor_transform)
  : _name(name)
  , _frame_id(frame_id)
  , _reference_frame_id(reference_frame_id)
  , _sensor_transform(sensor_transform)
{

}

geometry_msgs::msg::TransformStamped Sensor::getTransformMsg(const rclcpp::Time stamp) const
{
  geometry_msgs::msg::TransformStamped message;
  
  message.child_frame_id  = _frame_id;
  message.header.frame_id = _reference_frame_id;
  message.header.stamp    = stamp;
  message.transform       = tf2::toMsg(_sensor_transform);

  return message;
}

void Sensor::processDiagnostics(diagnostic_updater::DiagnosticStatusWrapper& diagnostic)
{
  // process sensor specific diagnostics in derived class
  const diagnostic::Diagnostic sensor_diagnostic = processDiagnosticsImpl();

  // convert diagnostic in ROS diagnostic message
  diagnostic.summary(convert(sensor_diagnostic.level()), sensor_diagnostic.summery());

  for (const auto& entry : sensor_diagnostic.entries()) {
    diagnostic.add(entry.first, entry.second);
  }
}

} // end namespace eduart
} // end namespace robot
