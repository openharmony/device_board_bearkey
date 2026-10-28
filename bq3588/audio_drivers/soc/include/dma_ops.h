/*
 * Copyright © 2015-2026 Xiamen Bearkey Technology Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef DMA_OPS_H
#define DMA_OPS_H

#include "osal_io.h"
#include <linux/dmaengine.h>
#include <linux/slab.h>
#include "audio_driver_log.h"
#include "audio_core.h"
#include "audio_platform_base.h"

#define HDF_LOG_TAG platform_dma

#define I2S0_ADDR 0xfe470000

#define DMA_RX_CHANNEL 0
#define DMA_TX_CHANNEL 1
#define DMA_CHANNEL_MAX 2

#define DMA_SLAVE_MAX_BURST 8
#define DMA_PREP_FLAGS (DMA_PREP_INTERRUPT | DMA_CTRL_ACK)
#define DMA_COHERENT_MASK 0xffffffffUL

int32_t DmaTransferResume(const struct PlatformData *data, const enum AudioStreamType streamType);
int32_t DmaDeviceInit(const struct AudioCard *card, const struct PlatformDevice *platform);
int32_t DmaTransferSubmit(const struct PlatformData *data, const enum AudioStreamType streamType);
int32_t DmaBufferFree(struct PlatformData *data, const enum AudioStreamType streamType);
int32_t DmaChannelRequest(const struct PlatformData *data, const enum AudioStreamType streamType);
int32_t DmaPcmPointer(struct PlatformData *data, const enum AudioStreamType streamType, uint32_t *pointer);
int32_t DmaBufferAlloc(struct PlatformData *data, const enum AudioStreamType streamType);
int32_t DmaPrepare(const struct PlatformData *data, const enum AudioStreamType streamType);
int32_t DmaIssuePending(struct PlatformData *data, const enum AudioStreamType streamType);
int32_t DmaTransferPause(struct PlatformData *data, const enum AudioStreamType streamType);
int32_t DmaChannelConfig(const struct PlatformData *data, const enum AudioStreamType streamType);

#endif /* DMA_OPS_H */