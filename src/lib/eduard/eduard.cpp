#include "edu_robot/eduard/eduard_hardware_component_factory.hpp"
#include <cstddef>
#include <edu_robot/eduard/eduard.hpp>
#include <edu_robot/hardware_component_interface.hpp>
#include <edu_robot/motor_controller.hpp>
#include <edu_robot/robot.hpp>
#include <edu_robot/range_sensor.hpp>
#include <edu_robot/imu_sensor.hpp>

#include <memory>
#include <rclcpp/logging.hpp>
#include <stdexcept>
#include <string>
#include <tf2/LinearMath/Transform.h>

namespace eduart {
namespace robot {
namespace eduard {

static Eduard::Parameter get_robot_ros_parameter(rclcpp::Node& ros_node)
{
  Eduard::Parameter parameter;

  // Declaring of Parameters
  ros_node.declare_parameter<std::string>("tf_footprint_frame", parameter.tf_footprint_frame);
  ros_node.declare_parameter<std::string>("kinematic", parameter.kinematic);

  // Reading Parameters
  parameter.tf_footprint_frame = ros_node.get_parameter("tf_footprint_frame").as_string();
  const std::string kinematic = ros_node.get_parameter("kinematic").as_string();

  if (kinematic == KINEMATIC::SKID || kinematic == KINEMATIC::MECANUM) {
    parameter.kinematic = kinematic;
  }
  else {
    // keep default kinematic
    RCLCPP_WARN(ros_node.get_logger(), "Via parameter given kinematic is not supported: given = %s", kinematic.c_str());
    RCLCPP_WARN(ros_node.get_logger(), "Fallback to default kinematic \"%s\".", parameter.kinematic.c_str());
  }

  return parameter;
}

Eduard::Eduard(const std::string& robot_name, std::unique_ptr<RobotHardwareInterface> hardware_interface)
  : robot::Robot(robot_name, std::move(hardware_interface))
  , _parameter(get_robot_ros_parameter(*this))
{ }

void Eduard::initialize(EduardHardwareComponentFactory& factory)
{
  // Lightings
  registerLighting(std::make_shared<robot::Lighting>(
    "head",
    COLOR::DEFAULT::HEAD,
    1.0f,
    factory.lightingHardware().at("head")
  ));
  registerLighting(std::make_shared<robot::Lighting>(
    "right_side",
    COLOR::DEFAULT::HEAD,
    1.0f,
    factory.lightingHardware().at("right_side")
  ));
  registerLighting(std::make_shared<robot::Lighting>(
    "left_side",
    COLOR::DEFAULT::BACK,
    1.0f,
    factory.lightingHardware().at("left_side")
  ));
  registerLighting(std::make_shared<robot::Lighting>(
    "back",
    COLOR::DEFAULT::BACK,
    1.0f,
    factory.lightingHardware().at("back")
  ));

  // Use all representation to set a initial light.
  auto lighting_all = std::make_shared<robot::Lighting>(
    "all",
    COLOR::DEFAULT::HEAD,
    1.0f,
    factory.lightingHardware().at("all")
  );
  lighting_all->setColor(COLOR::DEFAULT::BACK, Lighting::Mode::RUNNING);
  registerLighting(lighting_all);


  // Motor Controllers
  constexpr robot::MotorController::Parameter motor_controller_default_parameter{ };
  constexpr std::array<const char*, 4> motor_controller_name = {
    "motor_a", "motor_b", "motor_c", "motor_d" };
  constexpr std::array<const char*, 4> motor_controller_joint_name = {
    "base_to_wheel_rear_right", "base_to_wheel_front_right", "base_to_wheel_rear_left", "base_to_wheel_front_left" };

  for (std::size_t i = 0; i < motor_controller_name.size(); ++i) {
    registerMotorController(std::make_shared<robot::MotorController>(
      motor_controller_name[i],
      i,
      motor_controller_default_parameter,
      motor_controller_joint_name[i],
      *this,
      factory.motorControllerHardware().at(motor_controller_name[i]),
      factory.motorSensorHardware().at(motor_controller_name[i])
    ));
  }


  // Range Sensors
  constexpr std::array<const char*, 4> range_sensor_name = {
    "range/front/left", "range/front/right", "range/rear/left", "range/rear/right" };
  const std::array<tf2::Transform, 4> range_sensor_pose = {
    tf2::Transform(tf2::Quaternion(0.0, 0.0, 0.0, 1.0), tf2::Vector3( 0.17,  0.063, 0.045)),
    tf2::Transform(tf2::Quaternion(0.0, 0.0, 0.0, 1.0), tf2::Vector3( 0.17, -0.063, 0.045)),
    tf2::Transform(tf2::Quaternion(0.0, 0.0, 1.0, 0.0), tf2::Vector3(-0.17,  0.063, 0.050)),
    tf2::Transform(tf2::Quaternion(0.0, 0.0, 1.0, 0.0), tf2::Vector3(-0.17, -0.063, 0.050))
  };
  constexpr eduart::robot::RangeSensor::Parameter range_sensor_parameter{ 10.0 * M_PI / 180.0, 0.01, 5.0 };

  for (std::size_t i = 0; i < range_sensor_name.size(); ++i) {
    auto range_sensor = std::make_shared<robot::RangeSensor>(
      range_sensor_name[i],
      /*get_effective_namespace() + "/" + */range_sensor_name[i],
      Robot::_parameter.tf_base_frame,
      range_sensor_pose[i],
      range_sensor_parameter,
      *this,
      factory.rangeSensorHardware().at(range_sensor_name[i])
    );
    registerSensor(range_sensor);
    range_sensor->registerComponentInput(_collision_avoidance_component);
  }

  // IMU Sensor
  auto imu_sensor = std::make_shared<robot::ImuSensor>(
    "imu",
    /*get_effective_namespace() + "/*/"imu/base",
    /*get_effective_namespace() + "/*/_parameter.tf_footprint_frame,
    tf2::Transform(tf2::Quaternion(0.0, 0.0, 0.0, 1.0), tf2::Vector3(0.0, 0.0, 0.1)),
    ImuSensor::Parameter{ false, Robot::_parameter.tf_base_frame },
    getTfBroadcaster(),
    *this,
    factory.imuSensorHardware().at("imu")
  );
  registerSensor(imu_sensor);

  // Set Up Drive Kinematic
  // \todo make it configurable
  // \todo handle Mecanum kinematic, too
  constexpr float l_y = 0.32f;
  constexpr float l_x = 0.25f;
  constexpr float l_squared = l_x * l_x + l_y * l_y;
  constexpr float wheel_diameter = 0.17f;

  if (_parameter.kinematic == KINEMATIC::SKID) {
    _kinematic_matrix.resize(4, 3);
    _kinematic_matrix << -1.0f, 0.0f, -l_squared / (2.0f * l_y),
                         -1.0f, 0.0f, -l_squared / (2.0f * l_y),
                          1.0f, 0.0f, -l_squared / (2.0f * l_y),
                          1.0f, 0.0f, -l_squared / (2.0f * l_y);
    _kinematic_matrix *= 1.0f / wheel_diameter;
  }
  else if (_parameter.kinematic == KINEMATIC::MECANUM) {
    _kinematic_matrix.resize(4, 3);
    _kinematic_matrix << -1.0f,  1.0f, -(l_x + l_y) * 0.5f,
                         -1.0f, -1.0f, -(l_x + l_y) * 0.5f,
                          1.0f,  1.0f, -(l_x + l_y) * 0.5f,
                          1.0f, -1.0f, -(l_x + l_y) * 0.5f;
    _kinematic_matrix *= 1.0f / wheel_diameter;    
  }
  else {
    throw std::invalid_argument("Eduard: given kinematic is not supported.");
  }
}

Eduard::~Eduard()
{

}

} // end namespace eduard
} // end namespace robot
} // end namespace eduart
