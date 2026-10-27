/*
 * Copyright © 2015-2026 Xiamen Bearkey Technology Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include <sound/memalloc.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_dma.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/suspend.h>

#include "osal_uaccess.h"
#include "dma_ops.h"
#include "dai_linux.h"

struct HdfDeviceObject *g_audioDevice;

struct DmaStreamRuntime {
    struct dma_chan *dmaChn[DMA_CHANNEL_MAX];
    dma_cookie_t cookie[DMA_CHANNEL_MAX];
    uint32_t streamType;
};

struct DmaChannelEntry {
    struct dma_chan *chan;
    dma_cookie_t cookie;
};

struct DmaChannelTable {
    struct DmaChannelEntry entries[DMA_CHANNEL_MAX];
};

struct DmaChannelSpec {
    const char *name;
};

struct StreamConfig {
    uint32_t channelId;
    enum dma_transfer_direction direction;
    uint32_t slaveAddr;
    const char *descErrLog;
    const char *streamLog;
};

struct StreamHandle {
    const struct StreamConfig *config;
    struct DmaChannelEntry *entry;
};

static struct DmaChannelTable g_dmaChannelTable;
static struct device *g_dmaDevice;
static const char *g_i2sDtsPath = "/i2s@fe470000";

static const struct DmaChannelSpec g_dmaChannelSpecTable[DMA_CHANNEL_MAX] = {
    [DMA_RX_CHANNEL] = { .name = "rx" },
    [DMA_TX_CHANNEL] = { .name = "tx" },
};

static const struct StreamConfig g_streamConfigTable[DMA_CHANNEL_MAX] = {
    [DMA_RX_CHANNEL] = {
        .channelId = DMA_RX_CHANNEL,
        .direction = DMA_DEV_TO_MEM,
        .slaveAddr = I2S0_ADDR + I2S_RXDR,
        .descErrLog = "DMA_RX_CHANNEL desc create failed",
        .streamLog = "AUDIO_CAPTURE_STREAM",
    },
    [DMA_TX_CHANNEL] = {
        .channelId = DMA_TX_CHANNEL,
        .direction = DMA_MEM_TO_DEV,
        .slaveAddr = I2S0_ADDR + I2S_TXDR,
        .descErrLog = "DMA_TX_CHANNEL desc create failed",
        .streamLog = "AUDIO_RENDER_STREAM",
    },
};

static bool IsValidStreamType(enum AudioStreamType streamType)
{
    return streamType == AUDIO_CAPTURE_STREAM || streamType == AUDIO_RENDER_STREAM;
}

static uint32_t StreamTypeToChannelId(enum AudioStreamType streamType)
{
    return (streamType == AUDIO_RENDER_STREAM) ? DMA_TX_CHANNEL : DMA_RX_CHANNEL;
}

static struct CircleBufInfo *GetCircleBufInfo(struct PlatformData *data,
    enum AudioStreamType streamType)
{
    return (streamType == AUDIO_RENDER_STREAM) ? &data->renderBufInfo : &data->captureBufInfo;
}

static const struct CircleBufInfo *GetCircleBufInfoConst(const struct PlatformData *data,
    enum AudioStreamType streamType)
{
    return (streamType == AUDIO_RENDER_STREAM) ? &data->renderBufInfo : &data->captureBufInfo;
}

static const struct PcmInfo *GetPcmInfo(const struct PlatformData *data,
    enum AudioStreamType streamType)
{
    return (streamType == AUDIO_RENDER_STREAM) ? &data->renderPcmInfo : &data->capturePcmInfo;
}

int32_t DmaChannelRequest(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    (void)data;
    (void)streamType;

    AUDIO_DEVICE_LOG_DEBUG("sucess");
    return HDF_SUCCESS;
}

int32_t DmaPrepare(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    (void)data;
    (void)streamType;

    return HDF_SUCCESS;
}

static void ResolveStreamHandle(enum AudioStreamType streamType, struct StreamHandle *handle)
{
    uint32_t channelId = StreamTypeToChannelId(streamType);

    handle->config = &g_streamConfigTable[channelId];
    handle->entry = &g_dmaChannelTable.entries[channelId];
}

int32_t DmaTransferPause(struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct StreamHandle handle;

    (void)data;

    ResolveStreamHandle(streamType, &handle);

    // can not use dmaengine_pause function
    if (handle.entry->chan == NULL) {
        AUDIO_DEVICE_LOG_ERR("dmaChan is null");
        return HDF_FAILURE;
    }

    dmaengine_terminate_async(handle.entry->chan);

    AUDIO_DEVICE_LOG_DEBUG("success");
    return HDF_SUCCESS;
}

static void FillSlaveConfig(const struct StreamConfig *config, struct dma_slave_config *slaveConfig)
{
    slaveConfig->direction = config->direction;

    if (config->direction == DMA_MEM_TO_DEV) {
        slaveConfig->dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
        slaveConfig->dst_addr = config->slaveAddr;
        slaveConfig->dst_maxburst = DMA_SLAVE_MAX_BURST;
    } else {
        slaveConfig->src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
        slaveConfig->src_addr = config->slaveAddr;
        slaveConfig->src_maxburst = DMA_SLAVE_MAX_BURST;
    }

    slaveConfig->device_fc = 0;
    slaveConfig->slave_id = 0;
}

int32_t DmaChannelConfig(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct StreamHandle handle;
    struct dma_slave_config slaveConfig;
    int32_t ret;

    (void)data;

    ResolveStreamHandle(streamType, &handle);

    (void)memset_s(&slaveConfig, sizeof(slaveConfig), 0, sizeof(slaveConfig));
    FillSlaveConfig(handle.config, &slaveConfig);

    if (handle.entry->chan == NULL) {
        AUDIO_DEVICE_LOG_ERR("dmaChan is null");
        return HDF_FAILURE;
    }

    ret = dmaengine_slave_config(handle.entry->chan, &slaveConfig);
    if (ret != 0) {
        AUDIO_DEVICE_LOG_ERR("dmaengine_slave_config failed");
        return HDF_FAILURE;
    }

    AUDIO_DEVICE_LOG_DEBUG("success");
    return HDF_SUCCESS;
}

struct device *getDmaDevice(void)
{
    struct device_node *dmaNode = NULL;
    struct platform_device *platformDev = NULL;

    dmaNode = of_find_node_by_path(g_i2sDtsPath);
    if (dmaNode == NULL) {
        AUDIO_DEVICE_LOG_ERR("get device node failed.");
        return NULL;
    }

    platformDev = of_find_device_by_node(dmaNode);
    if (platformDev == NULL) {
        AUDIO_DEVICE_LOG_ERR("get platformdev failed.");
        return NULL;
    }

    g_dmaDevice = &platformDev->dev;
    return g_dmaDevice;
}

int32_t DmaBufferAlloc(struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct device *dmaDevice = getDmaDevice();
    struct CircleBufInfo *circleBuf = NULL;

    if (data == NULL) {
        AUDIO_DEVICE_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    if (dmaDevice == NULL) {
        AUDIO_DEVICE_LOG_ERR("dmaDevice is null");
        return HDF_FAILURE;
    }

    if (!IsValidStreamType(streamType)) {
        AUDIO_DEVICE_LOG_ERR("stream Type is invalude.");
        return HDF_FAILURE;
    }

    circleBuf = GetCircleBufInfo(data, streamType);
    if (circleBuf->virtAddr == NULL) {
        dmaDevice->coherent_dma_mask = DMA_COHERENT_MASK;
        AUDIO_DEVICE_LOG_DEBUG("%s", g_streamConfigTable[StreamTypeToChannelId(streamType)].streamLog);
        circleBuf->virtAddr = dma_alloc_wc(dmaDevice, circleBuf->cirBufMax,
            (dma_addr_t *)&circleBuf->phyAddr, GFP_DMA | GFP_KERNEL);
    }

    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t DmaBufferFree(struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct device *dmaDevice = NULL;
    struct CircleBufInfo *circleBuf = NULL;

    if (data == NULL) {
        AUDIO_DEVICE_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    if (!IsValidStreamType(streamType)) {
        AUDIO_DEVICE_LOG_ERR("stream Type is invalude.");
        return HDF_FAILURE;
    }

    dmaDevice = getDmaDevice();
    circleBuf = GetCircleBufInfo(data, streamType);

    AUDIO_DEVICE_LOG_DEBUG("%s", g_streamConfigTable[StreamTypeToChannelId(streamType)].streamLog);
    dma_free_wc(dmaDevice, circleBuf->cirBufMax, circleBuf->virtAddr, circleBuf->phyAddr);

    AUDIO_DEVICE_LOG_DEBUG("success");
    return HDF_SUCCESS;
}

static int32_t DmaRequestChannels(struct device *dmaDevice)
{
    uint32_t i;

    for (i = 0; i < DMA_CHANNEL_MAX; i++) {
        g_dmaChannelTable.entries[i].chan =
            dma_request_slave_channel(dmaDevice, g_dmaChannelSpecTable[i].name);
        if (g_dmaChannelTable.entries[i].chan == NULL) {
            AUDIO_DEVICE_LOG_ERR("dma_request_slave_channel streamType=%d failed", i);
            return HDF_FAILURE;
        }
    }

    return HDF_SUCCESS;
}

int32_t DmaDeviceInit(const struct AudioCard *card, const struct PlatformDevice *platform)
{
    struct PlatformData *data = NULL;
    struct device *dmaDevice = getDmaDevice();
    int32_t ret;

    if (card == NULL) {
        AUDIO_DEVICE_LOG_ERR("card is null.");
        return HDF_FAILURE;
    }

    g_audioDevice = card->device;

    data = PlatformDataFromCard(card);
    if (data == NULL) {
        AUDIO_DEVICE_LOG_ERR("PlatformDataFromCard failed.");
        return HDF_FAILURE;
    }

    if (data->platformInitFlag == true) {
        AUDIO_DRIVER_LOG_INFO("platform init complete!");
        return HDF_SUCCESS;
    }

    data->platformInitFlag = true;

    ret = DmaRequestChannels(dmaDevice);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t DmaIssuePending(struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct StreamHandle handle;

    (void)data;

    AUDIO_DEVICE_LOG_DEBUG("streamType = %d", streamType);

    ResolveStreamHandle(streamType, &handle);
    if (handle.entry->chan == NULL) {
        AUDIO_DEVICE_LOG_ERR("dmaChan is null");
        return HDF_FAILURE;
    }

    dma_async_issue_pending(handle.entry->chan);

    AUDIO_DEVICE_LOG_DEBUG("dmaChan chan_id = %d.", handle.entry->chan->chan_id);
    AUDIO_DEVICE_LOG_DEBUG("success");
    return HDF_SUCCESS;
}

static int32_t BytesToFrames(uint32_t frameBits, uint32_t size, uint32_t *pointer)
{
    if (pointer == NULL || frameBits == 0) {
        AUDIO_DEVICE_LOG_ERR("input para is error.");
        return HDF_FAILURE;
    }

    *pointer = size / frameBits;
    return HDF_SUCCESS;
}

static int32_t DmaPointerStream(const struct PlatformData *data, enum AudioStreamType streamType,
    uint32_t *pointer)
{
    struct StreamHandle handle;
    const struct CircleBufInfo *circleBuf = NULL;
    const struct PcmInfo *pcmInfo = NULL;
    struct dma_tx_state txState;
    uint32_t currentOffset;
    int32_t ret;

    ResolveStreamHandle(streamType, &handle);

    circleBuf = GetCircleBufInfoConst(data, streamType);
    pcmInfo = GetPcmInfo(data, streamType);

    if (handle.entry->chan == NULL) {
        AUDIO_DEVICE_LOG_ERR("dmaChan is null");
        return HDF_FAILURE;
    }

    dmaengine_tx_status(handle.entry->chan, handle.entry->cookie, &txState);

    if (txState.residue) {
        currentOffset = circleBuf->cirBufSize - txState.residue;
        ret = BytesToFrames(pcmInfo->frameSize, currentOffset, pointer);
        if (ret != HDF_SUCCESS) {
            AUDIO_DEVICE_LOG_ERR("BytesToFrames is failed.");
            return HDF_FAILURE;
        }
    } else {
        *pointer = 0;
    }

    return HDF_SUCCESS;
}

int32_t DmaPcmPointer(struct PlatformData *data, const enum AudioStreamType streamType,
    uint32_t *pointer)
{
    if (data == NULL) {
        AUDIO_DEVICE_LOG_ERR("input para is null.");
        return HDF_FAILURE;
    }

    return DmaPointerStream(data, streamType, pointer);
}

static int32_t DmaTransferStream(const struct PlatformData *data, enum AudioStreamType streamType)
{
    struct StreamHandle handle;
    const struct CircleBufInfo *circleBuf = NULL;
    struct dma_async_tx_descriptor *txDesc = NULL;

    ResolveStreamHandle(streamType, &handle);
    circleBuf = GetCircleBufInfoConst(data, streamType);

    if (handle.entry->chan == NULL) {
        AUDIO_DEVICE_LOG_ERR("dmaChan is null");
        return HDF_FAILURE;
    }

    txDesc = dmaengine_prep_dma_cyclic(handle.entry->chan, circleBuf->phyAddr,
        circleBuf->cirBufSize, circleBuf->periodSize, handle.config->direction, DMA_PREP_FLAGS);
    if (!txDesc) {
        AUDIO_DEVICE_LOG_ERR("%s", handle.config->descErrLog);
        return -ENOMEM;
    }

    handle.entry->cookie = dmaengine_submit(txDesc);
    return HDF_SUCCESS;
}

int32_t DmaTransferSubmit(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    int32_t ret;

    if (data == NULL) {
        AUDIO_DEVICE_LOG_ERR("input para is null.");
        return HDF_FAILURE;
    }

    ret = DmaTransferStream(data, streamType);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    AUDIO_DEVICE_LOG_DEBUG("success");
    return 0;
}

int32_t DmaTransferResume(const struct PlatformData *data, const enum AudioStreamType streamType)
{
    struct StreamHandle handle;
    int32_t ret;

    if (data == NULL) {
        AUDIO_DEVICE_LOG_ERR("data is null");
        return HDF_FAILURE;
    }

    ResolveStreamHandle(streamType, &handle);
    if (handle.entry->chan == NULL) {
        AUDIO_DEVICE_LOG_ERR("dmaChan is null");
        return HDF_FAILURE;
    }

    // use start Operation function
    ret = DmaTransferSubmit(data, streamType);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("call DmaTransferSubmit failed");
        return HDF_FAILURE;
    }

    dma_async_issue_pending(handle.entry->chan);

    AUDIO_DEVICE_LOG_DEBUG("success");
    return HDF_SUCCESS;
}