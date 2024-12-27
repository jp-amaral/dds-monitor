FROM debian:bullseye-slim
ENV TZ=Europe/Lisbon
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
RUN printf "deb http://httpredir.debian.org/debian bullseye-backports main non-free\ndeb-src http://httpredir.debian.org/debian bullseye-backports main non-free\n" > /etc/apt/sources.list.d/backports.list

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    python3-pip \
    libasio-dev \
    libtinyxml2-dev \
    && pip3 install -U colcon-common-extensions vcstool \
    && rm -rf /var/lib/apt/lists/*


RUN wget https://github.com/Kitware/CMake/releases/download/v3.27.6/cmake-3.27.6-linux-x86_64.sh \
    && chmod +x cmake-3.27.6-linux-x86_64.sh \
    && ./cmake-3.27.6-linux-x86_64.sh --skip-license --prefix=/usr/local \
    && rm cmake-3.27.6-linux-x86_64.sh

# Build Fast DDS
WORKDIR /usr/src
RUN mkdir Fast-DDS && cd Fast-DDS \
    && wget https://raw.githubusercontent.com/eProsima/Fast-DDS/master/fastdds.repos \
    && mkdir src \
    && vcs import src < fastdds.repos \
    && colcon build --packages-up-to fastdds

# Copy Fast DDS installation to a temporary directory
WORKDIR /usr/src/Fast-DDS/install
RUN mkdir -p /fastdds-install && cp -r * /fastdds-install
RUN echo "source /fastdds-install/setup.bash" >> /etc/bash.bashrc

# Copy project files
WORKDIR /monitor
COPY *.hpp .
COPY *.h .
COPY *.cpp .
COPY CMakeLists.txt .

RUN /bin/bash -c "source /fastdds-install/setup.bash && mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)"
RUN chmod +x /monitor/build/monitor

# Run the application
CMD ["/bin/bash", "-c", "source /fastdds-install/setup.bash && /monitor/build/monitor"]