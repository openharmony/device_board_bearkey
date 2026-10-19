/*
 * Copyright © 2015-2026 Xiamen Bearkey Technology Co., Ltd
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */
#include "es8323_codec_impl.h"
#include "audio_control.h"
#include "audio_core.h"
#include "audio_host.h"
#include "audio_parse.h"
#include "audio_platform_base.h"
#include "audio_sapm.h"
#include "audio_driver_log.h"
#include "i2c_if.h"
#include "gpio_if.h"
#include "linux/of_gpio.h"
#include "audio_codec_base.h"
#include "audio_stream_dispatch.h"


#define HDF_LOG_TAG "es8323_codec"

#define RK3588_I2C_BUS_NUMBER      (7)    // i2c7(master)
/* ES8323 I2C Device address 0 0 1 0 0 0 AD0 R/W */
#define ES8323_I2C_DEV_ADDR        (0x11) // 0010000 (slave)
#define ES8323_I2C_REG_DATA_LEN    (1)    // reg value length is 8 bits

#define ES8323_I2C_WAIT_TIMES      (10)   // ms

#define RK3588_GPIO_NUMBER         (122)   // gpio3 RK_PD2;

/* Original accessory_base implements */
#define ES8323_COMM_SHIFT_8BIT     (8)
#define ES8323_COMM_MASK_FF        (0xFF)

#define ES8323_I2C_REG_SIZE        (1)
#define ES8323_I2C_MSG_NUM         (2)
#define ES8323_I2C_MSG_BUF_SIZE    (2)

#define NUM_NEG_SIXTEEN            (-16)

static const struct AudioSapmRoute g_audioRoutes[] = {
    { "SPKL", NULL, "SPKL PGA"},
    { "HPL", NULL, "HPL PGA"},
    { "HPR", NULL, "HPR PGA"},
    { "SPKL PGA", "Speaker1 Switch", "DAC1"},
    { "HPL PGA", "Headphone1 Switch", "DAC2"},

    { "ADCL", NULL, "LPGA"},
    { "ADCR", NULL, "RPGA"},
    { "LPGA", "LPGA MIC Switch", "MIC1"},
    { "RPGA", "RPGA MIC Switch", "MIC2"},
};

/*
 * release I2C object public function
 */
static void Es8323I2cRelease(struct I2cMsg *msgs, int16_t msgSize, DevHandle i2cHandle)
{
    if (msgs != NULL) {
        if (msgSize == 0 && msgs->buf != NULL) {
            OsalMemFree(msgs->buf);
            msgs->buf = NULL;
        } else if (msgSize == 1 && msgs[0].buf != NULL) {
            OsalMemFree(msgs[0].buf);
            msgs[0].buf = NULL;
        } else if (msgSize >= ES8323_I2C_MSG_NUM) {
            if (msgs[0].buf != NULL) {
                msgs[0].buf = NULL;
            }
            if (msgs[1].buf != NULL) {
                OsalMemFree(msgs[1].buf);
                msgs[1].buf = NULL;
            }
        }
        AUDIO_DRIVER_LOG_DEBUG("OsalMemFree msgBuf success.\n");
    }
    // close i2c device
    if (i2cHandle != NULL) {
        I2cClose(i2cHandle);
        i2cHandle = NULL;
        AUDIO_DRIVER_LOG_DEBUG("I2cClose success.\n");
    }
}

