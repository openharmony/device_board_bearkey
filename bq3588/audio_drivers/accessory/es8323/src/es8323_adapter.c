/*
 * Copyright (C) 2022 HiHope Open Source Organization .
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include "es8323_codec_impl.h"
#include "audio_codec_if.h"
#include "audio_codec_base.h"
#include "audio_driver_log.h"

#define HDF_LOG_TAG "es8323_codec_adapter"

struct CodecData g_es8323Data = {
    .Init = Es8323DeviceInit,
    .Read = Es8323DeviceRegRead,
    .Write = Es8323DeviceRegWrite,
};

struct AudioDaiOps g_es8323DaiDeviceOps = {
    .Startup = Es8323DaiStartup,
    .HwParams = Es8323DaiHwParams,
    .Trigger = Es8323NormalTrigger,
};

struct DaiData g_es8323DaiData = {
    .drvDaiName = "codec_dai",
    .DaiInit = Es8323DaiDeviceInit,
    .ops = &g_es8323DaiDeviceOps,
};

/* HdfDriverEntry implementations */
static int32_t Es8323DriverBind(struct HdfDeviceObject *device)
{
    (void)device;
    AUDIO_DRIVER_LOG_INFO("success!");
    return HDF_SUCCESS;
}

static int32_t Es8323DriverInit(struct HdfDeviceObject *device)
{
    int32_t ret;

    if (device == NULL) {
        AUDIO_DRIVER_LOG_ERR("device is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }

    if (CodecGetConfigInfo(device, &g_es8323Data) !=  HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("CodecGetConfigInfo failed.");
        return HDF_FAILURE;
    }

    if (CodecDaiGetPortConfigInfo(device, &g_es8323DaiData) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("get port config info failed.");
        return HDF_FAILURE;
    }

    if (CodecSetConfigInfoOfControls(&g_es8323Data, &g_es8323DaiData) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("set config info failed.");
        return HDF_FAILURE;
    }

    ret = CodecGetServiceName(device, &(g_es8323Data.drvCodecName));
    if (ret != HDF_SUCCESS) {
        return ret;
    }
    AUDIO_DRIVER_LOG_INFO("g_es8323Data.drvCodecName = %s", g_es8323Data.drvCodecName);

    OsalMutexInit(&g_es8323Data.mutex);
    ret = AudioRegisterCodec(device, &g_es8323Data, &g_es8323DaiData);
    if (ret !=  HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("AudioRegisterCodec failed.");
        return ret;
    }
    AUDIO_DRIVER_LOG_INFO("success!");
    return HDF_SUCCESS;
}

static void Es8323DriverRelease(struct HdfDeviceObject *device)
{
    struct CodecHost *codecHost;
    if (device == NULL) {
        AUDIO_DRIVER_LOG_ERR("device is NULL.");
        return;
    }
    OsalMutexDestroy(&g_es8323Data.mutex);
    if (device->priv != NULL) {
        OsalMemFree(device->priv);
    }
    codecHost = (struct CodecHost *)device->service;
    if (codecHost == NULL) {
        HDF_LOGE("CodecDriverRelease: codecHost is NULL");
        return;
    }
    OsalMemFree(codecHost);
}

/* HdfDriverEntry definitions */
struct HdfDriverEntry g_es8323DriverEntry = {
    .moduleVersion = 1,
    .moduleName = "CODEC_ES8323",
    .Bind = Es8323DriverBind,
    .Init = Es8323DriverInit,
    .Release = Es8323DriverRelease,
};
HDF_INIT(g_es8323DriverEntry);
