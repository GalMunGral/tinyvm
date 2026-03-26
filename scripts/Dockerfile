FROM ubuntu:24.04

ARG KERNEL_VERSION=6.13.7
ARG BUSYBOX_VERSION=1.36.1

# Install all build dependencies
RUN apt-get update -qq && apt-get install -y -qq \
    gcc \
    gcc-riscv64-linux-gnu \
    make \
    wget \
    bc \
    flex \
    bison \
    libelf-dev \
    libssl-dev \
    xz-utils \
    cpio \
    bzip2 \
    && rm -rf /var/lib/apt/lists/*

# Download and extract kernel source
RUN wget -q https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-${KERNEL_VERSION}.tar.xz \
    && tar xf linux-${KERNEL_VERSION}.tar.xz \
    && mv linux-${KERNEL_VERSION} /linux \
    && rm linux-${KERNEL_VERSION}.tar.xz

# Download and extract BusyBox source
RUN wget -q https://busybox.net/downloads/busybox-${BUSYBOX_VERSION}.tar.bz2 \
    && tar xf busybox-${BUSYBOX_VERSION}.tar.bz2 \
    && mv busybox-${BUSYBOX_VERSION} /busybox \
    && rm busybox-${BUSYBOX_VERSION}.tar.bz2