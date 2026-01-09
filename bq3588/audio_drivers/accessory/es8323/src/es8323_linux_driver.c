/*
 * Copyright (C) 2022 HiHope Open Source Organization .
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
#include "es8323_codec_impl.h"

#define HDF_LOG_TAG "es8323_codec_linux"

static const struct regmap_config es8323_regmap_config = {
    .reg_bits	= 8,
    .val_bits	= 8,
    .max_register	= ES8323_DACCONTROL30,
    .cache_type	= REGCACHE_RBTREE,
    .use_single_read = true,
    .use_single_write = true,
};

struct i2c_client *i2c_get_clientdata_by_name(const char *node_name)
{
    struct device_node *node;
    struct i2c_client *client = NULL;

    node = of_find_node_by_name(NULL, node_name);
    if (!node) {
        pr_err("Cannot find node %s\n", node_name);
        return NULL;
    }

    client = of_find_i2c_device_by_node(node);
    of_node_put(node);

    return client;
}

struct es8323_priv *GetCodecDevice(void)
{
    struct i2c_client *client = i2c_get_clientdata_by_name("es8388");
    if (!client)
        return NULL;
    return i2c_get_clientdata(client);
}

static irqreturn_t hp_det_irq_handler(int irq, void *dev_id)
{
    struct es8323_priv *es8323 = (struct es8323_priv *)dev_id;

    es8323->hp_state = gpio_get_value(es8323->headset_gpio);

    return IRQ_HANDLED;
}

static int hp_det_init(struct es8323_priv *es8323, struct device *dev)
{
    int ret;
    struct device_node *np = dev->of_node;

    es8323->headset_gpio = of_get_named_gpio(np, "headset_gpio", 0);
    if (es8323->headset_gpio < 0) {
        dev_err(dev, "Failed to get headset_gpio from DT\n");
        return -EINVAL;
    }

    if (!gpio_is_valid(es8323->headset_gpio)) {
        dev_err(dev, "Invalid headset_gpio: %d\n", es8323->headset_gpio);
        return -EINVAL;
    }

    ret = devm_gpio_request_one(dev, es8323->headset_gpio,
                                 GPIOF_DIR_IN, "hp-det");
    if (ret) {
        dev_err(dev, "Failed to request headset_gpio: %d\n", ret);
        return ret;
    }

    es8323->hp_det_irq = gpio_to_irq(es8323->headset_gpio);
    if (es8323->hp_det_irq < 0) {
        dev_err(dev, "Failed to get hp-det IRQ\n");
        return es8323->hp_det_irq;
    }

    ret = devm_request_irq(dev, es8323->hp_det_irq,
                           hp_det_irq_handler,
                           IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                           "hp-det", es8323);
    if (ret) {
        dev_err(dev, "Failed to request hp-det IRQ: %d\n", ret);
        return ret;
    }

    es8323->hp_state = gpio_get_value(es8323->headset_gpio);

    return 0;
}

static int es8323_i2c_probe(struct i2c_client *i2c)
{
    int ret = -1;
    struct es8323_priv *es8323;
    struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);

    if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
        dev_warn(&adapter->dev,
                 "I2C-Adapter doesn't support I2C_FUNC_I2C\n");
    return -EIO;
    }

    es8323 = devm_kzalloc(&i2c->dev, sizeof(struct es8323_priv), GFP_KERNEL);
    if (!es8323)
        return -ENOMEM;

    es8323->regmap = devm_regmap_init_i2c(i2c, &es8323_regmap_config);
    if (IS_ERR(es8323->regmap))
        return PTR_ERR(es8323->regmap);

    i2c_set_clientdata(i2c, es8323);

    ret = hp_det_init(es8323, &i2c->dev);
    if (ret) {
        dev_warn(&i2c->dev, "Headphone detection init failed: %d\n", ret);
    }

    return ret;
}

static void es8323_i2c_remove(struct i2c_client *client)
{
    struct es8323_priv *es8323 = i2c_get_clientdata(client);
    if (es8323->hp_det_irq > 0) {
        devm_free_irq(&client->dev, es8323->hp_det_irq, es8323);
    }
    i2c_set_clientdata(client, NULL);

    return;
}

static const struct i2c_device_id es8323_i2c_id[] = {
    {"es8323", 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, es8323_i2c_id);

static const struct of_device_id es8323_of_match[] = {
    { .compatible = "everest,es8323", },
    { }
};
MODULE_DEVICE_TABLE(of, es8323_of_match);

static struct i2c_driver es8323_i2c_driver = {
    .driver = {
        .name = "ES8323",
        .of_match_table = of_match_ptr(es8323_of_match),
        },
    .probe = es8323_i2c_probe,
    .remove = es8323_i2c_remove,
    .id_table = es8323_i2c_id,
};
module_i2c_driver(es8323_i2c_driver);
