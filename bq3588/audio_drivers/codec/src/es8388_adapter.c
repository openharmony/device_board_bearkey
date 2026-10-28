/*
 * Copyright © 2015-2026 Xiamen Bearkey Technology Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include "es8388_impl.h"
#include "audio_codec_if.h"
#include "audio_codec_base.h"
#include "audio_driver_log.h"

struct CodecData g_es8388Data = {
    .Init = Es8388DeviceInit,
    .Read = Es8388DeviceRegRead,
    .Write = Es8388DeviceRegWrite,
};

struct AudioDaiOps g_es8388DaiDeviceOps = {
    .Startup = Es8388DaiStartup,
    .HwParams = Es8388DaiHwParams,
    .Trigger = Es8388NormalTrigger,
};

struct DaiData g_es8388DaiData = {
    .drvDaiName = "codec_dai",
    .DaiInit = Es8388DaiDeviceInit,
    .ops = &g_es8388DaiDeviceOps,
};

/* HdfDriverEntry implementations */
static int32_t Es8388DriverBind(struct HdfDeviceObject *device)
{
    (void)device;
    AUDIO_DRIVER_LOG_INFO("success!");
    return HDF_SUCCESS;
}

static int32_t Es8388DriverInit(struct HdfDeviceObject *device)
{
    int32_t ret;

    if (device == NULL) {
        AUDIO_DRIVER_LOG_ERR("device is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }

    if (CodecGetConfigInfo(device, &g_es8388Data) !=  HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("CodecGetConfigInfo failed.");
        return HDF_FAILURE;
    }

    if (CodecDaiGetPortConfigInfo(device, &g_es8388DaiData) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("get port config info failed.");
        return HDF_FAILURE;
    }

    if (CodecSetConfigInfoOfControls(&g_es8388Data, &g_es8388DaiData) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("set config info failed.");
        return HDF_FAILURE;
    }

    ret = CodecGetServiceName(device, &(g_es8388Data.drvCodecName));
    if (ret != HDF_SUCCESS) {
        return ret;
    }
    AUDIO_DRIVER_LOG_INFO("g_es8388Data.drvCodecName = %s", g_es8388Data.drvCodecName);

    OsalMutexInit(&g_es8388Data.mutex);
    ret = AudioRegisterCodec(device, &g_es8388Data, &g_es8388DaiData);
    if (ret !=  HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("AudioRegisterCodec failed.");
        return ret;
    }
    AUDIO_DRIVER_LOG_INFO("success!");
    return HDF_SUCCESS;
}

struct HdfDriverEntry g_es8388DriverEntry = {
    .moduleVersion = 1,
    .moduleName = "CODEC_ES8323",
    .Bind = Es8388DriverBind,
    .Init = Es8388DriverInit,
};
HDF_INIT(g_es8388DriverEntry);