static int32_t Es8323I2cMsgFill(const struct AudioAddrConfig *regAttr, uint16_t rwFlag,
    uint8_t *regs, struct I2cMsg *msgs)
{
    uint8_t *msgBuf = NULL;
    if (rwFlag != 0 && rwFlag != I2C_FLAG_READ) {
        AUDIO_DRIVER_LOG_ERR("invalid rwFlag value: %d.", rwFlag);
        return HDF_ERR_INVALID_PARAM;
    }
    regs[0] = regAttr->addr;
    msgs[0].addr = ES8323_I2C_DEV_ADDR;
    msgs[0].flags = 0;
    msgs[0].len = ES8323_I2C_REG_DATA_LEN + 1;

    if (rwFlag == 0) { // write
        // S 11011A2A1 0 A ADDR A MS1 A LS1 A <....> P
        msgBuf = OsalMemCalloc(ES8323_I2C_REG_DATA_LEN + 1);
        if (msgBuf == NULL) {
            AUDIO_DRIVER_LOG_ERR("[write]: malloc buf failed!");
            return HDF_ERR_MALLOC_FAIL;
        }
        msgBuf[0] = regs[0];
        msgBuf[1] = (uint8_t)regAttr->value;
        if (ES8323_I2C_REG_DATA_LEN == ES8323_I2C_MSG_BUF_SIZE) { // when 2 bytes
            msgBuf[1] = (regAttr->value >> ES8323_COMM_SHIFT_8BIT); // High 8 bits
            msgBuf[ES8323_I2C_MSG_BUF_SIZE] = (uint8_t)(regAttr->value & ES8323_COMM_MASK_FF);    // Low 8 bits
        }
        msgs[0].buf = msgBuf;
    } else {
        // S 11011A2A1 0 A ADDR A Sr 11011A2A1 1 A MS1 A LS1 A <....> NA P
        msgBuf = OsalMemCalloc(ES8323_I2C_REG_DATA_LEN);
        if (msgBuf == NULL) {
            AUDIO_DRIVER_LOG_ERR("[read]: malloc buf failed!");
            return HDF_ERR_MALLOC_FAIL;
        }
        msgs[0].len = 1;
        msgs[0].buf = regs;
        msgs[1].addr = ES8323_I2C_DEV_ADDR;
        msgs[1].flags = I2C_FLAG_READ;
        msgs[1].len = ES8323_I2C_REG_DATA_LEN;
        msgs[1].buf = msgBuf;
    }
    return HDF_SUCCESS;
}

static int32_t Es8323I2cReadWrite(struct AudioAddrConfig *regAttr, uint16_t rwFlag)
{
    int32_t ret;
    DevHandle i2cHandle;
    int16_t transferMsgCount = 1;
    uint8_t regs[ES8323_I2C_REG_SIZE];
    struct I2cMsg msgs[ES8323_I2C_MSG_NUM];
    (void)memset_s(msgs, sizeof(struct I2cMsg) * ES8323_I2C_MSG_NUM, 0, sizeof(struct I2cMsg) * ES8323_I2C_MSG_NUM);

    if (regAttr == NULL || rwFlag > 1) {
        AUDIO_DRIVER_LOG_ERR("invalid parameter.");
        return HDF_ERR_INVALID_PARAM;
    }
    i2cHandle = I2cOpen(RK3588_I2C_BUS_NUMBER);
    if (i2cHandle == NULL) {
        AUDIO_DRIVER_LOG_ERR("open i2cBus:%u failed! i2cHandle:%p", RK3588_I2C_BUS_NUMBER, i2cHandle);
        return HDF_FAILURE;
    }
    if (rwFlag == I2C_FLAG_READ) {
        transferMsgCount = ES8323_I2C_MSG_NUM;
    }
    ret = Es8323I2cMsgFill(regAttr, rwFlag, regs, msgs);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("Es8323I2cMsgFill failed!");
        I2cClose(i2cHandle);
        return HDF_FAILURE;
    }
    ret = I2cTransfer(i2cHandle, msgs, transferMsgCount);
    if (ret != transferMsgCount) {
        AUDIO_DRIVER_LOG_ERR("I2cTransfer err:%d", ret);
        Es8323I2cRelease(msgs, transferMsgCount, i2cHandle);
        return HDF_FAILURE;
    }
    if (rwFlag == I2C_FLAG_READ) {
        regAttr->value = msgs[1].buf[0];
        if (ES8323_I2C_REG_DATA_LEN == ES8323_I2C_MSG_BUF_SIZE) { // when 2 bytes
            regAttr->value = (msgs[1].buf[0] << ES8323_COMM_SHIFT_8BIT) | msgs[1].buf[1]; // result value 16 bits
        }
        AUDIO_DEVICE_LOG_DEBUG("[read]: regAttr->regValue=0x%x.\n", regAttr->value);
    }
    Es8323I2cRelease(msgs, transferMsgCount, i2cHandle);
    return HDF_SUCCESS;
}

