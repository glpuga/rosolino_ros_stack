FROM ros:humble-ros-base-jammy AS main_development_image

ARG USER=developer
ARG GROUP=developer

ENV DEBIAN_FRONTEND noninteractive

WORKDIR /root

RUN apt update \
 && apt install --no-install-recommends -y \
    bash-completion \
    build-essential \
    ccache \
    curl \
    gdb \
    git \
    htop \
    locate \
    locate \
    mc \
    psmisc \
    python3-pip \
    python3-venv \
    screen \
    screen \
    tmux \
    wget \
 && rm -rf /var/lib/apt/lists/*

RUN sudo apt update \
 && sudo apt install -y --no-install-recommends \
    ros-humble-desktop \
 && sudo rm -rf /var/lib/apt/lists/*

RUN apt update \
 && apt install --no-install-recommends -y \
    bison \
    ccache \
    cmake ninja-build \
    dfu-util \
    flex \
    git \
    gperf \
    libffi-dev \
    libssl-dev \
    libusb-1.0-0 \
    python3 \
    python3-pip \
    python3-setuptools \
    wget \
 && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /opt/espressif \
 && cd /opt/espressif \
 && git clone -b v5.2 --recursive https://github.com/espressif/esp-idf.git

RUN cd /opt/espressif/esp-idf/components \
 && git clone -b master --recursive https://github.com/espressif/esp32-camera.git \
 && git clone -b humble --recursive https://github.com/micro-ROS/micro_ros_espidf_component.git

RUN addgroup --gid 1000 $GROUP \
 && adduser --uid 1000 --ingroup $GROUP --home /home/$USER --shell /bin/sh --disabled-password --gecos "" $USER \
 && adduser $USER sudo \
 && adduser $USER dialout \
 && echo "$USER ALL=NOPASSWD: ALL" >> /etc/sudoers.d/$USER

USER $USER:$GROUP

ENV USER_HOME /home/$USER
ENV USER_WS $USER_HOME/workspace
RUN mkdir -p $USER_WS/src

WORKDIR $USER_HOME

ENV IDF_PATH /opt/espressif/esp-idf

WORKDIR $USER_WS

RUN colcon mixin add default https://raw.githubusercontent.com/colcon/colcon-mixin-repository/master/index.yaml \
 && colcon mixin update default

COPY --chown=$USER:$GROUP docker/files/colcon_defaults.yaml /home/$USER/.colcon/defaults.yaml

COPY --chown=root:root docker/files/fixuid_config.yml /etc/fixuid/config.yml
RUN /bin/bash -c '\
    ARCH=`uname -m` && if [ "$ARCH" == "aarch64" ]; then FIXUID_ARCH="arm64"; else FIXUID_ARCH="amd64"; fi \
 && curl -SsL https://github.com/boxboat/fixuid/releases/download/v0.6.0/fixuid-0.6.0-linux-$FIXUID_ARCH.tar.gz | sudo tar -C /usr/local/bin -xzf - \
 && sudo chmod 4755 /usr/local/bin/fixuid \
 && cd /etc/fixuid \
 && sudo sed -i "s/_USER_/$USER/" config.yml \
 && sudo sed -i "s/_GROUP_/$GROUP/" config.yml \
 && echo Done installing fixuid for $FIXUID_ARCH'

# Prevent ROS from using the host network for multicast
ENV ROS_LOCALHOST_ONLY 1

#
# Final setup of the workspace

RUN mkdir -p $USER_HOME/persistence && chmod 777 $USER_HOME/persistence
COPY docker/files/developer_rc /home/developer/.developer_rc
RUN echo "source /home/developer/.developer_rc" >> /home/developer/.bashrc

RUN sudo updatedb

ENV WITHIN_DEV 1
ENV IDF_CCACHE_ENABLE 1

ENV SHELL /bin/bash
ENTRYPOINT ["fixuid", "-q", "/ros_entrypoint.sh", "/bin/bash"]
