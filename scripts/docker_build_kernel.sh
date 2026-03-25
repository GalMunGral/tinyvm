#!/bin/bash
set -e

KERNEL_VERSION=6.13.7
CROSS=riscv64-linux-gnu

apt-get update -qq
apt-get install -y -qq gcc gcc-${CROSS} make wget bc flex bison libelf-dev libssl-dev xz-utils

wget -q https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-${KERNEL_VERSION}.tar.xz
tar xf linux-${KERNEL_VERSION}.tar.xz
cd linux-${KERNEL_VERSION}

make defconfig ARCH=riscv CROSS_COMPILE=${CROSS}-
make -j$(nproc) ARCH=riscv CROSS_COMPILE=${CROSS}- vmlinux

cp vmlinux /out/vmlinux
echo "kernel built: $(du -sh /out/vmlinux | cut -f1)"