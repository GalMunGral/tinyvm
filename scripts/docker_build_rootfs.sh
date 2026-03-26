#!/bin/bash
set -e

CROSS=riscv64-linux-gnu

cd /busybox

make defconfig CROSS_COMPILE=${CROSS}-
# Enable static linking; disable tc (uses CBQ types removed from newer kernel headers)
sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
sed -i 's/CONFIG_TC=y/# CONFIG_TC is not set/' .config

make -j$(nproc) CROSS_COMPILE=${CROSS}- ARCH=riscv
make install CONFIG_PREFIX=/staging CROSS_COMPILE=${CROSS}-

cp /scripts/init /staging/init
chmod +x /staging/init

cd /staging
find . | cpio -H newc -o | gzip > /out/initramfs.cpio.gz

echo "initramfs built: $(du -sh /out/initramfs.cpio.gz | cut -f1)"