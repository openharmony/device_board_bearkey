/*
 * Copyright © 2015-2026 Xiamen Bearkey Technology Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef DAI_OPS_H
#define DAI_OPS_H

#include "audio_core.h"
#include "audio_host.h"
#include "audio_dai_base.h"
#include "audio_platform_base.h"
#include "osal_io.h"
#include "audio_driver_log.h"

typedef enum {
    AUDIO_DEVICE_SAMPLE_RATE_8000   = 8000,    /* 8kHz sample_rate */
    AUDIO_DEVICE_SAMPLE_RATE_12000  = 12000,   /* 12kHz sample_rate */
    AUDIO_DEVICE_SAMPLE_RATE_11025  = 11025,   /* 11.025kHz sample_rate */
    AUDIO_DEVICE_SAMPLE_RATE_16000  = 16000,   /* 16kHz sample_rate */
    AUDIO_DEVICE_SAMPLE_RATE_22050  = 22050,   /* 22.050kHz sample_rate */
    AUDIO_DEVICE_SAMPLE_RATE_24000  = 24000,   /* 24kHz sample_rate */
    AUDIO_DEVICE_SAMPLE_RATE_32000  = 32000,   /* 32kHz sample_rate */
    AUDIO_DEVICE_SAMPLE_RATE_44100  = 44100,   /* 44.1kHz sample_rate */
    AUDIO_DEVICE_SAMPLE_RATE_48000  = 48000,   /* 48kHz sample_rate */
    AUDIO_DEVICE_SAMPLE_RATE_64000  = 64000,   /* 64kHz sample_rate */
    AUDIO_DEVICE_SAMPLE_RATE_88200  = 88200,   /* 88.2kHz sample_rate */
    AUDIO_DEVICE_SAMPLE_RATE_96000  = 96000,   /* 96kHz sample_rate */
    AUDIO_DEVICE_SAMPLE_RATE_176400  = 176400, /* 176.4kHz sample_rate */
    AUDIO_DEVICE_SAMPLE_RATE_192000  = 192000, /* 192kHz sample_rate */
    AUDIO_DEVICE_SAMPLE_RATE_BUTT,
} AudioDeviceSampleRate;

int32_t DaiDeviceInit(struct AudioCard *card,
    const struct DaiDevice *dai);
int32_t NormalTrigger(const struct AudioCard *card,
    int cmd, const struct DaiDevice *dai);
int32_t DeviceReadReg(const struct DaiDevice *dai,
    uint32_t reg, uint32_t *val);
int32_t DaiStartup(const struct AudioCard *card,
    const struct DaiDevice *dai);
int32_t DeviceWriteReg(const struct DaiDevice *dai,
    uint32_t reg, uint32_t value);
int32_t DaiHwParams(const struct AudioCard *card,
    const struct AudioPcmHwParams *param);

#endif