// Read contrl reg bits value
static int32_t Es8323RegBitsRead(struct AudioMixerControl *regAttr, uint32_t *regValue)
{
    int32_t ret;
    struct AudioAddrConfig regVal;
    if (regAttr == NULL || regAttr->reg < 0 || regValue == NULL) {
        AUDIO_DRIVER_LOG_ERR("input invalid parameter.");
        return HDF_ERR_INVALID_PARAM;
    }
    regVal.addr  = regAttr->reg;
    ret = Es8323I2cReadWrite(&regVal, I2C_FLAG_READ);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("Es8323RegBitsRead failed.");
        return HDF_FAILURE;
    }
    *regValue = regVal.value;
    regAttr->value = (regVal.value >> regAttr->shift) & regAttr->mask;
    if (regAttr->value > regAttr->max || regAttr->value < regAttr->min) {
        AUDIO_DRIVER_LOG_ERR("invalid bitsValue=0x%x", regAttr->value);
        return HDF_FAILURE;
    }
    if (regAttr->invert) {
        regAttr->value = regAttr->max - regAttr->value;
    }
    AUDIO_DEVICE_LOG_DEBUG("regAddr=0x%x, regValue=0x%x, currBitsValue=0x%x",
        regAttr->reg, regVal.value, regAttr->value);
    AUDIO_DEVICE_LOG_DEBUG("mask=0x%x, shift=%d, max=0x%x,min=0x%x, invert=%d",
        regAttr->mask, regAttr->shift, regAttr->max, regAttr->min, regAttr->invert);
    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

// Update contrl reg bits value
static int32_t Es8323RegBitsUpdate(struct AudioMixerControl regAttr)
{
    int32_t ret;
    struct AudioAddrConfig regVal;
    uint32_t newValue, newMask, value;
    if (regAttr.reg < 0) {
        AUDIO_DRIVER_LOG_ERR("input invalid parameter.");
        return HDF_ERR_INVALID_PARAM;
    }
    if (regAttr.invert) {
        regAttr.value = regAttr.max - regAttr.value;
    }
    newValue = regAttr.value << regAttr.shift;
    newMask = regAttr.mask << regAttr.shift;
    ret = Es8323RegBitsRead(&regAttr, &value);
    if (ret != HDF_SUCCESS) {
        ADM_LOG_ERR("Es8323RegBitsRead faileded, ret=%d.", ret);
        return HDF_FAILURE;
    }
    regVal.value = (value & ~newMask) | (newValue & newMask);
    regVal.addr  = regAttr.reg;
    ret = Es8323I2cReadWrite(&regVal, 0);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("Es8323I2cReadWrite faileded.");
        return HDF_FAILURE;
    }
    AUDIO_DEVICE_LOG_DEBUG("regAddr=0x%x, regValue=0x%x, oldValue=0x%x, newValue=0x%x,",
        regAttr.reg, regVal.value, regAttr.value, newValue);
    AUDIO_DEVICE_LOG_DEBUG(" mask=0x%x, shift=%d, max=0x%x, min=0x%x, invert=%d",
        newMask, regAttr.shift, regAttr.max, regAttr.min, regAttr.invert);
    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

static int32_t Es8323DaiParamsUpdate4Scene(struct AudioMixerControl *daiHwParamsRegCfgItem,
    struct Es8323DaiHwParamsTransferData transferData, struct Es8323DaiParamsVal daiParamsVal)
{
    int32_t ret;
    uint8_t i;

