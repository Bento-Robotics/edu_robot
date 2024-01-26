#include "edu_robot/hardware/igus/motor_controller_hardware.hpp"
#include "edu_robot/hardware/igus/can/message_definition.hpp"
#include "edu_robot/hardware/igus/can/protocol.hpp"

#include <edu_robot/component_error.hpp>
#include <edu_robot/rpm.hpp>
#include <edu_robot/state.hpp>

#include <edu_robot/hardware_error.hpp>

#include <memory>
#include <stdexcept>

namespace eduart {
namespace robot {
namespace igus {

using namespace std::chrono_literals;

using can::message::PROTOCOL;
using can::message::SetVelocity;
using can::message::AcknowledgedVelocity;
using can::message::SetEnableMotor;
using can::message::SetDisableMotor;

void MotorControllerHardware::processRxData(const can::message::RxMessageDataBuffer &data)
{
  if (_callback_process_measurement == nullptr || can::message::AcknowledgedVelocity::canId(data) != _can_id) {
    return;
  }
}

void MotorControllerHardware::initialize(const Motor::Parameter& parameter)
{
  // Initial Motor Controller Hardware
  if (false == parameter.isValid()) {
    throw std::invalid_argument("Given parameter are not valid. Cancel initialization of motor controller.");
  }

  // Motor Controller Parameter
}

void MotorControllerHardware::processSetValue(const std::vector<Rpm>& rpm)
{
  if (rpm.size() < 1) {
    throw std::runtime_error("Given RPM vector is too small.");
  }

  const auto stamp = std::chrono::system_clock::now();
  const auto ms = std::chrono::time_point_cast<std::chrono::milliseconds>(stamp).time_since_epoch().count() % 256;
  auto request = Request::make_request<SetVelocity>(
    _can_id,
    rpm[0],
    ms
  );

  auto future_response = _communicator->sendRequest(std::move(request));
  wait_for_future(future_response, 200ms);
  auto got = future_response.get();
  _measured_rpm[0] = AcknowledgedVelocity::rpm(got.response());

  if (_callback_process_measurement == nullptr) {
    return;
  }
  if (AcknowledgedVelocity::errorCode(got.response()) & ~PROTOCOL::ERROR::MOTOR_NOT_ENABLED) {

  }

  _callback_process_measurement(_measured_rpm, AcknowledgedVelocity::enabled(got.response()));
}

} // end namespace igus
} // end namespace eduart
} // end namespace robot
