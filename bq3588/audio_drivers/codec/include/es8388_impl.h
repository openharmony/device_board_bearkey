/*
 * Copyright © 2015-2026 Xiamen Bearkey Technology Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef ES8388_IMPL_H
#define ES8388_IMPL_H

#include "audio_codec_if.h"
#include "osal_mem.h"
#include "osal_time.h"
#include "osal_io.h"
#include "securec.h"
#include <linux/types.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#define HDF_LOG_TAG "es8388_codec"

enum Es8388I2sFormatRegVal {
    ES8388_I2S_SAMPLE_FORMAT_REG_VAL_24 = 0x0,
    ES8388_I2S_SAMPLE_FORMAT_REG_VAL_20 = 0x1,
    ES8388_I2S_SAMPLE_FORMAT_REG_VAL_16 = 0x2,
    ES8388_I2S_SAMPLE_FORMAT_REG_VAL_18 = 0x3,
    ES8388_I2S_SAMPLE_FORMAT_REG_VAL_32 = 0x4,
};

/**
 The following enum values correspond to the location of the configuration
 parameters in the HCS file.
 If you modify the configuration parameters, you need to modify this value.
*/
enum Es8388DaiHwParamsIndex {
    ES8388_DHP_RENDER_FREQUENCY_INX = 0,
    ES8388_DHP_RENDER_FORMAT_INX = 1,
    ES8388_DHP_RENDER_CHANNEL_INX = 2,
    ES8388_DHP_CAPTURE_FREQUENCY_INX = 3,
    ES8388_DHP_CAPTURE_FORMAT_INX = 4,
    ES8388_DHP_CAPTURE_CHANNEL_INX = 5,
};

/* ES8388 register address map (was previously a set of macros). */
enum Es8388RegAddr {
    ES8388_CONTROL1         = 0x00,
    ES8388_CONTROL2         = 0x01,
    ES8388_CHIPPOWER        = 0x02,
    ES8388_ADCPOWER         = 0x03,
    ES8388_DACPOWER         = 0x04,
    ES8388_CHIPLOPOW1       = 0x05,
    ES8388_CHIPLOPOW2       = 0x06,
    ES8388_ANAVOLMANAG      = 0x07,
    ES8388_MASTERMODE       = 0x08,
    ES8388_ADCCONTROL1      = 0x09,
    ES8388_ADCCONTROL2      = 0x0a,
    ES8388_ADCCONTROL3      = 0x0b,
    ES8388_ADCCONTROL4      = 0x0c,
    ES8388_ADCCONTROL5      = 0x0d,
    ES8388_ADCCONTROL6      = 0x0e,
    ES8388_ADCCONTROL7      = 0x0f,
    ES8388_ADCCONTROL8      = 0x10,
    ES8388_ADCCONTROL9      = 0x11,
    ES8388_ADCCONTROL10     = 0x12,
    ES8388_ADCCONTROL11     = 0x13,
    ES8388_ADCCONTROL12     = 0x14,
    ES8388_ADCCONTROL13     = 0x15,
    ES8388_ADCCONTROL14     = 0x16,

    ES8388_DACCONTROL1      = 0x17,
    ES8388_DACCONTROL2      = 0x18,
    ES8388_DACCONTROL3      = 0x19,
    ES8388_DACCONTROL4      = 0x1a,
    ES8388_DACCONTROL5      = 0x1b,
    ES8388_DACCONTROL6      = 0x1c,
    ES8388_DACCONTROL7      = 0x1d,
    ES8388_DACCONTROL8      = 0x1e,
    ES8388_DACCONTROL9      = 0x1f,
    ES8388_DACCONTROL10     = 0x20,
    ES8388_DACCONTROL11     = 0x21,
    ES8388_DACCONTROL12     = 0x22,
    ES8388_DACCONTROL13     = 0x23,
    ES8388_DACCONTROL14     = 0x24,
    ES8388_DACCONTROL15     = 0x25,
    ES8388_DACCONTROL16     = 0x26,
    ES8388_DACCONTROL17     = 0x27,
    ES8388_DACCONTROL18     = 0x28,
    ES8388_DACCONTROL19     = 0x29,
    ES8388_DACCONTROL20     = 0x2a,
    ES8388_DACCONTROL21     = 0x2b,
    ES8388_DACCONTROL22     = 0x2c,
    ES8388_DACCONTROL23     = 0x2d,
    ES8388_DACCONTROL24     = 0x2e,
    ES8388_DACCONTROL25     = 0x2f,
    ES8388_DACCONTROL26     = 0x30,
    ES8388_DACCONTROL27     = 0x31,
    ES8388_DACCONTROL28     = 0x32,
    ES8388_DACCONTROL29     = 0x33,
    ES8388_DACCONTROL30     = 0x34,

    ES8388_LADC_VOL         = ES8388_ADCCONTROL8,
    ES8388_RADC_VOL         = ES8388_ADCCONTROL9,

    ES8388_LDAC_VOL         = ES8388_DACCONTROL4,
    ES8388_RDAC_VOL         = ES8388_DACCONTROL5,

    ES8388_LOUT1_VOL        = ES8388_DACCONTROL24,
    ES8388_ROUT1_VOL        = ES8388_DACCONTROL25,
    ES8388_LOUT2_VOL        = ES8388_DACCONTROL26,
    ES8388_ROUT2_VOL        = ES8388_DACCONTROL27,

    ES8388_ADC_MUTE         = ES8388_ADCCONTROL7,
    ES8388_DAC_MUTE         = ES8388_DACCONTROL3,

    ES8388_IFACE            = ES8388_MASTERMODE,

    ES8388_ADC_IFACE        = ES8388_ADCCONTROL4,
    ES8388_ADC_SRATE        = ES8388_ADCCONTROL5,

    ES8388_DAC_IFACE        = ES8388_DACCONTROL1,
    ES8388_DAC_SRATE        = ES8388_DACCONTROL2,
};

/* Supported MCLK/LRCK ratios. */
enum Es8388MclkLrckRatio {
    NR_SUPPORTED_MCLK_LRCK_RATIOS = 5,
};

struct Es8388DaiHwParamsTransferData {
    uint8_t inputParamsBeginIndex;
    uint8_t inputParamsEndIndex;
};

struct Es8388DaiParamsVal {
    uint32_t frequencyVal;
    uint32_t formatVal;
    uint32_t channelVal;
};

int32_t Es8388DeviceRegRead(const struct CodecDevice *codec, uint32_t reg,
    uint32_t *value);
int32_t Es8388DeviceRegWrite(const struct CodecDevice *codec, uint32_t reg,
    uint32_t value);

int32_t Es8388DeviceInit(struct AudioCard *audioCard,
    const struct CodecDevice *device);
int32_t Es8388DaiDeviceInit(struct AudioCard *card,
    const struct DaiDevice *device);
int32_t Es8388DaiStartup(const struct AudioCard *card,
    const struct DaiDevice *device);
int32_t Es8388DaiHwParams(const struct AudioCard *card,
    const struct AudioPcmHwParams *param);
int32_t Es8388NormalTrigger(const struct AudioCard *card, int cmd,
    const struct DaiDevice *device);

/* codec private data */
struct Es8388Priv {
    struct regmap *regMap;
    unsigned int headsetGpio;
    unsigned int hpDetIrq;
    bool hpState;
};

extern struct Es8388Priv *GetCodecDevice(void);

#endif /* ES8388_IMPL_H */