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

#define HDF_LOG_TAG dai_drivers
#define DRV_NAME "rockchip-i2s-tdm"

static bool I2sTdmWrReg(struct device *dev, unsigned int reg);
static bool I2sTdmRdReg(struct device *dev, unsigned int reg);
static bool I2sTdmVolatileReg(struct device *dev, unsigned int reg);
static bool I2sTdmPreciousReg(struct device *dev, unsigned int reg);
static int i2sTdmRuntimeSuspend(struct I2sTdmDev *i2sTdm);
static int I2sTdmProbe(struct platform_device *pdev);
static int I2sTdmRemove(struct platform_device *pdev);
static int I2sTdmInitProperties(struct I2sTdmDev *i2sTdm,
    struct device_node *node);
static int I2sTdmInitResets(struct I2sTdmDev *i2sTdm,
    struct platform_device *pdev);
static int I2sTdmInitClocks(struct I2sTdmDev *i2sTdm,
    struct platform_device *pdev);
static int I2sTdmInitRegmap(struct I2sTdmDev *i2sTdm,
    struct platform_device *pdev);
static void I2sTdmInitRegs(struct I2sTdmDev *i2sTdm);

static const struct reg_default I2sTdmRegDefaults[] = {
    {0x00, 0x7200000f},
    {0x04, 0x01c8000f},
    {0x08, 0x00001f1f},
    {0x10, 0x001f0000},
    {0x14, 0x01f00000},
    {0x30, 0x00003eff},
    {0x34, 0x00003eff},
    {0x38, 0x00000707},
};

static const struct regmap_config I2sTdmRegmapConfig = {
    .reg_bits = 32,
    .reg_stride = 4,
    .val_bits = 32,
    .max_register = I2S_CLKDIV,
    .reg_defaults = I2sTdmRegDefaults,
    .num_reg_defaults = ARRAY_SIZE(I2sTdmRegDefaults),
    .writeable_reg = I2sTdmWrReg,
    .readable_reg = I2sTdmRdReg,
    .volatile_reg = I2sTdmVolatileReg,
    .precious_reg = I2sTdmPreciousReg,
    .cache_type = REGCACHE_FLAT,
};

static const struct of_device_id I2sTdmMatch[] = {
    { .compatible = "rockchip,rk3588-i2s-tdm", },
    {},
};

static struct platform_driver I2sTdmDriver = {
    .probe = I2sTdmProbe,
    .remove = I2sTdmRemove,
    .driver = {
        .name = DRV_NAME,
        .of_match_table = of_match_ptr(I2sTdmMatch),
        .pm = NULL,
    },
};

static bool I2sTdmPreciousReg(struct device *dev, unsigned int reg)
{
    (void)dev;

    switch (reg) {
        case I2S_RXDR:
            return true;
        default:
            return false;
    }
}

static int I2sTdmRemove(struct platform_device *pdev)
{
    struct I2sTdmDev *i2sTdm = dev_get_drvdata(&pdev->dev);

    if (i2sTdm == NULL) {
        return -EINVAL;
    }

    i2sTdmRuntimeSuspend(i2sTdm);

    if (!IS_ERR(i2sTdm->mclkTx)) {
        clk_prepare_enable(i2sTdm->mclkTx);
    }
    if (!IS_ERR(i2sTdm->mclkRx)) {
        clk_prepare_enable(i2sTdm->mclkRx);
    }
    if (!IS_ERR(i2sTdm->hclk)) {
        clk_disable_unprepare(i2sTdm->hclk);
    }

    return 0;
}

static bool I2sTdmWrReg(struct device *dev, unsigned int reg)
{
    (void)dev;

    switch (reg) {
        case I2S_TXCR:
        case I2S_RXCR:
        case I2S_CKR:
        case I2S_DMACR:
        case I2S_INTCR:
        case I2S_XFER:
        case I2S_CLR:
        case I2S_TXDR:
        case I2S_TDM_TXCR:
        case I2S_TDM_RXCR:
        case I2S_CLKDIV:
            return true;
        default:
            return false;
    }
}

static int i2sTdmRuntimeSuspend(struct I2sTdmDev *i2sTdm)
{
    if (i2sTdm == NULL) {
        return -EINVAL;
    }

    regcache_cache_only(i2sTdm->regmap, true);
    if (!IS_ERR(i2sTdm->mclkTx)) {
        clk_disable_unprepare(i2sTdm->mclkTx);
    }
    if (!IS_ERR(i2sTdm->mclkRx)) {
        clk_disable_unprepare(i2sTdm->mclkRx);
    }

    return 0;
}

static bool I2sTdmVolatileReg(struct device *dev, unsigned int reg)
{
    (void)dev;

    switch (reg) {
        case I2S_TXFIFOLR:
        case I2S_INTSR:
        case I2S_CLR:
        case I2S_TXDR:
        case I2S_RXDR:
        case I2S_RXFIFOLR:
            return true;
        default:
            return false;
    }
}

static int I2sTdmInitProperties(struct I2sTdmDev *i2sTdm,
    struct device_node *node)
{
    const int defaultBclkFs = 64;
    const int minBclkFs = 32;
    const int clkTrcmMin = 0;
    const int clkTrcmMax = 2;
    int val;

    i2sTdm->bclkFs = defaultBclkFs;
    if (!of_property_read_u32(node, "rockchip,bclk-fs", &val)) {
        if ((val >= minBclkFs) && (val % 2 == 0)) {
            i2sTdm->bclkFs = val;
        }
    }

