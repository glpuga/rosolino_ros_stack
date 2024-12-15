# ROSolino

TBD

### References

- https://discourse.ros.org/t/micro-ros-esp32-cam/18827
- https://github.com/micro-ROS/micro_ros_espidf_component
- https://github.com/espressif/esp32-camera
- https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32/get-started/index.html



. $IDF_PATH/export.sh
cd examples/int32_publisher
# Set target board [esp32|esp32s2|esp32s3|esp32c3]
idf.py set-target esp32
idf.py menuconfig
# Set your micro-ROS configuration and WiFi credentials under micro-ROS Settings
idf.py build
idf.py flash
idf.py monitor
