# ROSolino

TBD

### References

- https://discourse.ros.org/t/micro-ros-esp32-cam/18827
- https://github.com/micro-ROS/micro_ros_espidf_component
- https://github.com/espressif/esp32-camera
- https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32/get-started/index.html



. $IDF_PATH/export.sh
idf.py build
idf.py flash
idf.py monitor



docker run -it --rm --net=host microros/micro-ros-agent:humble udp4 --port 8888
