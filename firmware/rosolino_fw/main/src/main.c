
// standard headers
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// microros
#include <rcl/error_handling.h>
#include <rcl/rcl.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <uros_network_interfaces.h>

// camera
#include <esp_camera.h>

// messages
#include <geometry_msgs/msg/twist.h>
#include <nav_msgs/msg/odometry.h>
#include <sensor_msgs/msg/compressed_image.h>

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

// Before changing consider 40K is about enough for a jpeg encoded 640x480 image
#define IMAGE_BUFFER_SIZE 40000

rcl_allocator_t allocator;
rclc_support_t support;
rcl_init_options_t init_options;

rcl_node_t node;

rcl_timer_t odometry_timer;
rcl_timer_t image_timer;

rcl_subscription_t cmd_vel_subscriber;
geometry_msgs__msg__Twist cmd_vel_msg;

rcl_publisher_t odometry_publisher;
nav_msgs__msg__Odometry odometry_msg;

rcl_publisher_t image_publisher;
sensor_msgs__msg__CompressedImage image_msg;

uint8_t *image_buffer;

#define fill_current_timestamp(timestamp)                                      \
  {                                                                            \
    struct timespec ts;                                                        \
    clock_gettime(CLOCK_REALTIME, &ts);                                        \
    timestamp.sec = ts.tv_sec;                                                 \
    timestamp.nanosec = ts.tv_nsec;                                            \
  }

#define fill_static_string_field(field, value)                                 \
  {                                                                            \
    field.data = value;                                                        \
    field.size = strlen(value);                                                \
  }

#include "esp_camera.h"

// WROVER-KIT PIN Map
#define CAM_PIN_PWDN 32  // power down is not used
#define CAM_PIN_RESET -1 // software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    .xclk_freq_hz = 20000000, // EXPERIMENTAL: Set to 16MHz on ESP32-S2 or
                              // ESP32-S3 to enable EDMA mode
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_VGA,
    .jpeg_quality = 12,
    .fb_count = 2,
    .grab_mode = CAMERA_GRAB_LATEST,
};

static esp_err_t init_camera() {
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    return err;
  }

  return ESP_OK;
}

void initialize_camera() {
  if (ESP_OK != init_camera()) {
    printf("Failed to initialize camera\n");
    vTaskDelete(NULL);
  }
}

void publish_updated_image(sensor_msgs__msg__CompressedImage *msg_ptr) {
  sensor_msgs__msg__CompressedImage__init(msg_ptr);
  fill_current_timestamp(msg_ptr->header.stamp);
  fill_static_string_field(msg_ptr->header.frame_id,
                           CONFIG_ROSOLINO_IMAGE_FRAME_ID);

  msg_ptr->format.data = "jpeg";
  msg_ptr->format.size = strlen(msg_ptr->format.data);
  msg_ptr->data.data = image_buffer;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    printf("Camera Capture Failed");
    return;
  }

  if (IMAGE_BUFFER_SIZE >= fb->len) {
    msg_ptr->data.size = fb->len;
    memcpy(image_buffer, fb->buf, msg_ptr->data.size);
    RCSOFTCHECK(rcl_publish(&image_publisher, msg_ptr, NULL));
  } else {
    printf("Image buffer size is not enough, need %d bytes\n", fb->len);
  }
  esp_camera_fb_return(fb);
}

void update_odometry_message(nav_msgs__msg__Odometry *msg) {
  nav_msgs__msg__Odometry__init(msg);
  fill_current_timestamp(msg->header.stamp);
  fill_static_string_field(msg->header.frame_id,
                           CONFIG_ROSOLINO_ODOMETRY_FRAME_ID);
  fill_static_string_field(msg->child_frame_id,
                           CONFIG_ROSOLINO_ODOMETRY_BASE_LINK_FRAME_ID);
  msg->pose.pose.position.x = 1.0;
  msg->pose.pose.position.y = 2.0;
  msg->pose.pose.position.z = 3.0;
  msg->pose.pose.orientation.x = 0.0;
  msg->pose.pose.orientation.y = 0.0;
  msg->pose.pose.orientation.z = 0.0;
  msg->pose.pose.orientation.w = 1.0;
}

void odometry_publisher_callback(rcl_timer_t *timer, int64_t last_call_time) {
  (void)last_call_time;
  if (timer != NULL) {
    update_odometry_message(&odometry_msg);
    RCSOFTCHECK(rcl_publish(&odometry_publisher, &odometry_msg, NULL));
  }
}

void image_publisher_callback(rcl_timer_t *timer, int64_t last_call_time) {
  (void)last_call_time;
  if (timer != NULL) {
    // publish_updated_image(&image_msg);
  }
}

void cmd_vel_callback(const void *msgin) {
  const geometry_msgs__msg__Twist *msg =
      (const geometry_msgs__msg__Twist *)msgin;
  (void)msg;
  printf("Received cmd_vel message\n");
}

