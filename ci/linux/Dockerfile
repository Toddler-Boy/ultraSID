# The Linux build image: Ubuntu 22.04 sets the AppImage's glibc floor (2.35),
# the toolchain on top is clang from apt.llvm.org, libstdc++ from the toolchain
# PPA and CMake from Kitware. Rebuilt by the "Linux image" workflow
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive LANG=C.UTF-8 LC_ALL=C.UTF-8

RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
        git curl wget ca-certificates gnupg lsb-release software-properties-common \
        ninja-build pkg-config p7zip-full file binutils \
        libasound2-dev libjack-jackd2-dev libcurl4-openssl-dev \
        libfreetype-dev libfontconfig1-dev \
        libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxi-dev \
        libxinerama-dev libxrandr-dev libxrender-dev \
        libegl-dev libgl-dev libglu1-mesa-dev mesa-common-dev \
    && wget -qO /tmp/llvm.sh https://apt.llvm.org/llvm.sh \
    && bash /tmp/llvm.sh 22 \
    && add-apt-repository -y ppa:ubuntu-toolchain-r/test \
    && apt-get install -y -qq --no-install-recommends libstdc++-14-dev \
    && ln -sf /usr/bin/clang-22 /usr/local/bin/clang \
    && ln -sf /usr/bin/clang++-22 /usr/local/bin/clang++ \
    && ln -sf /usr/bin/ld.lld-22 /usr/local/bin/ld.lld \
    && curl -sSL https://github.com/Kitware/CMake/releases/download/v4.3.0/cmake-4.3.0-linux-x86_64.tar.gz | tar xz -C /opt \
    && ln -sf /opt/cmake-4.3.0-linux-x86_64/bin/cmake /opt/cmake-4.3.0-linux-x86_64/bin/cpack /usr/local/bin/ \
    && rm -rf /var/lib/apt/lists/* /tmp/llvm.sh
