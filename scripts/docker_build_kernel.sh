#!/bin/bash
set -e

CROSS=riscv64-linux-gnu

cd /linux

make defconfig ARCH=riscv CROSS_COMPILE=${CROSS}-

# Disable after defconfig, before olddefconfig resolves dependencies.
# CONFIG_EFI (default y) selects both RISCV_ISA_C and EFI_GENERIC_STUB.
# NONPORTABLE=y stops CONFIG_PORTABLE from re-selecting EFI via syncconfig.
sed -i 's/# CONFIG_NONPORTABLE is not set/CONFIG_NONPORTABLE=y/' .config
sed -i 's/CONFIG_EFI=y/# CONFIG_EFI is not set/' .config

echo "CONFIG_VIRTIO_MMIO=y" >> .config
echo "CONFIG_VIRTIO_BLK=y" >> .config

make olddefconfig ARCH=riscv CROSS_COMPILE=${CROSS}-

make -j$(nproc) ARCH=riscv CROSS_COMPILE=${CROSS}- vmlinux

cp vmlinux /out/vmlinux
echo "kernel built: $(du -sh /out/vmlinux | cut -f1)"