    for (i = transferData.inputParamsBeginIndex; i <= transferData.inputParamsEndIndex; i++) {
        switch (i) {
            case ES8323_DHP_RENDER_FREQUENCY_INX:
            case ES8323_DHP_CAPTURE_FREQUENCY_INX:
                daiHwParamsRegCfgItem[i].value = daiParamsVal.frequencyVal;
                break;
            case ES8323_DHP_RENDER_FORMAT_INX:
            case ES8323_DHP_CAPTURE_FORMAT_INX:
                daiHwParamsRegCfgItem[i].value = daiParamsVal.formatVal;
                break;
            case ES8323_DHP_RENDER_CHANNEL_INX:
            case ES8323_DHP_CAPTURE_CHANNEL_INX:
                daiHwParamsRegCfgItem[i].value = daiParamsVal.channelVal;
                break;
            default:
                continue;
        }
        ret = Es8323RegBitsUpdate(daiHwParamsRegCfgItem[i]);
        if (ret != HDF_SUCCESS) {
            AUDIO_DEVICE_LOG_ERR("Es8323RegBitsUpdate failed.");
            return HDF_FAILURE;
        }
    }

    return HDF_SUCCESS;
}

// update external codec I2S frequency
static int32_t Es8323DaiParamsUpdate(const struct AudioCard *card,
    struct Es8323DaiParamsVal daiParamsVal, bool playback)
{
    int32_t ret;
    struct AudioMixerControl *daiHwParamsRegCfgItem = NULL;
    struct Es8323DaiHwParamsTransferData transferData;

    ret = (card == NULL || card->rtd == NULL || card->rtd->codec == NULL
           || card->rtd->codec->devData == NULL || card->rtd->codec->devData->regConfig == NULL
           || card->rtd->codec->devData->regConfig->audioRegParams[AUDIO_DAI_PATAM_GROUP]->regCfgItem == NULL);
    if (ret) {
        AUDIO_DEVICE_LOG_ERR("input param is NULL.");
        return HDF_FAILURE;
    }

    daiHwParamsRegCfgItem = card->rtd->codec->devData->regConfig->audioRegParams[AUDIO_DAI_PATAM_GROUP]->regCfgItem;
    if (daiHwParamsRegCfgItem == NULL) {
        AUDIO_DEVICE_LOG_ERR("Register config item is NULL or count is 0.");
        return HDF_FAILURE;
    }

    if (!playback) { // capture
        transferData.inputParamsBeginIndex = ES8323_DHP_CAPTURE_FREQUENCY_INX;
        transferData.inputParamsEndIndex = ES8323_DHP_CAPTURE_FORMAT_INX;
    } else { // playback
        transferData.inputParamsBeginIndex = ES8323_DHP_RENDER_FREQUENCY_INX;
        transferData.inputParamsEndIndex = ES8323_DHP_RENDER_FORMAT_INX;
    }
    ret = Es8323DaiParamsUpdate4Scene(daiHwParamsRegCfgItem, transferData, daiParamsVal);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("Es8323DaiParamsUpdate4Scene failed.");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}


static int32_t Es8323DeviceFrequencyParse(uint32_t rate, uint16_t *freq)
{
    if (freq == NULL) {
        AUDIO_DRIVER_LOG_ERR("input param is NULL");
        return HDF_FAILURE;
    }
    switch (rate) {
        case ES8323_I2S_SAMPLE_FREQUENCY_8000:
            *freq = 0x0a;
            break;
        case ES8323_I2S_SAMPLE_FREQUENCY_11025:
            *freq = 0x07;
            break;
        case ES8323_I2S_SAMPLE_FREQUENCY_16000:
            *freq = 0x06;
            break;
        case ES8323_I2S_SAMPLE_FREQUENCY_22050:
            *freq = 0x04;
            break;
        case ES8323_I2S_SAMPLE_FREQUENCY_32000:
            *freq = 0x03;
            break;
        case ES8323_I2S_SAMPLE_FREQUENCY_44100:
            *freq = 0x02;
            break;
        case ES8323_I2S_SAMPLE_FREQUENCY_48000:
            *freq = 0x02;
            break;
        case ES8323_I2S_SAMPLE_FREQUENCY_88200:
            *freq = 0x00;
            break;
        case ES8323_I2S_SAMPLE_FREQUENCY_96000:
            *freq = 0x00;
            break;
        default:
            AUDIO_DRIVER_LOG_ERR("rate: %d is not support.", rate);
            return HDF_ERR_NOT_SUPPORT;
    }

    return HDF_SUCCESS;
}

