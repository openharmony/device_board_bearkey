/*
 * Copyright © 2015-2026 Xiamen Bearkey Technology Co., Ltd.
 *
 * HDF is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 * See the LICENSE file in the root of this repository for complete details.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include "es8388_impl.h"

static const struct regmap_config es8388RegmapConfig = {
    .reg_bits	= 8,
    .val_bits	= 8,
    .max_register	= ES8388_DACCONTROL30,
    .cache_type	= REGCACHE_RBTREE,
    .use_single_read = true,
    .use_single_write = true,
};

struct i2c_client *I2cGetClientDataByName(const char *nodeName)
{
    struct device_node *node;
    struct i2c_client *client = NULL;

    node = of_find_node_by_name(NULL, nodeName);
    if (!node) {
        pr_err("Cannot find node %s\n", nodeName);
        return NULL;
    }

    client = of_find_i2c_device_by_node(node);
    of_node_put(node);

    return client;
}

struct Es8388Priv *GetCodecDevice(void)
{
    struct i2c_client *client = I2cGetClientDataByName("es8388");
    if (!client) {
        return NULL;
    }

    return i2c_get_clientdata(client);
}

static irqreturn_t HpDetIrqHandler(int irq, void *devId)
{
    struct Es8388Priv *es8388 = (struct Es8388Priv *)devId;

    es8388->hpState = gpio_get_value(es8388->headsetGpio);

    return IRQ_HANDLED;
}

static int HpDetInit(struct Es8388Priv *es8388, struct device *dev)
{
    int ret;
    struct device_node *np = dev->of_node;

    es8388->headsetGpio = of_get_named_gpio(np, "headset_gpio", 0);
    if (es8388->headsetGpio < 0) {
        dev_err(dev, "Failed to get headsetGpio from DT\n");
        return -EINVAL;
    }

    if (!gpio_is_valid(es8388->headsetGpio)) {
        dev_err(dev, "Invalid headsetGpio: %d\n", es8388->headsetGpio);
        return -EINVAL;
    }

    ret = devm_gpio_request_one(dev, es8388->headsetGpio,
                                 GPIOF_DIR_IN, "hp-det");
    if (ret) {
        dev_err(dev, "Failed to request headsetGpio: %d\n", ret);
        return ret;
    }

    es8388->hpDetIrq = gpio_to_irq(es8388->headsetGpio);
    if (es8388->hpDetIrq < 0) {
        dev_err(dev, "Failed to get hpDetIrq IRQ\n");
        return es8388->hpDetIrq;
    }

    ret = devm_request_irq(dev, es8388->hpDetIrq,
                           HpDetIrqHandler,
                           IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                           "hpDetIrq", es8388);
    if (ret) {
        dev_err(dev, "Failed to request hpDetIrq IRQ: %d\n", ret);
        return ret;
    }

    es8388->hpState = gpio_get_value(es8388->headsetGpio);

    return 0;
}

static int Es8388I2cProbe(struct i2c_client *i2c)
{
    int ret = -1;
    struct Es8388Priv *es8388;
    struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);

    if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
        dev_warn(&adapter->dev,
                 "I2C-Adapter doesn't support I2C_FUNC_I2C\n");
        return -EIO;
    }

    es8388 = devm_kzalloc(&i2c->dev, sizeof(struct Es8388Priv), GFP_KERNEL);
    if (!es8388) {
        return -ENOMEM;
    }

    es8388->regMap = devm_regmap_init_i2c(i2c, &es8388RegmapConfig);
    if (IS_ERR(es8388->regMap)) {
        return PTR_ERR(es8388->regMap);
    }

    i2c_set_clientdata(i2c, es8388);

    ret = HpDetInit(es8388, &i2c->dev);
    if (ret) {
        dev_warn(&i2c->dev, "Headphone detection init failed: %d\n", ret);
    }

    return ret;
}

static void Es8388I2cRemove(struct i2c_client *client)
{
    struct Es8388Priv *es8388 = i2c_get_clientdata(client);
    if (es8388->hpDetIrq > 0) {
        devm_free_irq(&client->dev, es8388->hpDetIrq, es8388);
    }
    i2c_set_clientdata(client, NULL);

    return;
}

static const struct i2c_device_id es8388I2cId[] = {
    {"es8323", 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, es8388I2cId);

static const struct of_device_id es8388OfMatch[] = {
    { .compatible = "everest,es8323", },
    { }
};
MODULE_DEVICE_TABLE(of, es8388OfMatch);

static struct i2c_driver es8388I2cDriver = {
    .driver = {
        .name = "ES8388",
        .of_match_table = of_match_ptr(es8388OfMatch),
        },
    .probe = Es8388I2cProbe,
    .remove = Es8388I2cRemove,
    .id_table = es8388I2cId,
};
module_i2c_driver(es8388I2cDriver);
