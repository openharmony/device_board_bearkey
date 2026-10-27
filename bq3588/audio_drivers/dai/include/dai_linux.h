/*
 * Copyright © 2015-2026 Xiamen Bearkey Technology Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */

#ifndef DAI_LINUX_H
#define DAI_LINUX_H

#include <linux/clk.h>
#include <linux/clk/rockchip.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/spinlock.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm_params.h>

/* I2S REGS */
#define I2S_TXCR        (0x0000)
#define I2S_RXCR        (0x0004)
#define I2S_CKR         (0x0008)
#define I2S_TXFIFOLR    (0x000c)
#define I2S_DMACR       (0x0010)
#define I2S_INTCR       (0x0014)
#define I2S_INTSR       (0x0018)
#define I2S_XFER        (0x001c)
#define I2S_CLR         (0x0020)
#define I2S_TXDR        (0x0024)
#define I2S_RXDR        (0x0028)
#define I2S_RXFIFOLR    (0x002c)
#define I2S_TDM_TXCR    (0x0030)
#define I2S_TDM_RXCR    (0x0034)
#define I2S_CLKDIV      (0x0038)

/* XFER: transfer start register */
#define I2S_XFER_RXS_SHIFT  1
#define I2S_XFER_RXS_STOP   (0 << I2S_XFER_RXS_SHIFT)
#define I2S_XFER_RXS_START  (1 << I2S_XFER_RXS_SHIFT)
#define I2S_XFER_TXS_SHIFT  0
#define I2S_XFER_TXS_STOP   (0 << I2S_XFER_TXS_SHIFT)
#define I2S_XFER_TXS_START  (1 << I2S_XFER_TXS_SHIFT)

/* CLR: clear SCLK domain logic register */
#define I2S_CLR_RXC         BIT(1)
#define I2S_CLR_TXC         BIT(0)

/* DMACR: DMA control register */
#define I2S_DMACR_RDE_SHIFT     24
#define I2S_DMACR_RDE_DISABLE   (0 << I2S_DMACR_RDE_SHIFT)
#define I2S_DMACR_RDE_ENABLE    (1 << I2S_DMACR_RDE_SHIFT)
#define I2S_DMACR_RDL_SHIFT     16
#define I2S_DMACR_RDL_MASK      (0x1f << I2S_DMACR_RDL_SHIFT)
#define I2S_DMACR_TDE_SHIFT     8
#define I2S_DMACR_TDE_DISABLE   (0 << I2S_DMACR_TDE_SHIFT)
#define I2S_DMACR_TDE_ENABLE    (1 << I2S_DMACR_TDE_SHIFT)
#define I2S_DMACR_TDL_SHIFT     0
#define I2S_DMACR_TDL_MASK      (0x1f << I2S_DMACR_TDL_SHIFT)

/* CKR: clock generation register */
#define I2S_CKR_TRCM_SHIFT      28
#define I2S_CKR_TRCM_TXRX       (0 << I2S_CKR_TRCM_SHIFT)
#define I2S_CKR_TRCM_TXONLY     (1 << I2S_CKR_TRCM_SHIFT)
#define I2S_CKR_TRCM_RXONLY     (2 << I2S_CKR_TRCM_SHIFT)
#define I2S_CKR_TRCM_MASK       (3 << I2S_CKR_TRCM_SHIFT)
#define I2S_CKR_RSD_SHIFT       8
#define I2S_CKR_RSD_MASK        (0xff << I2S_CKR_RSD_SHIFT)
#define I2S_CKR_TSD_SHIFT       0
#define I2S_CKR_TSD_MASK        (0xff << I2S_CKR_TSD_SHIFT)

