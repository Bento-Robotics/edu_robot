/**
 * Copyright EduArt Robotik GmbH 2022
 *
 * Author: Christian Wendt (christian.wendt@eduart-robotik.com)
 */
#pragma once

#include "edu_robot/ethernet_gateway/tcp/message_definition.hpp"
#include "edu_robot/ethernet_gateway/tcp/protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <edu_robot/state.hpp>
#include <edu_robot/hardware_error.hpp>

#include <algorithm>
#include <atomic>
#include <list>
#include <mutex>
#include <functional>
#include <memory>
#include <array>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <future>
#include <queue>

namespace eduart {
namespace robot {
namespace ethernet {

class IotShieldDevice;

class Request
{
  friend class EthernetCommunicator;

  enum class State {
    CREATED,
    SENT,
    RECEIVED,
    ACCEPTED,
    REJECTED,
  };

public:
  template <class Message, class... Arguments>
  inline static Request make_request(Arguments&&... args) {
    return Request(Message{}, std::forward<Arguments>(args)...);
  }
  Request(Request&&) = default;

private:
  template <class CommandByte, class... Elements>
  Request(const tcp::message::MessageFrame<CommandByte, Elements...>, const typename Elements::type&... element_value) {
    _request_message = tcp::message::MessageFrame<CommandByte, Elements...>::serialize(++_sequence_number, element_value...);
    const auto search_pattern = tcp::message::MessageFrame<CommandByte, Elements...>::makeSearchPattern();

    _response_search_pattern.resize(search_pattern.size());
    std::copy(search_pattern.begin(), search_pattern.end(), _response_search_pattern.begin());
  }

  State _state = State::CREATED;
  tcp::message::TxMessageDataBuffer _request_message;
  tcp::message::RxMessageDataBuffer _response_message;
  std::chrono::time_point<std::chrono::system_clock> _was_sent_at;
  std::chrono::time_point<std::chrono::system_clock> _was_received_at;  
  std::vector<tcp::message::Byte> _response_search_pattern;

  static std::uint8_t _sequence_number;
};

template <typename Duration>
inline void wait_for_future(std::future<Request>& future, const Duration& timeout) {
  if (future.wait_for(timeout) == std::future_status::timeout) {
    throw HardwareError(State::SHIELD_REQUEST_TIMEOUT, "TCP Request Timeout! Cancel set RPM to motor controller.");
  }
}

class EthernetCommunicator
{
  using TaskSendingUart = std::packaged_task<void()>;
  using TaskReceiving = std::packaged_task<tcp::message::RxMessageDataBuffer()>;

public:
  EthernetCommunicator(char const* const device_name, const std::uint16_t port);
  ~EthernetCommunicator();

  std::future<Request> sendRequest(Request request);
  tcp::message::RxMessageDataBuffer getRxBuffer();

private:
  void processSending(const std::chrono::milliseconds wait_time_after_sending);
  void processReceiving();
  void processing();
  void sendingData(tcp::message::Byte const *const tx_buffer, const std::size_t length);
  tcp::message::RxMessageDataBuffer receivingData();

  // Ethernet Interface
  int _socket_fd = -1;
  struct sockaddr_in _socket_address;

  // Handling Thread
  std::thread _handling_thread;
  std::mutex _mutex_data_input;
  std::atomic_bool _is_running;
  std::atomic_bool _new_incoming_requests;
  std::queue<std::pair<Request, std::promise<Request>>> _incoming_requests;
  std::queue<std::pair<std::pair<Request, std::promise<Request>>, std::future<void>>> _is_being_send;
  std::list<std::pair<Request, std::promise<Request>>> _open_request;

  // Sending Thread
  std::chrono::milliseconds _wait_time_after_sending;

  std::queue<TaskSendingUart> _sending_in_progress;
  std::mutex _mutex_sending_data;
  std::thread _tcp_sending_thread;

  // Reading Thread
  std::mutex _mutex_receiving_data;
  std::mutex _mutex_received_data_copy;
  std::thread _tcp_receiving_thread;
  std::queue<tcp::message::RxMessageDataBuffer> _rx_buffer_queue;
  tcp::message::RxMessageDataBuffer _rx_buffer_copy;
  std::atomic_bool _new_received_data;

  std::list<Request> _open_response_tasks;
};

} // end namespace iotbot
} // end namespace eduart
} // end namespace robot
