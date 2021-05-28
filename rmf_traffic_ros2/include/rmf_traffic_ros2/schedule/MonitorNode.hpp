/*
 * Copyright (C) 2021 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#ifndef RMF_TRAFFIC_ROS2__SCHEDULE__MONITOR_NODE_HPP
#define RMF_TRAFFIC_ROS2__SCHEDULE__MONITOR_NODE_HPP

#include <rclcpp/node.hpp>

#include <functional>

namespace rmf_traffic_ros2 {
namespace schedule {

/// Make a monitor node to monitor a heartbeat and restart a node when
/// the heartbeat is lost
std::shared_ptr<rclcpp::Node> make_monitor_node(
  std::function<void(std::shared_ptr<rclcpp::Node>)> callback,
  const rclcpp::NodeOptions& options = rclcpp::NodeOptions());

} // namespace schedule
} // namespace rmf_traffic_ros2

#endif // RMF_TRAFFIC_ROS2__SCHEDULE__MONITOR_NODE_HPP
