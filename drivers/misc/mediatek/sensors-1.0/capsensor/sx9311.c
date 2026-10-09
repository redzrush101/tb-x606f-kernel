// SPDX-License-Identifier: GPL-2.0
/*
 * SX9311 SAR input support for Lenovo TB-X606F Android 10.
 * Register sequencing follows the GPL SX9310 driver in Lenovo's 4.9 drop.
 * Board tuning and key mapping match the captured S301068 stock kernel.
 */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/regmap.h>

#define SX9311_IRQSTAT	0x00
#define SX9311_STAT0	0x01
#define SX9311_WHOAMI	0x42
#define SX9311_RESET	0x7f

struct sx9311_data {
	struct regmap *regmap;
	struct input_dev *input;
	struct mutex lock;
};

static const struct reg_sequence sx9311_init_regs[] = {
	{ 0x03, 0x70 }, { 0x04, 0x00 }, { 0x11, 0x00 }, { 0x12, 0x0b },
	{ 0x13, 0x0f }, { 0x14, 0x0f }, { 0x15, 0xc3 }, { 0x16, 0x20 },
	{ 0x17, 0x4d }, { 0x18, 0x20 }, { 0x19, 0x38 }, { 0x1a, 0x10 },
	{ 0x1b, 0x00 }, { 0x1c, 0x00 }, { 0x1d, 0x00 }, { 0x1e, 0x00 },
	{ 0x1f, 0x00 }, { 0x20, 0x00 }, { 0x21, 0x18 }, { 0x22, 0x16 },
	{ 0x23, 0x2b }, { 0x2a, 0x01 }, { 0x2b, 0x80 }, { 0x2c, 0x0c },
	{ 0x10, 0x27 },
};

static const unsigned int sx9311_keys[] = {
	KEY_ARCHIVE, KEY_VENDOR, KEY_TIME, KEY_INFO,
};

static int sx9311_report(struct sx9311_data *data)
{
	unsigned int status;
	int ret, i;

	ret = regmap_read(data->regmap, SX9311_STAT0, &status);
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(sx9311_keys); i++)
		input_report_key(data->input, sx9311_keys[i], !!(status & BIT(i)));
	input_sync(data->input);
	return 0;
}

static int sx9311_init(struct sx9311_data *data)
{
	unsigned int status;
	int ret;

	ret = regmap_write(data->regmap, SX9311_RESET, 0xde);
	if (ret)
		return ret;
	msleep(300);
	ret = regmap_multi_reg_write(data->regmap, sx9311_init_regs,
				   ARRAY_SIZE(sx9311_init_regs));
	if (ret)
		return ret;
	msleep(300);
	ret = regmap_write(data->regmap, SX9311_IRQSTAT, 0xff);
	if (ret)
		return ret;
	msleep(300);
	ret = regmap_read(data->regmap, SX9311_IRQSTAT, &status);
	if (ret)
		return ret;
	return sx9311_report(data);
}

static irqreturn_t sx9311_irq_thread(int irq, void *arg)
{
	struct sx9311_data *data = arg;
	unsigned int status;
	int ret;

	mutex_lock(&data->lock);
	ret = regmap_read(data->regmap, SX9311_IRQSTAT, &status);
	if (!ret)
		ret = sx9311_report(data);
	mutex_unlock(&data->lock);
	if (ret)
		dev_err_ratelimited(data->input->dev.parent,
				    "SAR interrupt read failed: %d\n", ret);
	return IRQ_HANDLED;
}

static ssize_t chipinfo_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct sx9311_data *data = dev_get_drvdata(dev);
	unsigned int id;
	int ret = regmap_read(data->regmap, SX9311_WHOAMI, &id);

	return ret ? ret : scnprintf(buf, PAGE_SIZE, "WHOAMI:%u\n", id);
}
static DEVICE_ATTR_RO(chipinfo);

static ssize_t manual_calibrate_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct sx9311_data *data = dev_get_drvdata(dev);
	unsigned int status;
	int ret;

	mutex_lock(&data->lock);
	ret = regmap_read(data->regmap, SX9311_IRQSTAT, &status);
	mutex_unlock(&data->lock);
	return ret ? ret : scnprintf(buf, PAGE_SIZE, "%u\n", status);
}

