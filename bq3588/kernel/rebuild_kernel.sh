#!/bin/bash

# Copyright (c) 2023 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 在sdk根目录执行本脚本，快速编译kernel
set -e
ROOT_DIR=`pwd`
pushd kernel/linux/linux-6.6
ENABLE_LTO_O0=disable_lto_O0
KERNEL_SRC_TMP_PATH=${ROOT_DIR}/out/kernel/src_tmp/linux-6.6
KERNEL_OBJ_TMP_PATH=${ROOT_DIR}/out/kernel/OBJ/linux-6.6

RAMDISK_ARG="disable_ramdisk"
MAKE_OHOS_ENV="GPUDRIVER=mali"
export KBUILD_OUTPUT=${KERNEL_OBJ_TMP_PATH}
RAMDISK_ARG=enable_ramdisk

#cd ${ROOT_DIR}
[ -e ${KERNEL_SRC_TMP_PATH}/boot_linux.img ] && rm ${KERNEL_SRC_TMP_PATH}/*.img

cd ${KERNEL_SRC_TMP_PATH}
echo "eval $MAKE_OHOS_ENV ./make-ohos.sh  TB-RK3588X0 $RAMDISK_ARG ${ENABLE_LTO_O0}"
eval $MAKE_OHOS_ENV ./make-ohos.sh  TB-RK3588X0 $RAMDISK_ARG ${ENABLE_LTO_O0}
eval $MAKE_OHOS_ENV ./make-boot.sh ../../../

cd ${ROOT_DIR}
cp ${KERNEL_SRC_TMP_PATH}/boot_linux.img out/bq3588/packages/phone/images/boot_linux.img
cp ${KERNEL_OBJ_TMP_PATH}/resource.img out/bq3588/packages/phone/images/resource.img

#