int32_t Es8323DeviceRegRead(const struct CodecDevice *codec, uint32_t reg, uint32_t *val)
{
    int32_t ret;
    struct AudioAddrConfig regAttr;
    if (val == NULL) {
        AUDIO_DRIVER_LOG_ERR("input para is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }
    regAttr.addr = (uint8_t)reg;
    regAttr.value = 0;
    ret = Es8323I2cReadWrite(&regAttr, I2C_FLAG_READ);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("failed.");
        return HDF_FAILURE;
    }
    *val = regAttr.value;

    return HDF_SUCCESS;
}

int32_t Es8323DeviceRegWrite(const struct CodecDevice *codec, uint32_t reg, uint32_t value)
{
    int32_t ret;
    struct AudioAddrConfig regAttr;
    regAttr.addr = (uint8_t)reg;
    regAttr.value = (uint16_t)value;
    ret = Es8323I2cReadWrite(&regAttr, 0);
    if (ret != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("failed.");
        return HDF_FAILURE;
    }
    AUDIO_DRIVER_LOG_DEBUG("success");
    return HDF_SUCCESS;
}

/* Original es8323 functions */
static int32_t Es8323FormatParse(enum AudioFormat format, uint16_t *bitWidth)
{
    // current set default format(standard) for 24 bit
    if (bitWidth == NULL) {
        AUDIO_DEVICE_LOG_ERR("input param is NULL");
        return HDF_FAILURE;
    }
    switch (format) {
        case AUDIO_FORMAT_TYPE_PCM_16_BIT:
            *bitWidth = ES8323_I2S_SAMPLE_FORMAT_REG_VAL_16;
            break;
        case AUDIO_FORMAT_TYPE_PCM_24_BIT:
            *bitWidth = ES8323_I2S_SAMPLE_FORMAT_REG_VAL_24;
            break;
        case AUDIO_FORMAT_TYPE_PCM_32_BIT:
            *bitWidth = ES8323_I2S_SAMPLE_FORMAT_REG_VAL_32;
            break;
        default:
            AUDIO_DEVICE_LOG_ERR("format: %d is not support.", format);
            return HDF_ERR_NOT_SUPPORT;
    }

    return HDF_SUCCESS;
}

static int32_t Es8323WorkStatusEnable(const struct AudioCard *card)
{
    int32_t ret;
    uint8_t i;
    struct AudioMixerControl *daiStartupParamsRegCfgItem = NULL;
    uint8_t daiStartupParamsRegCfgItemCount;

    ret = (card == NULL || card->rtd == NULL || card->rtd->codec == NULL
        || card->rtd->codec->devData == NULL || card->rtd->codec->devData->regConfig == NULL
        || card->rtd->codec->devData->regConfig->audioRegParams[AUDIO_DAI_STARTUP_PATAM_GROUP]->regCfgItem == NULL);
    if (ret) {
        AUDIO_DEVICE_LOG_ERR("input param is NULL.");
        return HDF_FAILURE;
    }

    daiStartupParamsRegCfgItem = card->rtd->codec->devData->regConfig
        ->audioRegParams[AUDIO_DAI_STARTUP_PATAM_GROUP]->regCfgItem;
    daiStartupParamsRegCfgItemCount = card->rtd->codec->devData->regConfig
        ->audioRegParams[AUDIO_DAI_STARTUP_PATAM_GROUP]->itemNum;
    if (daiStartupParamsRegCfgItem == NULL || daiStartupParamsRegCfgItemCount == 0) {
        AUDIO_DEVICE_LOG_ERR("Register config item is NULL or count is 0.");
        return HDF_FAILURE;
    }
    for (i = 0; i < daiStartupParamsRegCfgItemCount; i++) {
        ret = Es8323RegBitsUpdate(daiStartupParamsRegCfgItem[i]);
        if (ret != HDF_SUCCESS) {
            AUDIO_DEVICE_LOG_ERR("Es8323RegBitsUpdate failed.");
            return HDF_FAILURE;
        }
    }
    return HDF_SUCCESS;
}

int32_t SpkGpioSetValue(uint16_t gpioNum, uint16_t value)
{
    int32_t ret;
    ret = gpio_request(gpioNum, "spk-con");
    if (ret != 0 && ret != NUM_NEG_SIXTEEN) {
        AUDIO_DEVICE_LOG_ERR("gpio0_b3 request failed. ret:%d", ret);
        gpio_free(gpioNum);
        return HDF_FAILURE;
    }
    ret = GpioSetDir(gpioNum, GPIO_DIR_OUT);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("set gpio dir failed. ret:%d", ret);
        return ret;
    }
    ret = GpioWrite(gpioNum, value);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("write gpio val failed. ret:%d", ret);
        return ret;
    }
    return HDF_SUCCESS;
}