/* TXCR */
#define I2S_TXCR_TFS_SHIFT      5
#define I2S_TXCR_TFS_I2S        (0 << I2S_TXCR_TFS_SHIFT)
#define I2S_TXCR_TFS_PCM        (1 << I2S_TXCR_TFS_SHIFT)
#define I2S_TXCR_TFS_TDM_PCM    (2 << I2S_TXCR_TFS_SHIFT)
#define I2S_TXCR_TFS_TDM_I2S    (3 << I2S_TXCR_TFS_SHIFT)
#define I2S_TXCR_TFS_MASK       (3 << I2S_TXCR_TFS_SHIFT)
#define I2S_TXCR_CSR_SHIFT      15
#define I2S_TXCR_CSR_MASK       (3 << I2S_TXCR_CSR_SHIFT)
#define I2S_TXCR_VDW_SHIFT      0
#define I2S_TXCR_VDW_MASK       (0x1f << I2S_TXCR_VDW_SHIFT)

/* RXCR */
#define I2S_RXCR_CSR_SHIFT      15
#define I2S_RXCR_CSR_MASK       (3 << I2S_RXCR_CSR_SHIFT)
#define I2S_RXCR_VDW_SHIFT      0
#define I2S_RXCR_VDW_MASK       (0x1f << I2S_RXCR_VDW_SHIFT)

/* CLKDIV: Mclk div register */
#define I2S_CLKDIV_TXM_SHIFT    0
#define I2S_CLKDIV_TXM_MASK     (0xff << I2S_CLKDIV_TXM_SHIFT)
#define I2S_CLKDIV_RXM_SHIFT    8
#define I2S_CLKDIV_RXM_MASK     (0xff << I2S_CLKDIV_RXM_SHIFT)

/* channel select */
#define I2S_CSR_SHIFT           15
#define I2S_CHN_2               (0 << I2S_CSR_SHIFT)
#define I2S_CHN_4               (1 << I2S_CSR_SHIFT)
#define I2S_CHN_6               (2 << I2S_CSR_SHIFT)
#define I2S_CHN_8               (3 << I2S_CSR_SHIFT)

/*
 * Register field helpers.
 * G.PRE.02-CPP: use static inline functions instead of function-like macros.
 */

static inline unsigned int I2sDmacrRdl(unsigned int x)
{
    return ((x) - 1) << I2S_DMACR_RDL_SHIFT;
}

static inline unsigned int I2sDmacrTdl(unsigned int x)
{
    return x << I2S_DMACR_TDL_SHIFT;
}

static inline unsigned int I2sCkrTrcm(unsigned int x)
{
    return x << I2S_CKR_TRCM_SHIFT;
}

static inline unsigned int I2sCkrRsd(unsigned int x)
{
    return ((x) - 1) << I2S_CKR_RSD_SHIFT;
}

static inline unsigned int I2sCkrTsd(unsigned int x)
{
    return ((x) - 1) << I2S_CKR_TSD_SHIFT;
}

static inline unsigned int I2sTxcrCsr(unsigned int x)
{
    return x << I2S_TXCR_CSR_SHIFT;
}

static inline unsigned int I2sTxcrVdw(unsigned int x)
{
    return ((x) - 1) << I2S_TXCR_VDW_SHIFT;
}

static inline unsigned int I2sRxcrCsr(unsigned int x)
{
    return x << I2S_RXCR_CSR_SHIFT;
}

static inline unsigned int I2sRxcrVdw(unsigned int x)
{
    return ((x) - 1) << I2S_RXCR_VDW_SHIFT;
}

static inline unsigned int I2sClkdivTxm(unsigned int x)
{
    return ((x) - 1) << I2S_CLKDIV_TXM_SHIFT;
}

static inline unsigned int I2sClkdivRxm(unsigned int x)
{
    return ((x) - 1) << I2S_CLKDIV_RXM_SHIFT;
}

struct I2sTdmDev {
    struct device *dev;
    struct clk *hclk;
    struct clk *mclkTx;
    struct clk *mclkRx;
    struct regmap *regmap;
    struct reset_control *txReset;
    struct reset_control *rxReset;
    unsigned int mclkRxFreq;
    unsigned int mclkTxFreq;
    unsigned int bclkFs;
    unsigned int clkTrcm;
    atomic_t refcount;
    spinlock_t lock;
    bool txStart;
    bool rxStart;
};

#endif /* DAI_LINUX_H */