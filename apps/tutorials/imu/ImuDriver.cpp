/*
Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
*/
#include "ImuDriver.hpp"

#include "messages/math.hpp"

#include <cmath>
#include "engine/alice/components/Failsafe.hpp"
#include "engine/core/constants.hpp"
#include "engine/gems/sight/sight.hpp"
#include "engine/gems/state/io.hpp"
#include "messages/state/differential_base.hpp"
#include "apps/tutorials/imu/gems/segway.hpp"

namespace isaac {

// Goal tolerance in meters
constexpr double kGoalTolerance = 0.1;


void ImuDriver::start() {
  // This part will be run once in the beginning of the program

  // We can tick periodically, on every message, or blocking. The tick period is set in the
  // json ping.app.json file. You can for example change the value there to change the tick
  // frequency of the node.
  // Alternatively you can also overwrite configuration with an existing configuration file like
  // in the example file fast_ping.json. Run the application like this to use an additional config
  // file:
  //   bazel run //apps/tutorials/ping -- --config apps/tutorials/ping/fast_ping.json
  goal_timestamp_ = 0;
  goal_position_ = Vector2d::Zero();
  segway_.reset(new drivers::Segway(get_ip(), get_port()));

  // segway_->start();

  tickPeriodically();
}

void ImuDriver::publishGoal(const Vector2d& position){
  // Save the timestamp to later check it against the feedback timestamp
  goal_timestamp_ = node()->clock()->timestamp();
  // Update the last goal information to avoid transmitting repeated messages
  goal_position_ = position;
  // Show the new goal on WebSight
  show("goal_timestamp", goal_timestamp_);
  show("goal_position_x", goal_position_.x());
  show("goal_position_y", goal_position_.y());
  // Publish a goal with the new goal location
  auto goal_proto = tx_goal().initProto();
  goal_proto.setStopRobot(false);
  ToProto(Pose2d::Translation(position), goal_proto.initGoal());
  // Use a goal tolerance of kGoalTolerance meters
  goal_proto.setTolerance(kGoalTolerance);
  // The goal we publish is with respect to the global "world" coordinate frame
  goal_proto.setGoalFrame("world");
  tx_goal().publish(goal_timestamp_);
}

void ImuDriver::tick() {
  // This part will be run at every tick. We are ticking periodically in this example.
  // Read desired position parameter <- ISAAC_PARAM(Vector2d, desired_position, Vector2d(9.0, 25.0));
  const Vector2d position = get_desired_position();
  // Publish goal, if there has been a location change
  if (isFirstTick() || (position - goal_position_).norm() > kGoalTolerance) {
    publishGoal(position);
  }

  auto state = segway_->getSegwayState();
  std::cout<<state.linear_accel_msp2<<std::endl;


  // Process feedback
  rx_feedback().processLatestNewMessage(
      [this](auto feedback_proto, int64_t pubtime, int64_t acqtime) {
        // Check if this feedback is associated with the last goal we transmitted
        if (goal_timestamp_ != acqtime) {
          return;
        }
        const float A_x = feedback_proto.getLinearAccelerationX();
        // std::cout<<A_x<<std::endl;
        // // Show information on WebSight
        // show("arrived", arrived ? 1.0 : 0.0);
      });
  // // Print the desired message to the console <- ISAAC_PARAM(std::string, message, "Hello World!");
  // LOG_INFO(get_message().c_str());
}

}  // namespace isaac