/*
 * ES8323 Hardware Rest, enable speaker
*/
static int32_t Es8323HardwareRest(void)
{
    int32_t ret;
    ret = SpkGpioSetValue(RK3588_GPIO_NUMBER, GPIO_VAL_HIGH);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("write gpio val failed. ret:%d", ret);
        return ret;
    }

    return HDF_SUCCESS;
}

int32_t Es8323DeviceInit(struct AudioCard *audioCard, const struct CodecDevice *device)
{
    int32_t ret;

    if (audioCard == NULL || device == NULL || device->devData == NULL ||
        device->devData->sapmComponents == NULL || device->devData->controls == NULL) {
        AUDIO_DRIVER_LOG_ERR("input para is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }

    ret = Es8323HardwareRest();
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("Es8323HardwareRest failed.");
        return HDF_FAILURE;
    }

    ret = CodecDeviceInitRegConfig(device);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("CodecDeviceInitRegConfig failed.");
        return HDF_FAILURE;
    }

    if (AudioAddControls(audioCard, device->devData->controls, device->devData->numControls) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("add controls failed.");
        return HDF_FAILURE;
    }

    if (AudioSapmNewComponents(audioCard, device->devData->sapmComponents,
        device->devData->numSapmComponent) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("new components failed.");
        return HDF_FAILURE;
    }

    if (AudioSapmAddRoutes(audioCard, g_audioRoutes, HDF_ARRAY_SIZE(g_audioRoutes)) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("add route failed.");
        return HDF_FAILURE;
    }

    if (AudioSapmNewControls(audioCard) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("add sapm controls failed.");
        return HDF_FAILURE;
    }

    if (AudioSapmSleep(audioCard) != HDF_SUCCESS) {
        AUDIO_DRIVER_LOG_ERR("add sapm sleep modular failed.");
        return HDF_FAILURE;
    }
    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t Es8323DaiDeviceInit(struct AudioCard *card, const struct DaiDevice *device)
{
    if (device == NULL || device->devDaiName == NULL) {
        AUDIO_DEVICE_LOG_ERR("input para is NULL.");
        return HDF_FAILURE;
    }
    (void)card;

    return HDF_SUCCESS;
}

int32_t Es8323DaiStartup(const struct AudioCard *card, const struct DaiDevice *device)
{
    int ret;
    (void)card;
    (void)device;

    struct Es8323Priv  *priv = GetCodecDevice();
    if (priv == NULL) {
        AUDIO_DEVICE_LOG_ERR("priv is NULL, default use mic device!");
    }
    ret = Es8323WorkStatusEnable(card);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("Es8323WorkStatusEnable failed.");
        return HDF_FAILURE;
    }

    Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, 0x09, 0x33);
    if ((priv != NULL) && !(priv->hpState)) {
        Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, 0x0a, 0x00);
    } else {
        Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, 0x0a, 0x50);
    }

    return HDF_SUCCESS;
}

