/*
 * Copyright © 2015-2026 Xiamen Bearkey Technology Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include "dma_ops.h"
#include "osal_mem.h"

struct AudioDmaOps g_dmaDeviceOps = {
    .DmaBufAlloc = DmaBufferAlloc,
    .DmaBufFree = DmaBufferFree,
    .DmaRequestChannel = DmaChannelRequest,
    .DmaConfigChannel = DmaChannelConfig,
    .DmaPrep = DmaPrepare,
    .DmaSubmit = DmaTransferSubmit,
    .DmaPending = DmaIssuePending,
    .DmaPause = DmaTransferPause,
    .DmaResume = DmaTransferResume,
    .DmaPointer = DmaPcmPointer,
};

struct PlatformData g_platformData = {
    .PlatformInit = DmaDeviceInit,
    .ops = &g_dmaDeviceOps,
};

static struct PlatformHost *DmaHostCreate(struct HdfDeviceObject *device)
{
    struct PlatformHost *platformHost = NULL;

    platformHost = (struct PlatformHost *)OsalMemCalloc(sizeof(*platformHost));
    if (platformHost == NULL) {
        AUDIO_DEVICE_LOG_ERR("malloc host fail!");
        return NULL;
    }

    platformHost->device = device;
    device->service = &platformHost->service;

    return platformHost;
}

static int32_t DmaDriverBind(struct HdfDeviceObject *device)
{
    if (device == NULL) {
        AUDIO_DEVICE_LOG_ERR("input para is NULL.");
        return HDF_FAILURE;
    }

    if (DmaHostCreate(device) == NULL) {
        return HDF_FAILURE;
    }

    AUDIO_DEVICE_LOG_DEBUG("success!");
    return HDF_SUCCESS;
}

static void DmaInitBuffMutex(void)
{
    OsalMutexInit(&g_platformData.renderBufInfo.buffMutex);
    OsalMutexInit(&g_platformData.captureBufInfo.buffMutex);
}

static void DmaDestroyBuffMutex(void)
{
    OsalMutexDestroy(&g_platformData.renderBufInfo.buffMutex);
    OsalMutexDestroy(&g_platformData.captureBufInfo.buffMutex);
}

static int32_t DmaDriverInit(struct HdfDeviceObject *device)
{
    int32_t ret;
    struct DeviceResourceIface *drsOps = NULL;

    if (device == NULL || device->property == NULL) {
        AUDIO_DEVICE_LOG_ERR("device or node is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }

    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetString == NULL) {
        AUDIO_DEVICE_LOG_ERR("get drsops object instance fail!");
        return HDF_FAILURE;
    }

    ret = drsOps->GetString(device->property, "serviceName", &g_platformData.drvPlatformName, 0);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("read serviceName fail!");
        return ret;
    }
    AUDIO_DEVICE_LOG_INFO("serviceName:%s.", g_platformData.drvPlatformName);

    DmaInitBuffMutex();
    ret = AudioSocRegisterPlatform(device, &g_platformData);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("register dai fail.");
        DmaDestroyBuffMutex();
        return ret;
    }

    AUDIO_DEVICE_LOG_DEBUG("success.\n");
    return HDF_SUCCESS;
}

static void DmaDriverRelease(struct HdfDeviceObject *device)
{
    struct PlatformHost *platformHost = NULL;

    if (device == NULL) {
        AUDIO_DEVICE_LOG_ERR("device is NULL");
        return;
    }

    platformHost = (struct PlatformHost *)device->service;
    if (platformHost == NULL) {
        AUDIO_DEVICE_LOG_ERR("platformHost is NULL");
        return;
    }

    DmaDestroyBuffMutex();
    OsalMemFree(platformHost);
    device->service = NULL;

    AUDIO_DEVICE_LOG_DEBUG("success.\n");
}

struct HdfDriverEntry g_platformDriverEntry = {
    .moduleVersion = 1,
    .moduleName = "DMA_RK3588",
    .Bind = DmaDriverBind,
    .Init = DmaDriverInit,
    .Release = DmaDriverRelease,
};

HDF_INIT(g_platformDriverEntry);