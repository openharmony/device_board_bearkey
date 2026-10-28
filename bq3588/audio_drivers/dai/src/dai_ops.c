/*
 * Copyright © 2015-2026 Xiamen Bearkey Technology Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#include "audio_control.h"
#include "audio_dai_if.h"
#include "audio_stream_dispatch.h"
#include "dai_ops.h"
#include "dai_linux.h"

#define HDF_LOG_TAG dai_ops

static struct platform_device *GetPlatformDev(const struct DaiDevice *dai);
static int GetTriggeredFlag(int cmd);
static int GetStreamType(int cmd);
static int32_t TxAndRxStart(struct I2sTdmDev *i2sTdm,
    enum AudioStreamType streamType);
static int32_t TxAndRxStop(struct I2sTdmDev *i2sTdm,
    enum AudioStreamType streamType);
static int32_t TxAndRxSetReg(struct I2sTdmDev *i2sTdm,
    enum AudioStreamType streamType, int on);
static int32_t SetI2sMclkFrequency(struct I2sTdmDev *i2sTdm,
    const struct AudioPcmHwParams *param, unsigned int *mclkRate);
static int32_t SetI2sFormatVal(const struct AudioPcmHwParams *param);
static int32_t SetI2sChannels(struct I2sTdmDev *i2sTdm,
    const struct AudioPcmHwParams *param);
static int32_t ConfigInfoSetToReg(struct I2sTdmDev *i2sTdm,
    const struct AudioPcmHwParams *param, unsigned int divBclk,
    unsigned int divLrck, int32_t fmt);
static int32_t I2sTdmSetMclk(struct I2sTdmDev *i2sTdm,
    const struct AudioPcmHwParams *param);
static int32_t I2sTdmSetSysClk(struct I2sTdmDev *i2sTdm,
    const struct AudioPcmHwParams *param);

int32_t NormalTrigger(const struct AudioCard *card, int cmd,
    const struct DaiDevice *device)
{
    struct platform_device *platformdev = GetPlatformDev(device);
    struct I2sTdmDev *i2sTdm = NULL;
    int32_t triggerFlag = GetTriggeredFlag(cmd);
    enum AudioStreamType streamType = GetStreamType(cmd);
    int32_t ret;

    (void)card;

    if (platformdev == NULL) {
        AUDIO_DEVICE_LOG_ERR("platformdev is null");
        return HDF_FAILURE;
    }

    i2sTdm = dev_get_drvdata(&platformdev->dev);
    if (i2sTdm == NULL) {
        AUDIO_DEVICE_LOG_ERR("i2sTdm is null");
        return HDF_FAILURE;
    }

    ret = TxAndRxSetReg(i2sTdm, streamType, triggerFlag);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("TxAndRxSetReg is failed");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static int32_t ConfigInfoSetToReg(struct I2sTdmDev *i2sTdm,
    const struct AudioPcmHwParams *param, unsigned int divBclk,
    unsigned int divLrck, int32_t fmt)
{
    if (i2sTdm == NULL || param == NULL) {
        AUDIO_DEVICE_LOG_ERR("input para is null.");
        return HDF_FAILURE;
    }

    regmap_update_bits(i2sTdm->regmap, I2S_CLKDIV,
        I2S_CLKDIV_TXM_MASK | I2S_CLKDIV_RXM_MASK,
        I2sClkdivTxm(divBclk) | I2sClkdivRxm(divBclk));
    regmap_update_bits(i2sTdm->regmap, I2S_CKR,
        I2S_CKR_TSD_MASK | I2S_CKR_RSD_MASK,
        I2sCkrTsd(divLrck) | I2sCkrRsd(divLrck));

    if (param->streamType == AUDIO_RENDER_STREAM) {
        regmap_update_bits(i2sTdm->regmap, I2S_TXCR,
            I2S_TXCR_VDW_MASK | I2S_TXCR_CSR_MASK, fmt);
    } else {
        regmap_update_bits(i2sTdm->regmap, I2S_RXCR,
            I2S_RXCR_VDW_MASK | I2S_RXCR_CSR_MASK, fmt);
    }

    return HDF_SUCCESS;
}

int32_t DaiDeviceInit(struct AudioCard *card, const struct DaiDevice *dai)
{
    struct DaiData *data = NULL;

    (void)card;

    if (dai == NULL || dai->device == NULL || dai->devDaiName == NULL ||
        dai->devData == NULL) {
        AUDIO_DEVICE_LOG_ERR("input para is NULL.");
        return HDF_ERR_INVALID_PARAM;
    }

    data = dai->devData;
    if (DaiSetConfigInfoOfControls(data) != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("set config info fail.");
        return HDF_FAILURE;
    }

    if (data->daiInitFlag == true) {
        AUDIO_DRIVER_LOG_INFO("dai init complete!");
        return HDF_SUCCESS;
    }

    data->daiInitFlag = true;
    return HDF_SUCCESS;
}

int32_t DeviceWriteReg(const struct DaiDevice *dai, uint32_t reg,
    uint32_t value)
{
    struct platform_device *platformdev = GetPlatformDev(dai);
    struct I2sTdmDev *i2sTdm = NULL;

    if (platformdev == NULL) {
        AUDIO_DEVICE_LOG_ERR("platformdev is null");
        return HDF_FAILURE;
    }

    i2sTdm = dev_get_drvdata(&platformdev->dev);
    if (i2sTdm == NULL) {
        AUDIO_DEVICE_LOG_ERR("i2sTdm is null");
        return HDF_FAILURE;
    }

    if (regmap_write(i2sTdm->regmap, reg, value)) {
        AUDIO_DEVICE_LOG_ERR("write register fail: [%04x] = %04x", reg, value);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static int32_t SetI2sFormatVal(const struct AudioPcmHwParams *param)
{
    int32_t val = 0;

    if (param == NULL) {
        AUDIO_DEVICE_LOG_ERR("input param is null.");
        return -1;
    }

    switch (param->format) {
        case AUDIO_FORMAT_TYPE_PCM_8_BIT:
            val |= I2sTxcrVdw(8);
            break;
        case AUDIO_FORMAT_TYPE_PCM_16_BIT:
            val |= I2sTxcrVdw(16);
            break;
        case AUDIO_FORMAT_TYPE_PCM_24_BIT:
            val |= I2sTxcrVdw(24);
            break;
        case AUDIO_FORMAT_TYPE_PCM_32_BIT:
            val |= I2sTxcrVdw(32);
            break;
        default:
            AUDIO_DEVICE_LOG_ERR("format error, format is 0x%x", param->format);
            return -1;
    }

    return val;
}

static int32_t TxAndRxStop(struct I2sTdmDev *i2sTdm,
    enum AudioStreamType streamType)
{
    uint32_t val = 0;
    int retry = 10;
    int ret;
    const int delayTimeUs = 150;

    if (i2sTdm == NULL || i2sTdm->regmap == NULL) {
        AUDIO_DEVICE_LOG_ERR("i2sTdm is null");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_RENDER_STREAM) {
        ret = regmap_update_bits(i2sTdm->regmap, I2S_DMACR,
            I2S_DMACR_TDE_ENABLE, I2S_DMACR_TDE_DISABLE);
        if (ret != HDF_SUCCESS) {
            AUDIO_DEVICE_LOG_ERR("regmap_update_bits fail");
            return HDF_FAILURE;
        }
        i2sTdm->txStart = false;
    } else {
        ret = regmap_update_bits(i2sTdm->regmap, I2S_DMACR,
            I2S_DMACR_RDE_ENABLE, I2S_DMACR_RDE_DISABLE);
        if (ret != HDF_SUCCESS) {
            AUDIO_DEVICE_LOG_ERR("regmap_update_bits fail");
            return HDF_FAILURE;
        }
        i2sTdm->rxStart = false;
    }

    if (i2sTdm->txStart || i2sTdm->rxStart) {
        return HDF_SUCCESS;
    }

    ret = regmap_update_bits(i2sTdm->regmap, I2S_XFER,
        I2S_XFER_TXS_START | I2S_XFER_RXS_START,
        I2S_XFER_TXS_STOP | I2S_XFER_RXS_STOP);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("regmap_update_bits fail");
        return HDF_FAILURE;
    }

    udelay(delayTimeUs);
    ret = regmap_update_bits(i2sTdm->regmap, I2S_CLR,
        I2S_CLR_TXC | I2S_CLR_RXC, I2S_CLR_TXC | I2S_CLR_RXC);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("regmap_update_bits fail");
        return HDF_FAILURE;
    }

    ret = regmap_read(i2sTdm->regmap, I2S_CLR, &val);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("regmap_read fail");
        return HDF_FAILURE;
    }

    while (val && retry > 0) {
        ret = regmap_read(i2sTdm->regmap, I2S_CLR, &val);
        if (ret != HDF_SUCCESS) {
            AUDIO_DEVICE_LOG_ERR("regmap_read fail");
            return HDF_FAILURE;
        }
        retry--;
    }

    return HDF_SUCCESS;
}

static int32_t SetI2sMclkFrequency(struct I2sTdmDev *i2sTdm,
    const struct AudioPcmHwParams *param, unsigned int *mclkRate)
{
    int32_t ret = 0;

    if (i2sTdm == NULL || param == NULL || mclkRate == NULL) {
        AUDIO_DEVICE_LOG_ERR("input param is null.");
        return HDF_FAILURE;
    }

    switch (param->streamType) {
        case AUDIO_RENDER_STREAM:
            ret = clk_set_rate(i2sTdm->mclkTx, i2sTdm->mclkTxFreq);
            if (ret) {
                AUDIO_DEVICE_LOG_ERR("clk_set_rate for TX ret = %d", ret);
                return ret;
            }
            *mclkRate = clk_get_rate(i2sTdm->mclkTx);
            break;
        case AUDIO_CAPTURE_STREAM:
            ret = clk_set_rate(i2sTdm->mclkRx, i2sTdm->mclkRxFreq);
            if (ret) {
                AUDIO_DEVICE_LOG_ERR("clk_set_rate for RX ret = %d", ret);
                return ret;
            }
            *mclkRate = clk_get_rate(i2sTdm->mclkRx);
            break;
        default:
            AUDIO_DEVICE_LOG_ERR("streamType [%d] is invalid.", param->streamType);
            return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

int32_t DaiStartup(const struct AudioCard *card,
    const struct DaiDevice *dai)
{
    (void)card;
    (void)dai;
    return HDF_SUCCESS;
}

static int GetTriggeredFlag(int cmd)
{
    int triggerFlag = false;

    switch (cmd) {
        case AUDIO_DRV_PCM_IOCTL_RENDER_START:
        case AUDIO_DRV_PCM_IOCTL_RENDER_RESUME:
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_START:
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_RESUME:
            triggerFlag = true;
            break;
        case AUDIO_DRV_PCM_IOCTL_RENDER_STOP:
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_STOP:
        case AUDIO_DRV_PCM_IOCTL_RENDER_PAUSE:
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_PAUSE:
            triggerFlag = false;
            break;
        default:
            triggerFlag = false;
            break;
    }

    return triggerFlag;
}

int32_t DeviceReadReg(const struct DaiDevice *dai, uint32_t reg,
    uint32_t *val)
{
    struct platform_device *platformdev = GetPlatformDev(dai);
    struct I2sTdmDev *i2sTdm = NULL;

    if (val == NULL) {
        AUDIO_DEVICE_LOG_ERR("val is null");
        return HDF_FAILURE;
    }

    if (platformdev == NULL) {
        AUDIO_DEVICE_LOG_ERR("platformdev is null");
        return HDF_FAILURE;
    }

    i2sTdm = dev_get_drvdata(&platformdev->dev);
    if (i2sTdm == NULL) {
        AUDIO_DEVICE_LOG_ERR("i2sTdm is null");
        return HDF_FAILURE;
    }

    if (regmap_read(i2sTdm->regmap, reg, val)) {
        AUDIO_DEVICE_LOG_ERR("read register fail: [%04x]", reg);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static int32_t I2sTdmSetMclk(struct I2sTdmDev *i2sTdm,
    const struct AudioPcmHwParams *param)
{
    int32_t ret = 0;
    unsigned int mclkRate = 0;
    unsigned int bclkRate = 0;
    unsigned int bclkDiv = 0;
    unsigned int lrclkDiv = 0;
    int32_t fmt = 0;
    int32_t channels = 0;

    ret = SetI2sMclkFrequency(i2sTdm, param, &mclkRate);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("SetI2sMclkFrequency failed, ret = %d", ret);
        return ret;
    }

    bclkRate = i2sTdm->bclkFs * param->rate;
    bclkDiv = DIV_ROUND_CLOSEST(mclkRate, bclkRate);
    lrclkDiv = bclkRate / param->rate;
    fmt = SetI2sFormatVal(param);
    if (fmt < 0) {
        AUDIO_DEVICE_LOG_ERR("SetI2sFormatVal fmt = %d", fmt);
        return HDF_FAILURE;
    }

    channels = SetI2sChannels(i2sTdm, param);
    if (channels < 0) {
        AUDIO_DEVICE_LOG_ERR("SetI2sChannels channels = %d", channels);
        return HDF_FAILURE;
    }

    fmt |= channels;
    ret = ConfigInfoSetToReg(i2sTdm, param, bclkDiv, lrclkDiv, fmt);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("ConfigInfoSetToReg ret = %d", ret);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static int32_t TxAndRxStart(struct I2sTdmDev *i2sTdm,
    enum AudioStreamType streamType)
{
    uint32_t val = 0;

    if (i2sTdm == NULL || i2sTdm->regmap == NULL) {
        AUDIO_DEVICE_LOG_ERR("i2sTdm is null");
        return HDF_FAILURE;
    }

    if (streamType == AUDIO_RENDER_STREAM) {
        if (i2sTdm->mclkTx == NULL) {
            AUDIO_DEVICE_LOG_ERR("mclk tx is null");
            return HDF_FAILURE;
        }

        if (clk_prepare_enable(i2sTdm->mclkTx) != HDF_SUCCESS) {
            AUDIO_DEVICE_LOG_ERR("clk_prepare_enable fail");
            return HDF_FAILURE;
        }

        if (regmap_update_bits(i2sTdm->regmap, I2S_DMACR,
            I2S_DMACR_TDE_ENABLE, I2S_DMACR_TDE_ENABLE)) {
            AUDIO_DEVICE_LOG_ERR("regmap_update_bits fail");
            return HDF_FAILURE;
        }
        i2sTdm->txStart = true;
    } else {
        if (i2sTdm->mclkRx == NULL) {
            AUDIO_DEVICE_LOG_ERR("mclk rx is null");
            return HDF_FAILURE;
        }

        if (clk_prepare_enable(i2sTdm->mclkRx) != HDF_SUCCESS) {
            AUDIO_DEVICE_LOG_ERR("clk_prepare_enable fail");
            return HDF_FAILURE;
        }

        if (regmap_update_bits(i2sTdm->regmap, I2S_DMACR,
            I2S_DMACR_RDE_ENABLE, I2S_DMACR_RDE_ENABLE)) {
            AUDIO_DEVICE_LOG_ERR("regmap_update_bits fail");
            return HDF_FAILURE;
        }

        if (regmap_read(i2sTdm->regmap, I2S_DMACR, &val)) {
            AUDIO_DEVICE_LOG_ERR("read register fail: [%04x]", I2S_DMACR);
            return HDF_FAILURE;
        }
        i2sTdm->rxStart = true;
    }

    if (regmap_update_bits(i2sTdm->regmap, I2S_XFER,
        I2S_XFER_TXS_START | I2S_XFER_RXS_START,
        I2S_XFER_TXS_START | I2S_XFER_RXS_START) != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("regmap_update_bits fail");
        return HDF_FAILURE;
    }

    if (regmap_read(i2sTdm->regmap, I2S_XFER, &val)) {
        AUDIO_DEVICE_LOG_ERR("read register fail: [%04x]", I2S_XFER);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static int GetStreamType(int cmd)
{
    enum AudioStreamType streamType = AUDIO_CAPTURE_STREAM;

    switch (cmd) {
        case AUDIO_DRV_PCM_IOCTL_RENDER_START:
        case AUDIO_DRV_PCM_IOCTL_RENDER_RESUME:
        case AUDIO_DRV_PCM_IOCTL_RENDER_STOP:
        case AUDIO_DRV_PCM_IOCTL_RENDER_PAUSE:
            streamType = AUDIO_RENDER_STREAM;
            break;
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_START:
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_RESUME:
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_STOP:
        case AUDIO_DRV_PCM_IOCTL_CAPTURE_PAUSE:
            streamType = AUDIO_CAPTURE_STREAM;
            break;
        default:
            break;
    }

    return streamType;
}

int32_t DaiHwParams(const struct AudioCard *card,
    const struct AudioPcmHwParams *param)
{
    int ret;
    uint32_t bitWidth;
    struct DaiDevice *dai = NULL;
    struct DaiData *data = DaiDataFromCard(card);
    struct platform_device *platformdev = NULL;
    struct I2sTdmDev *i2sTdm = NULL;

    if (card == NULL || card->rtd == NULL || param == NULL ||
        param->cardServiceName == NULL) {
        AUDIO_DEVICE_LOG_ERR("input para is NULL.");
        return HDF_ERR_INVALID_PARAM;
    }

    if (data == NULL) {
        AUDIO_DEVICE_LOG_ERR("data is nullptr.");
        return HDF_FAILURE;
    }

    dai = card->rtd->cpuDai;
    platformdev = GetPlatformDev(dai);
    if (platformdev == NULL) {
        AUDIO_DEVICE_LOG_ERR("platformdev is NULL.");
        return HDF_FAILURE;
    }

    data->pcmInfo.channels = param->channels;
    AUDIO_DEVICE_LOG_INFO("channels count : %d .", param->channels);

    if (AudioFormatToBitWidth(param->format, &bitWidth) != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("AudioFormatToBitWidth error");
        return HDF_FAILURE;
    }

    data->pcmInfo.bitWidth = bitWidth;
    data->pcmInfo.rate = param->rate;
    data->pcmInfo.streamType = param->streamType;

    i2sTdm = dev_get_drvdata(&platformdev->dev);
    if (i2sTdm == NULL) {
        AUDIO_DEVICE_LOG_ERR("i2sTdm is null");
        return HDF_FAILURE;
    }

    ret = I2sTdmSetSysClk(i2sTdm, param);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("I2sTdmSetSysClk error");
        return HDF_FAILURE;
    }

    ret = I2sTdmSetMclk(i2sTdm, param);
    if (ret != HDF_SUCCESS) {
        AUDIO_DEVICE_LOG_ERR("I2sTdmSetMclk error");
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static int32_t SetI2sChannels(struct I2sTdmDev *i2sTdm,
    const struct AudioPcmHwParams *param)
{
    unsigned int regFmt;
    unsigned int fmt;
    int32_t ret = 0;

    if (i2sTdm == NULL || param == NULL) {
        AUDIO_DEVICE_LOG_ERR("input param is null.");
        return HDF_FAILURE;
    }

    if (param->streamType == AUDIO_RENDER_STREAM) {
        regFmt = I2S_TXCR;
    } else {
        regFmt = I2S_RXCR;
    }

    regmap_read(i2sTdm->regmap, regFmt, &fmt);
    fmt &= I2S_TXCR_TFS_MASK;
    if (fmt == 0) {
        switch (param->channels) {
            case 8:
                ret = I2S_CHN_8;
                break;
            case 6:
                ret = I2S_CHN_6;
                break;
            case 4:
                ret = I2S_CHN_4;
                break;
            case 2:
                ret = I2S_CHN_2;
                break;
            default:
                ret = -EINVAL;
                break;
        }
    }

    return ret;
}

static int32_t TxAndRxSetReg(struct I2sTdmDev *i2sTdm,
    enum AudioStreamType streamType, int on)
{
    int32_t ret;

    if (i2sTdm == NULL || i2sTdm->regmap == NULL) {
        AUDIO_DEVICE_LOG_ERR("i2sTdm is null");
        return HDF_FAILURE;
    }

    if (on) {
        ret = TxAndRxStart(i2sTdm, streamType);
        if (ret != HDF_SUCCESS) {
            AUDIO_DEVICE_LOG_ERR("TxAndRxStart is failed");
            return HDF_FAILURE;
        }
    } else {
        ret = TxAndRxStop(i2sTdm, streamType);
        if (ret != HDF_SUCCESS) {
            AUDIO_DEVICE_LOG_ERR("TxAndRxStop is failed");
            return HDF_FAILURE;
        }
    }

    return HDF_SUCCESS;
}

static int32_t I2sTdmSetSysClk(struct I2sTdmDev *i2sTdm,
    const struct AudioPcmHwParams *param)
{
    uint32_t sampleRate;
    uint32_t mclkParentFreq = 0;

    if (i2sTdm == NULL || param == NULL) {
        AUDIO_DEVICE_LOG_ERR("input para is null.");
        return HDF_FAILURE;
    }

    sampleRate = param->rate;

    switch (sampleRate) {
        case AUDIO_DEVICE_SAMPLE_RATE_8000:
        case AUDIO_DEVICE_SAMPLE_RATE_16000:
        case AUDIO_DEVICE_SAMPLE_RATE_24000:
        case AUDIO_DEVICE_SAMPLE_RATE_32000:
        case AUDIO_DEVICE_SAMPLE_RATE_48000:
        case AUDIO_DEVICE_SAMPLE_RATE_64000:
        case AUDIO_DEVICE_SAMPLE_RATE_96000:
            mclkParentFreq =
                i2sTdm->bclkFs * AUDIO_DEVICE_SAMPLE_RATE_192000;
            break;
        case AUDIO_DEVICE_SAMPLE_RATE_11025:
        case AUDIO_DEVICE_SAMPLE_RATE_22050:
        case AUDIO_DEVICE_SAMPLE_RATE_44100:
            mclkParentFreq =
                i2sTdm->bclkFs * AUDIO_DEVICE_SAMPLE_RATE_176400;
            break;
        default:
            AUDIO_DEVICE_LOG_ERR("Invalid LRCK freq: %u Hz", sampleRate);
            return HDF_FAILURE;
    }

    i2sTdm->mclkTxFreq = mclkParentFreq;
    i2sTdm->mclkRxFreq = mclkParentFreq;

    return HDF_SUCCESS;
}

static struct platform_device *GetPlatformDev(const struct DaiDevice *dai)
{
    struct device_node *dmaOfNode = NULL;

    if (dai == NULL || dai->devData == NULL ||
        dai->devData->regConfig == NULL) {
        AUDIO_DEVICE_LOG_ERR("dai is null");
        return NULL;
    }

    dmaOfNode =
        of_find_node_by_path(dai->devData->regConfig->audioIdInfo.chipName);
    if (dmaOfNode == NULL) {
        AUDIO_DEVICE_LOG_ERR("get device node failed.");
        return NULL;
    }

    return of_find_device_by_node(dmaOfNode);
}