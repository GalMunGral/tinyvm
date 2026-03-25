#!/bin/bash
set -e

BUSYBOX_VERSION=1.36.1
CROSS=riscv64-linux-gnu

apt-get update -qq
apt-get install -y -qq gcc gcc-${CROSS} make wget bzip2 bc cpio

wget -q https://busybox.net/downloads/busybox-${BUSYBOX_VERSION}.tar.bz2
tar xf busybox-${BUSYBOX_VERSION}.tar.bz2
cd busybox-${BUSYBOX_VERSION}

make defconfig CROSS_COMPILE=${CROSS}-
# Enable static linking; disable tc (uses CBQ types removed from newer kernel headers)
sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
sed -i 's/CONFIG_TC=y/# CONFIG_TC is not set/' .config

make -j$(nproc) CROSS_COMPILE=${CROSS}- ARCH=riscv
make install CONFIG_PREFIX=/staging CROSS_COMPILE=${CROSS}-

# Install /init from the mounted scripts dir
cp /scripts/init /staging/init
chmod +x /staging/init

# Pack as initramfs cpio archive
cd /staging
find . | cpio -H newc -o | gzip > /out/initramfs.cpio.gz

echo "initramfs built: $(du -sh /out/initramfs.cpio.gz | cut -f1)"