    i2sTdm->clkTrcm = I2S_CKR_TRCM_TXRX;
    if (!of_property_read_u32(node, "rockchip,clk-trcm", &val)) {
        if (val >= clkTrcmMin && val <= clkTrcmMax) {
            i2sTdm->clkTrcm = val << I2S_CKR_TRCM_SHIFT;
        }
    }

    return 0;
}

static int I2sTdmInitResets(struct I2sTdmDev *i2sTdm,
    struct platform_device *pdev)
{
    int ret;

    i2sTdm->txReset = devm_reset_control_get(&pdev->dev, "tx-m");
    if (IS_ERR(i2sTdm->txReset)) {
        ret = PTR_ERR(i2sTdm->txReset);
        if (ret != -ENOENT) {
            return ret;
        }
    }

    i2sTdm->rxReset = devm_reset_control_get(&pdev->dev, "rx-m");
    if (IS_ERR(i2sTdm->rxReset)) {
        ret = PTR_ERR(i2sTdm->rxReset);
        if (ret != -ENOENT) {
            return ret;
        }
    }

    return 0;
}

static int I2sTdmInitClocks(struct I2sTdmDev *i2sTdm,
    struct platform_device *pdev)
{
    int ret;

    i2sTdm->hclk = devm_clk_get(&pdev->dev, "hclk");
    if (IS_ERR(i2sTdm->hclk)) {
        return PTR_ERR(i2sTdm->hclk);
    }

    ret = clk_prepare_enable(i2sTdm->hclk);
    if (ret) {
        return ret;
    }

    i2sTdm->mclkTx = devm_clk_get(&pdev->dev, "mclk_tx");
    if (IS_ERR(i2sTdm->mclkTx)) {
        return PTR_ERR(i2sTdm->mclkTx);
    }

    i2sTdm->mclkRx = devm_clk_get(&pdev->dev, "mclk_rx");
    if (IS_ERR(i2sTdm->mclkRx)) {
        return PTR_ERR(i2sTdm->mclkRx);
    }

    return 0;
}

static int I2sTdmInitRegmap(struct I2sTdmDev *i2sTdm,
    struct platform_device *pdev)
{
    struct resource *res;
    void __iomem *regs;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (res == NULL) {
        return -ENODEV;
    }

    regs = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(regs)) {
        return PTR_ERR(regs);
    }

    i2sTdm->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
        &I2sTdmRegmapConfig);
    if (IS_ERR(i2sTdm->regmap)) {
        return PTR_ERR(i2sTdm->regmap);
    }

    return 0;
}

static void I2sTdmInitRegs(struct I2sTdmDev *i2sTdm)
{
    const int fifoDepth = 16;

    regmap_update_bits(i2sTdm->regmap, I2S_DMACR, I2S_DMACR_TDL_MASK,
        I2sDmacrTdl(fifoDepth));
    regmap_update_bits(i2sTdm->regmap, I2S_DMACR, I2S_DMACR_RDL_MASK,
        I2sDmacrRdl(fifoDepth));
    regmap_update_bits(i2sTdm->regmap, I2S_CKR, I2S_CKR_TRCM_MASK,
        i2sTdm->clkTrcm);
}

static int I2sTdmProbe(struct platform_device *pdev)
{
    struct device_node *node = pdev->dev.of_node;
    struct I2sTdmDev *i2sTdm;
    struct device *tempI2sDev;
    int ret;

    tempI2sDev = &pdev->dev;
    AUDIO_DEVICE_LOG_INFO("match dmaDevice->name %s ", dev_name(tempI2sDev));
    if (strcmp(dev_name(tempI2sDev), "fe470000.i2s") != 0) {
        AUDIO_DRIVER_LOG_INFO("failed dmaDevice->name %s",
            dev_name(tempI2sDev));
        return 0;
    }

    i2sTdm = devm_kzalloc(&pdev->dev, sizeof(*i2sTdm), GFP_KERNEL);
    if (i2sTdm == NULL) {
        return -ENOMEM;
    }

    i2sTdm->dev = &pdev->dev;
    spin_lock_init(&i2sTdm->lock);

    ret = I2sTdmInitProperties(i2sTdm, node);
    if (ret) {
		AUDIO_DRIVER_LOG_ERR("failed to init properties");
        return ret;
    }

    ret = I2sTdmInitResets(i2sTdm, pdev);
    if (ret) {
		AUDIO_DRIVER_LOG_ERR("failed to init resets");
        return ret;
    }

    ret = I2sTdmInitClocks(i2sTdm, pdev);
    if (ret) {
		AUDIO_DRIVER_LOG_ERR("failed to init clocks");
        return ret;
    }

    ret = I2sTdmInitRegmap(i2sTdm, pdev);
    if (ret) {
		AUDIO_DRIVER_LOG_ERR("failed to init regmap");
        return ret;
    }

    atomic_set(&i2sTdm->refcount, 0);
    dev_set_drvdata(&pdev->dev, i2sTdm);

    I2sTdmInitRegs(i2sTdm);
    return 0;
}

static bool I2sTdmRdReg(struct device *dev, unsigned int reg)
{
    (void)dev;

    switch (reg) {
        case I2S_TXCR:
        case I2S_RXCR:
        case I2S_CKR:
        case I2S_DMACR:
        case I2S_INTCR:
        case I2S_XFER:
        case I2S_CLR:
        case I2S_TXDR:
        case I2S_RXDR:
        case I2S_TXFIFOLR:
        case I2S_INTSR:
        case I2S_RXFIFOLR:
        case I2S_TDM_TXCR:
        case I2S_TDM_RXCR:
        case I2S_CLKDIV:
            return true;
        default:
            return false;
    }
}

module_platform_driver(I2sTdmDriver);