static ssize_t manual_calibrate_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct sx9311_data *data = dev_get_drvdata(dev);
	unsigned long value;
	int ret;

	ret = kstrtoul(buf, 0, &value);
	if (ret)
		return ret;
	if (!value)
		return count;
	mutex_lock(&data->lock);
	ret = regmap_write(data->regmap, SX9311_IRQSTAT, 0xff);
	mutex_unlock(&data->lock);
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(manual_calibrate);

static ssize_t register_read_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct sx9311_data *data = dev_get_drvdata(dev);
	unsigned int value;
	ssize_t len = 0;
	int reg, ret;

	mutex_lock(&data->lock);
	for (reg = 0x10; reg <= 0x2c; reg++) {
		ret = regmap_read(data->regmap, reg, &value);
		if (ret) {
			len = ret;
			break;
		}
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "Reg[0x%02X]: 0x%02X\n", reg, value);
	}
	mutex_unlock(&data->lock);
	return len;
}
static DEVICE_ATTR_RO(register_read);

static struct attribute *sx9311_attrs[] = {
	&dev_attr_chipinfo.attr,
	&dev_attr_manual_calibrate.attr,
	&dev_attr_register_read.attr,
	NULL,
};
ATTRIBUTE_GROUPS(sx9311);

static const struct regmap_config sx9311_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = SX9311_RESET,
};

static int sx9311_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct sx9311_data *data;
	unsigned int chipid;
	int ret, i;

	if (client->irq <= 0)
		return -EINVAL;
	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	mutex_init(&data->lock);
	data->regmap = devm_regmap_init_i2c(client, &sx9311_regmap_config);
	if (IS_ERR(data->regmap))
		return PTR_ERR(data->regmap);
	ret = regmap_read(data->regmap, SX9311_WHOAMI, &chipid);
	if (ret)
		return ret;
	if (chipid != 2)
		return -ENODEV;

	data->input = devm_input_allocate_device(&client->dev);
	if (!data->input)
		return -ENOMEM;
	data->input->name = "sx9310 Cap Touch";
	data->input->id.bustype = BUS_I2C;
	data->input->dev.parent = &client->dev;
	for (i = 0; i < ARRAY_SIZE(sx9311_keys); i++)
		input_set_capability(data->input, EV_KEY, sx9311_keys[i]);
	ret = input_register_device(data->input);
	if (ret)
		return ret;
	i2c_set_clientdata(client, data);
	ret = sx9311_init(data);
	if (ret)
		return ret;
	ret = devm_request_threaded_irq(&client->dev, client->irq, NULL,
				       sx9311_irq_thread, IRQF_ONESHOT,
				       dev_name(&client->dev), data);
	if (ret)
		return ret;
	ret = devm_device_add_groups(&client->dev, sx9311_groups);
	if (!ret)
		dev_info(&client->dev, "SX9311 SAR ready, IRQ %d\n", client->irq);
	return ret;
}

static int __maybe_unused sx9311_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	disable_irq(client->irq);
	return 0;
}

static int __maybe_unused sx9311_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sx9311_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->lock);
	ret = sx9311_init(data);
	mutex_unlock(&data->lock);
	if (!ret)
		enable_irq(client->irq);
	return ret;
}

static SIMPLE_DEV_PM_OPS(sx9311_pm_ops, sx9311_suspend, sx9311_resume);

static const struct of_device_id sx9311_of_match[] = {
	{ .compatible = "mediatek,sx9311" },
	{ }
};
MODULE_DEVICE_TABLE(of, sx9311_of_match);

static const struct i2c_device_id sx9311_ids[] = {
	{ "sx9311", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sx9311_ids);

static struct i2c_driver sx9311_driver = {
	.driver = {
		.name = "sx9311",
		.of_match_table = sx9311_of_match,
		.pm = &sx9311_pm_ops,
	},
	.probe = sx9311_probe,
	.id_table = sx9311_ids,
};
module_i2c_driver(sx9311_driver);

MODULE_DESCRIPTION("Lenovo TB-X606F SX9311 SAR input driver");
MODULE_LICENSE("GPL v2");
