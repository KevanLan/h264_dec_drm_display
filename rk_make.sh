#! /bin/sh
TOP_DIR=$(pwd)
BUILDROOT_TARGET_PATH=$(pwd)/../../../buildroot/output/target/

arm-linux-gcc -Wno-multichar -o h264_display_demo -g test.c rk_vdec.c bo.c dev.c modeset.c\
    -I$(pwd)/../inc \
    -I$(pwd)/../test \
    -I$(pwd)/../mpp/mpp/inc \
    -I$(pwd)/../osal/inc \
    -L$(pwd)/ \
    -lmpp \
    -ldrm

cp $TOP_DIR/h264_display_demo $BUILDROOT_TARGET_PATH/usr/bin/

echo "h264_display_demo is ready !"
