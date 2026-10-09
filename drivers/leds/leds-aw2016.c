// SPDX-License-Identifier: GPL-2.0
/*
 * AWINIC AW2016 three-channel LED driver.
 * Register programming follows the Lenovo TB-X606F 4.9 driver:
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * Each channel is a LED class device in manual PWM mode; blinking uses
 * the LED timer trigger, which is what the Android lights HAL drives.
 */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>

#define AW2016_REG_RESET	0x00
#define AW2016_REG_GCR1		0x01
#define AW2016_REG_GCR2		0x04
#define AW2016_REG_LEDEN	0x30
#define AW2016_REG_LCFG1	0x31
#define AW2016_REG_PWM1		0x34

#define AW2016_CHIPID		0x09
#define AW2016_RESET		0x55
#define AW2016_CHIP_ENABLE	BIT(0)
#define AW2016_CHARGE_DISABLE	BIT(1)
#define AW2016_CHANNELS		3

struct aw2016;

struct aw2016_led {
	struct led_classdev cdev;
	struct aw2016 *chip;
	u32 id;
	u32 imax;
	u32 current_setting;
};

struct aw2016 {
	struct i2c_client *client;
	struct mutex lock;	/* serialises GCR1/GCR2/LEDEN updates */
	struct aw2016_led leds[AW2016_CHANNELS];
};

static int aw2016_write(struct aw2016 *chip, u8 reg, u8 val)
{
	return i2c_smbus_write_byte_data(chip->client, reg, val);
}

static int aw2016_set(struct led_classdev *cdev, enum led_brightness value)
{
	struct aw2016_led *led = container_of(cdev, struct aw2016_led, cdev);
	struct aw2016 *chip = led->chip;
	int enabled, gcr1, ret;

	mutex_lock(&chip->lock);
	gcr1 = i2c_smbus_read_byte_data(chip->client, AW2016_REG_GCR1);
	enabled = i2c_smbus_read_byte_data(chip->client, AW2016_REG_LEDEN);
	ret = gcr1 < 0 ? gcr1 : enabled < 0 ? enabled : 0;
	if (ret)
		goto out;
	if (value && !(gcr1 & AW2016_CHIP_ENABLE)) {
		ret = aw2016_write(chip, AW2016_REG_GCR1,
				   AW2016_CHARGE_DISABLE | AW2016_CHIP_ENABLE);
		if (ret)
			goto out;
		usleep_range(2000, 3000);
	}
	if (value) {
		/* The stock driver applies the channel's current range globally. */
		ret = aw2016_write(chip, AW2016_REG_GCR2, led->imax);
		if (!ret)
			ret = aw2016_write(chip, AW2016_REG_LCFG1 + led->id,
					   led->current_setting);
		if (!ret)
			ret = aw2016_write(chip, AW2016_REG_PWM1 + led->id, value);
		enabled |= BIT(led->id);
	} else {
		enabled &= ~BIT(led->id);
	}
	if (!ret)
		ret = aw2016_write(chip, AW2016_REG_LEDEN, enabled);
	/* Power the chip down once every channel is off. */
	if (!ret && !enabled)
		ret = aw2016_write(chip, AW2016_REG_GCR1, AW2016_CHARGE_DISABLE);
out:
	mutex_unlock(&chip->lock);
	return ret;
}

static int aw2016_parse_led(struct aw2016 *chip, struct device_node *np)
{
	struct device *dev = &chip->client->dev;
	struct aw2016_led *led;
	u32 id, max = LED_FULL;
	int ret;

	ret = of_property_read_u32(np, "aw2016,id", &id);
	if (ret || id >= AW2016_CHANNELS)
		return ret ?: -EINVAL;
	led = &chip->leds[id];
	if (led->chip)
		return -EEXIST;
	ret = of_property_read_string(np, "aw2016,name", &led->cdev.name);
	if (!ret)
		ret = of_property_read_u32(np, "aw2016,imax", &led->imax);
	if (!ret)
		ret = of_property_read_u32(np, "aw2016,led-current",
					   &led->current_setting);
	if (ret)
		return ret;
	of_property_read_u32(np, "aw2016,max-brightness", &max);
	if (led->imax > 3 || led->current_setting > 15 || !max || max > 255)
		return -EINVAL;
	led->chip = chip;
	led->id = id;
	led->cdev.max_brightness = max;
	led->cdev.brightness_set_blocking = aw2016_set;
	return devm_of_led_classdev_register(dev, np, &led->cdev);
}

static int aw2016_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *np;
	struct aw2016 *chip;
	int ret;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->client = client;
	mutex_init(&chip->lock);
	i2c_set_clientdata(client, chip);

	ret = i2c_smbus_read_byte_data(client, AW2016_REG_RESET);
	if (ret < 0)
		return ret;
	if (ret != AW2016_CHIPID) {
		dev_err(dev, "unexpected chip id %#x\n", ret);
		return -ENODEV;
	}
	/* Start with the channels off and the charge indicator mode disabled. */
	ret = aw2016_write(chip, AW2016_REG_GCR1, AW2016_CHARGE_DISABLE);
	if (ret)
		return ret;

	for_each_available_child_of_node(dev->of_node, np) {
		ret = aw2016_parse_led(chip, np);
		if (ret) {
			dev_err(dev, "%pOFn: %d\n", np, ret);
			of_node_put(np);
			return ret;
		}
	}
	return 0;
}

static void aw2016_shutdown(struct i2c_client *client)
{
	struct aw2016 *chip = i2c_get_clientdata(client);

	aw2016_write(chip, AW2016_REG_RESET, AW2016_RESET);
}

static const struct of_device_id aw2016_of_match[] = {
	{ .compatible = "awinic,aw2016_led" },
	{ }
};
MODULE_DEVICE_TABLE(of, aw2016_of_match);

static const struct i2c_device_id aw2016_ids[] = {
	{ "aw2016_led", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aw2016_ids);

static struct i2c_driver aw2016_driver = {
	.driver = {
		.name = "aw2016_led",
		.of_match_table = aw2016_of_match,
	},
	.probe = aw2016_probe,
	.shutdown = aw2016_shutdown,
	.id_table = aw2016_ids,
};
module_i2c_driver(aw2016_driver);

MODULE_DESCRIPTION("AWINIC AW2016 LED driver");
MODULE_LICENSE("GPL v2");
