FROM ubuntu

RUN echo "deb http://http.debian.net/debian jessie-backports main" | \
    sudo tee -a /etc/apt/sources.list
RUN apt-get update
RUN apt-get install -yq unzip
RUN apt-get install -yq git
RUN apt-get install -yq curl
RUN apt-get install -yq libopus-dev
RUN apt-get install -yq cmake build-essential
#ENV http_proxy http://squid:lepow123@lepow.net:3129/
#ENV https_proxy http://squid:lepow123@lepow.net:3129/
RUN apt-get install -yq \
    libtool \
    libboost-all-dev \
    cmake \
    g++ \
    libgflags-dev \
    gperf \
    libevent-dev \
    libgoogle-glog-dev \
    libmicrohttpd-dev \
    wget

RUN apt-get install -yq --force-yes libgrpc-dev
RUN apt-get install -yq openjdk-7-jdk openjdk-7-source
RUN apt-get install -yq pkg-config zip g++ zlib1g-dev
RUN wget https://github.com/bazelbuild/bazel/releases/download/0.1.0/bazel-0.1.0-installer-linux-x86_64.sh
RUN chmod +x bazel-0.1.0-installer-linux-x86_64.sh
RUN ./bazel-0.1.0-installer-linux-x86_64.sh --user
RUN apt-get install -yq libnice-dev
RUN apt-get install -yq liblog4cxx10-dev libsrtp-dev
RUN apt-get install -yq python-software-properties
RUN apt-get install -yq  software-properties-common
RUN add-apt-repository -y ppa:gstreamer-developers/ppa
RUN echo "deb http://ubuntu.kurento.org trusty-dev kms6" | sudo tee /etc/apt/sources.list.d/kurento-dev.list
RUN wget -O - http://ubuntu.kurento.org/kurento.gpg.key | sudo apt-key add -
RUN apt-get update
RUN apt-get install -yq libgstreamer1.5-dev gstreamer1.5-tools gstreamer1.5-plugins-ugly gstreamer1.5-plugins-good gstreamer1.5-plugins-base gstreamer1.5-plugins-bad gstreamer1.5-libav gstreamer1.5-nice
RUN apt-get install -yq kms-core-6.0-dev
RUN apt-get install -yq kms-elements-6.0-dev
RUN apt-get install -yq libavcodec-dev libavformat-dev
RUN cp /usr/share/zoneinfo/Asia/Shanghai /etc/localtime 
WORKDIR /home
RUN export PATH="$PATH:$HOME/bin"
