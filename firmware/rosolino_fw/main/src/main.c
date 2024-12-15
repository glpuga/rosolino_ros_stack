
// standard headers
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// microros
#include <rcl/error_handling.h>
#include <rcl/rcl.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <uros_network_interfaces.h>

// messages
#include <geometry_msgs/msg/twist.h>
#include <nav_msgs/msg/odometry.h>

// esp headers
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef CONFIG_MICRO_ROS_ESP_XRCE_DDS_MIDDLEWARE
#include <rmw_microros/rmw_microros.h>
#endif

// application headers
#include "macros.h"

rcl_subscription_t cmd_vel_subscriber;
geometry_msgs__msg__Twist cmd_vel_msg;

rcl_publisher_t odometry_publisher;
nav_msgs__msg__Odometry odometry_msg;

void update_odometry_message(nav_msgs__msg__Odometry *msg) {
  msg->header.stamp.sec = esp_log_timestamp();
  msg->header.stamp.nanosec = 0;
  msg->header.frame_id.data = CONFIG_ROSOLINO_ODOMETRY_FRAME_ID;
  msg->child_frame_id.data = CONFIG_ROSOLINO_ODOMETRY_BASE_LINK_FRAME_ID;
  msg->pose.pose.position.x = 1.0;
  msg->pose.pose.position.y = 2.0;
  msg->pose.pose.position.z = 3.0;
  msg->pose.pose.orientation.x = 0.0;
  msg->pose.pose.orientation.y = 0.0;
  msg->pose.pose.orientation.z = 0.0;
  msg->pose.pose.orientation.w = 1.0;
}

void timer_callback(rcl_timer_t *timer, int64_t last_call_time) {
  (void)last_call_time;
  if (timer != NULL) {
    // publish odometry message
    update_odometry_message(&odometry_msg);
    RCSOFTCHECK(rcl_publish(&odometry_publisher, &odometry_msg, NULL));
  }
}

void cmd_vel_callback(const void *msgin) {
  const geometry_msgs__msg__Twist *msg =
      (const geometry_msgs__msg__Twist *)msgin;
  (void)msg;
  printf("Received cmd_vel message\n");
}

void init_task(void *arg) {
  rcl_allocator_t allocator = rcl_get_default_allocator();
  rclc_support_t support;

  // Create init_options.
  rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
  RCCHECK(rcl_init_options_init(&init_options, allocator));

#ifdef CONFIG_MICRO_ROS_ESP_XRCE_DDS_MIDDLEWARE
  rmw_init_options_t *rmw_options =
      rcl_init_options_get_rmw_init_options(&init_options);

  // Static Agent IP and port can be used instead of autodiscovery.
  RCCHECK(rmw_uros_options_set_udp_address(
      CONFIG_MICRO_ROS_AGENT_IP, CONFIG_MICRO_ROS_AGENT_PORT, rmw_options));
  // RCCHECK(rmw_uros_discover_agent(rmw_options));
#endif

  // configure the domain id
  RCCHECK(
      rcl_init_options_set_domain_id(&init_options, CONFIG_ROSOLINO_DOMAIN_ID));

  // Setup support structure.
  RCCHECK(rclc_support_init_with_options(&support, 0, NULL, &init_options,
                                         &allocator));

  // Create node.
  rcl_node_t node = rcl_get_zero_initialized_node();
  RCCHECK(
      rclc_node_init_default(&node, CONFIG_ROSOLINO_NODE_NAME, "", &support));

  // Create odometry publisher.
  RCCHECK(rclc_publisher_init_default(
      &odometry_publisher, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(nav_msgs, msg, Odometry),
      CONFIG_ROSOLINO_ODOMETRY_TOPIC_NAME));

  // Create subscriber.
  RCCHECK(rclc_subscription_init_default(
      &cmd_vel_subscriber, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
      CONFIG_ROSOLINO_CMD_VEL_TOPIC_NAME));

  // Create timer.
  rcl_timer_t timer = rcl_get_zero_initialized_timer();
  const unsigned int timer_timeout = CONFIG_ROSOLINO_ODOMETRY_INTERVAL_MS;
  RCCHECK(rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(timer_timeout),
                                  timer_callback));

  // Create executor.
  rclc_executor_t executor = rclc_executor_get_zero_initialized_executor();
  RCCHECK(rclc_executor_init(&executor, &support.context, 2, &allocator));
  unsigned int rcl_wait_timeout = 1000; // in ms
  RCCHECK(rclc_executor_set_timeout(&executor, RCL_MS_TO_NS(rcl_wait_timeout)));

  // Add timer and subscriber to executor.
  RCCHECK(rclc_executor_add_timer(&executor, &timer));

  RCCHECK(rclc_executor_add_subscription(&executor, &cmd_vel_subscriber,
                                         &cmd_vel_msg, &cmd_vel_callback,
                                         ON_NEW_DATA));

  while (1) {
    rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
    usleep(100000);
  }

  // Free resources.
  RCCHECK(rcl_subscription_fini(&cmd_vel_subscriber, &node));
  RCCHECK(rcl_publisher_fini(&odometry_publisher, &node));
  RCCHECK(rcl_node_fini(&node));

  vTaskDelete(NULL);
}

void app_main(void) {
#if defined(CONFIG_MICRO_ROS_ESP_NETIF_WLAN) ||                                \
    defined(CONFIG_MICRO_ROS_ESP_NETIF_ENET)
  ESP_ERROR_CHECK(uros_network_interface_initialize());
#endif

  // pin micro-ros task in APP_CPU to make PRO_CPU to deal with wifi:
  xTaskCreate(init_task, "uros_task", CONFIG_ROSOLINO_TASK_STACK, NULL,
              CONFIG_ROSOLINO_TASK_PRIO, NULL);
}