int32_t Es8323DaiHwParams(const struct AudioCard *card, const struct AudioPcmHwParams *param)
{
    int32_t ret;
    uint16_t frequency, bitWidth;
    struct Es8323DaiParamsVal daiParamsVal;
    bool playback = true;

    (void)card;
    if (param == NULL || param->cardServiceName == NULL) {
        AUDIO_DEVICE_LOG_ERR("input para is NULL.");
        return HDF_ERR_INVALID_PARAM;
    }
    ret = Es8323DeviceFrequencyParse(param->rate, &frequency);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("Es8323DeviceFrequencyParse failed.");
        return HDF_ERR_NOT_SUPPORT;
    }
    ret = Es8323FormatParse(param->format, &bitWidth);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("Es8323FormatParse failed.");
        return HDF_ERR_NOT_SUPPORT;
    }
    daiParamsVal.frequencyVal = frequency;
    daiParamsVal.formatVal = bitWidth;
    daiParamsVal.channelVal = param->channels;
    playback = (param->streamType == AUDIO_RENDER_STREAM) ? true : false;
    ret = Es8323DaiParamsUpdate(card, daiParamsVal, playback);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("Es8323DaiParamsUpdate failed.");
        return HDF_FAILURE;
    }
    AUDIO_DEVICE_LOG_DEBUG("channels = %d, rate = %d, periodSize = %d, \
        periodCount = %d, format = %d, cardServiceName = %s \n",
        param->channels, param->rate, param->periodSize,
        param->periodCount, (uint32_t)param->format, param->cardServiceName);
    AUDIO_DEVICE_LOG_DEBUG("success.");
    return HDF_SUCCESS;
}

int32_t Es8323NormalTrigger(const struct AudioCard *card, int cmd, const struct DaiDevice *device)
{
    int32_t ret;
    struct Es8323Priv  *priv = GetCodecDevice();

    switch (cmd) {
        case AUDIO_DRV_PCM_IOCTL_RENDER_START:
        case AUDIO_DRV_PCM_IOCTL_RENDER_RESUME:
            Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, ES8323_ANAVOLMANAG, 0x7C);
            Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, ES8323_CHIPPOWER, 0x00);
            if ((priv != NULL) && !(priv->hpState)) {
                ret = SpkGpioSetValue(RK3588_GPIO_NUMBER, GPIO_VAL_HIGH);
                if (ret != HDF_SUCCESS) {
                    AUDIO_DEVICE_LOG_ERR("write gpio val failed. ret:%d", ret);
                    return ret;
                }
                Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, ES8323_DACPOWER, 0x30);
            } else {
                Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, ES8323_DACPOWER, 0x0c);
                ret = SpkGpioSetValue(RK3588_GPIO_NUMBER, GPIO_VAL_LOW);
                if (ret != HDF_SUCCESS) {
                    AUDIO_DEVICE_LOG_ERR("write gpio val failed. ret:%d", ret);
                    return ret;
                }
            }
            break;
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_START:
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_RESUME:
            Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, ES8323_ANAVOLMANAG, 0x7C);
            Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, ES8323_CHIPPOWER, 0x00);
            Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, ES8323_DACPOWER, 0xc0);
            Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, ES8323_ADCPOWER, 0x00);
            break;
        case AUDIO_DRV_PCM_IOCTL_RENDER_STOP:
        case AUDIO_DRV_PCM_IOCTL_RENDER_PAUSE:
            Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, ES8323_DACPOWER, 0xc0);
            Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, ES8323_CHIPPOWER, 0xFF);
            Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, ES8323_ANAVOLMANAG, 0x7B);
            break;
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_STOP:
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_PAUSE:
            Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, ES8323_ADCPOWER, 0xFF);
            Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, ES8323_CHIPPOWER, 0xFF);
            Es8323DeviceRegWrite((const struct CodecDevice *)0x0710, ES8323_ANAVOLMANAG, 0x7B);
        break;

        default:
            break;
    }

    return HDF_SUCCESS;
}
