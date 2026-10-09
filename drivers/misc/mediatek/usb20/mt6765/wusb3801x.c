// SPDX-License-Identifier: GPL-2.0
/*
 * WUSB3801 CC controller support for the Lenovo TB-X606F.
 * Register setup derived from the Lenovo 4.9 driver:
 * Copyright (c) 2019 WillSemi Inc.; author lei.huang (lhuang@sh-willsemi.com).
 *
 * The controller handles attach detection and role selection autonomously
 * and the board routes the role indication to MUSB IDDIG. The stock board
 * names EINT 152 as its interrupt, but that line has no pin on MT6765 and
 * never fires on the Android 10 firmware, so only configure the chip.
 */
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>

#define WUSB_ID			0x01
#define WUSB_CONTROL0		0x02
#define WUSB_INTERRUPT		0x03
#define WUSB_STATUS		0x04
#define WUSB_CONTROL1		0x05
#define WUSB_MAX_REG		0x18

#define WUSB_VENDOR_MASK	0x07
#define WUSB_VENDOR_WILLSEMI	0x06
#define WUSB_MODE_MASK		0xe6
#define WUSB_MODE_SNK		0x80	/* sink only, accessories disabled */
#define WUSB_CURRENT_MASK	0x18
#define WUSB_CURRENT_3A		(2 << 3)
#define WUSB_INT_MASK		BIT(0)
#define WUSB_SM_RST		BIT(0)
#define WUSB_STATUS_VBUS	BIT(7)
#define WUSB_STATUS_CURRENT	0x60
#define WUSB_STATUS_TYPE	0x1c
#define WUSB_STATUS_CC		0x03

static int wusb3801_status(struct device *dev, unsigned int *status)
{
	struct regmap *map = dev_get_drvdata(dev);

	return regmap_read(map, WUSB_STATUS, status);
}

static ssize_t wcc_status_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	unsigned int status;
	int ret, cc;

	ret = wusb3801_status(dev, &status);
	if (ret)
		return ret;
	/* Stock ABI: 0 for CC1, 1 for CC2, -1 without a determined polarity. */
	cc = status & WUSB_STATUS_CC;
	return scnprintf(buf, PAGE_SIZE, "cc_sts (%d)\n",
			 cc == 1 || cc == 2 ? cc - 1 : -1);
}
static DEVICE_ATTR_RO(wcc_status);

static ssize_t wtype_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	static const char * const names[] = {
		"NOTYPE", "SINK", "SOURCE", "AUDIOACC", "DEBUGACC",
	};
	unsigned int status, type, index;
	int ret;

	ret = wusb3801_status(dev, &status);
	if (ret)
		return ret;
	type = status & WUSB_STATUS_TYPE;
	index = type >> 2;
	if (index >= ARRAY_SIZE(names))
		index = 0;
	return scnprintf(buf, PAGE_SIZE, "%s(%u)\n", names[index], type);
}
static DEVICE_ATTR_RO(wtype);

static ssize_t wregdump_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct regmap *map = dev_get_drvdata(dev);
	unsigned int reg, value;
	int ret, len = 0;

	for (reg = WUSB_ID; reg <= WUSB_MAX_REG; reg++) {
		/* Reading the interrupt register clears it; leave it alone. */
		if (reg == WUSB_INTERRUPT)
			continue;
		ret = regmap_read(map, reg, &value);
		if (ret)
			return ret;
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "from 0x%02x read 0x%02x\n", reg, value);
	}
	return len;
}
static DEVICE_ATTR_RO(wregdump);

static struct attribute *wusb3801_attrs[] = {
	&dev_attr_wcc_status.attr,
	&dev_attr_wtype.attr,
	&dev_attr_wregdump.attr,
	NULL,
};
ATTRIBUTE_GROUPS(wusb3801);

static const struct regmap_config wusb3801_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = WUSB_MAX_REG,
};

static int wusb3801_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	unsigned int version, status, pending;
	struct regmap *map;
	u32 mode = 0x24;
	int ret;

	map = devm_regmap_init_i2c(client, &wusb3801_regmap_config);
	if (IS_ERR(map))
		return PTR_ERR(map);
	ret = regmap_read(map, WUSB_ID, &version);
	if (ret)
		return ret;
	if ((version & WUSB_VENDOR_MASK) != WUSB_VENDOR_WILLSEMI)
		return -ENODEV;
	of_property_read_u32(dev->of_node, "wusb3801,init-mode", &mode);
	if (mode & ~WUSB_MODE_MASK || (mode & 6) == 6 || (mode & 0x60) == 0x60)
		return -EINVAL;
	dev_set_drvdata(dev, map);

	/*
	 * As the stock driver does: advertise 3 A as a source (a 3 A capable
	 * DRP partner without VBUS is otherwise misdetected as a sink), select
	 * the board mode with the chip interrupt masked, acknowledge stale
	 * events and restart detection unless a source is already attached.
	 */
	ret = regmap_update_bits(map, WUSB_CONTROL0,
				 WUSB_MODE_MASK | WUSB_CURRENT_MASK | WUSB_INT_MASK,
				 mode | WUSB_CURRENT_3A | WUSB_INT_MASK);
	if (!ret)
		ret = regmap_write(map, WUSB_CONTROL1, 0);
	if (!ret)
		ret = regmap_read(map, WUSB_INTERRUPT, &pending);
	if (!ret)
		ret = regmap_read(map, WUSB_STATUS, &status);
	if (!ret && (!(status & WUSB_STATUS_VBUS) ||
		     !(status & WUSB_STATUS_CURRENT)))
		ret = regmap_write(map, WUSB_CONTROL1, WUSB_SM_RST);
	if (ret)
		return ret;

	ret = devm_device_add_groups(dev, wusb3801_groups);
	if (ret)
		return ret;
	dev_info(dev, "WUSB3801 id %#x, mode %#x, status %#x\n",
		 version, mode, status);
	return 0;
}

/* Present only a sink while powered off so that charging still works. */
static void wusb3801_shutdown(struct i2c_client *client)
{
	struct regmap *map = dev_get_drvdata(&client->dev);

	regmap_update_bits(map, WUSB_CONTROL0, WUSB_MODE_MASK, WUSB_MODE_SNK);
	regmap_write(map, WUSB_CONTROL1, 0);
}

static const struct of_device_id wusb3801_of_match[] = {
	{ .compatible = "mediatek,wusb3801x" },
	{ }
};
MODULE_DEVICE_TABLE(of, wusb3801_of_match);

static const struct i2c_device_id wusb3801_ids[] = {
	{ "wusb3801x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wusb3801_ids);

static struct i2c_driver wusb3801_driver = {
	.driver = {
		.name = "wusb3801x",
		.of_match_table = wusb3801_of_match,
	},
	.probe = wusb3801_probe,
	.shutdown = wusb3801_shutdown,
	.id_table = wusb3801_ids,
};
module_i2c_driver(wusb3801_driver);

MODULE_DESCRIPTION("WUSB3801 CC controller for Lenovo TB-X606F");
MODULE_LICENSE("GPL v2");
