#!/bin/bash

export PLATFORM_VERSION=13
export ANDROID_MAJOR_VERSION=t
export ARCH=arm64

make ARCH=arm64 exlinux_defconfig

# ساخت کرنل به همراه ماژول‌ها
make ARCH=arm64 modules -j$(nproc)