void initialize_memory_buffers() {
  image_buffer = (uint8_t *)malloc(IMAGE_BUFFER_SIZE);
  if (image_buffer == NULL) {
    printf("Failed to allocate memory for image buffer\n");
    vTaskDelete(NULL);
  }
  memset(image_buffer, 0, IMAGE_BUFFER_SIZE);
}

void main_task(void *arg) {
  printf("Initializing main task\n");
  rclc_executor_t executor = rclc_executor_get_zero_initialized_executor();

  RCCHECK(rclc_executor_init(&executor, &support.context, 4, &allocator));

  unsigned int rcl_wait_timeout = 5000; // in ms
  RCCHECK(rclc_executor_set_timeout(&executor, RCL_MS_TO_NS(rcl_wait_timeout)));

  RCCHECK(rclc_executor_add_subscription(&executor, &cmd_vel_subscriber,
                                         &cmd_vel_msg, &cmd_vel_callback,
                                         ON_NEW_DATA));

  RCCHECK(rclc_executor_add_timer(&executor, &odometry_timer));
  RCCHECK(rclc_executor_add_timer(&executor, &image_timer));

  while (1) {
    rclc_executor_spin_some(&executor, RCL_MS_TO_NS(1));
  }
}

// void camera_task(void *arg) {
//   printf("Initializing camera task\n");

//   rclc_executor_t executor = rclc_executor_get_zero_initialized_executor();
//   RCCHECK(rclc_executor_init(&executor, &support.context, 4, &allocator));
//   unsigned int rcl_wait_timeout = 5000; // in ms
//   RCCHECK(rclc_executor_set_timeout(&executor,
//   RCL_MS_TO_NS(rcl_wait_timeout)));

//   RCCHECK(rclc_executor_add_timer(&executor, &image_timer));

//   const unsigned int spin_period =
//       RCL_MS_TO_NS(CONFIG_ROSOLINO_SPIN_INTERVAL_MS);
//   rclc_executor_spin_period(&executor, spin_period);

//   while (1) {
//     UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
//     printf("Camera_task stack high watermark: %d\n", uxHighWaterMark / 1024);
//     usleep(1000000);
//   }
// }

void app_main(void) {
#if defined(CONFIG_MICRO_ROS_ESP_NETIF_WLAN) ||                                \
    defined(CONFIG_MICRO_ROS_ESP_NETIF_ENET)
  ESP_ERROR_CHECK(uros_network_interface_initialize());
#endif
  initialize_memory_buffers();
  initialize_camera();

  allocator = rcl_get_default_allocator();

  // Create init_options.
  init_options = rcl_get_zero_initialized_init_options();
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

  printf("ROS 2 node initialization, node name: %s\n",
         CONFIG_ROSOLINO_NODE_NAME);
  printf("ROS 2 namespace: %s\n", CONFIG_ROSOLINO_NODE_NAMESPACE);
  printf("ROS 2 domain id: %d\n", CONFIG_ROSOLINO_DOMAIN_ID);

  // Create node.
  node = rcl_get_zero_initialized_node();
  RCCHECK(rclc_node_init_default(&node, CONFIG_ROSOLINO_NODE_NAME,
                                 CONFIG_ROSOLINO_NODE_NAMESPACE, &support));

  // Create odometry publisher.
  RCCHECK(rclc_publisher_init_best_effort(
      &odometry_publisher, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(nav_msgs, msg, Odometry),
      CONFIG_ROSOLINO_ODOMETRY_TOPIC_NAME));

  // create image publisher
  RCCHECK(rclc_publisher_init_default(
      &image_publisher, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, CompressedImage),
      CONFIG_ROSOLINO_IMAGE_TOPIC_NAME "/compressed"));

  // Create subscriber.
  RCCHECK(rclc_subscription_init_default(
      &cmd_vel_subscriber, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
      CONFIG_ROSOLINO_CMD_VEL_TOPIC_NAME));

  // Create timers
  odometry_timer = rcl_get_zero_initialized_timer();
  image_timer = rcl_get_zero_initialized_timer();
  {
    const unsigned int timer_timeout = CONFIG_ROSOLINO_ODOMETRY_INTERVAL_MS;
    RCCHECK(rclc_timer_init_default(&odometry_timer, &support,
                                    RCL_MS_TO_NS(timer_timeout),
                                    odometry_publisher_callback));
  }
  {
    const unsigned int timer_timeout = 1000;
    RCCHECK(rclc_timer_init_default(&image_timer, &support,
                                    RCL_MS_TO_NS(timer_timeout),
                                    image_publisher_callback));
  }

  xTaskCreate(main_task, "main_task", CONFIG_ROSOLINO_TASK_STACK, NULL,
              CONFIG_ROSOLINO_TASK_PRIORITY, NULL);

  // enabling this goes beyond the resources on the board
  // xTaskCreate(camera_task, "camera_task", CONFIG_ROSOLINO_TASK_STACK, NULL,
  //             CONFIG_ROSOLINO_TASK_PRIORITY, NULL);

  while (1) {
    usleep(1000000);
  }
}
