/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Google Whitechapel Audio Metrics Driver
 *
 * Copyright (c) 2021 Google LLC
 *
 */


#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/device.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "audiometrics.h"

#define DRIVER_NAME "audiometrics"

#define SPEAKER_MAX_COUNT 4
#define AUDIOMETRIC_CH_LENGTH 16
#define AMCS_MAX_MINOR (1U)
#define AMCS_CDEV_NAME "amcs"

static struct platform_device *amcs_pdev;

/*
 * DSP Speech Up/Down time and counts
 */
struct wdsp_stat_priv_type {
	ktime_t ktime_zero;
	ktime_t uptime;
	ktime_t crashtime;
	s64 total_uptime;
	s64 total_downtime;
	u64 crash_count;
	u64 recover_count;
	uint32_t action;
	struct mutex lock;
};

/*
 * ToDo: Reserve a fd handle for audio metric logging ring buffer.
 */
struct amcs_ion_handle_type {
	__u32 handle;
	__s32 fd;
};

 /*
  * Audio Suez related metrics
  */
struct audio_sz_type {
	int32_t codec_state;
	int32_t codec_crashed_counter;
	int32_t hs_codec_state;
	int32_t hs_codec_crashed_counter;
	int32_t speaker_impedance[SPEAKER_MAX_COUNT];
	int32_t speaker_temp[SPEAKER_MAX_COUNT];
	int32_t speaker_excursion[SPEAKER_MAX_COUNT];
	int32_t speaker_heartbeat[SPEAKER_MAX_COUNT];
	int32_t hwinfo_part_number[AUDIOMETRIC_CH_LENGTH];
	struct wdsp_stat_priv_type wdsp_stat_priv;
	uint32_t mic_broken_degrade;
};

struct audiometrics_priv_type {
	struct cdev cdev;
	dev_t amcs_dev;
	struct class *class;
	struct device *device;
	struct audio_sz_type sz;
	/* TODO: audiohal logger ring buffers */
	struct amcs_ion_handle_type amcs_ion;
	struct mutex lock;
	int amcs_major;
};

static ssize_t codec_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct audiometrics_priv_type *priv = NULL;
	int counts = 0;

	if (IS_ERR_OR_NULL(dev))
		return -EINVAL;

	priv = dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(priv))
		return -EINVAL;

	mutex_lock(&priv->lock);
	counts = scnprintf(buf, PAGE_SIZE, "%d", priv->sz.codec_state);
	mutex_unlock(&priv->lock);

	dev_dbg(dev, "%s: %s\n", __func__, buf);

	return counts;
}

static ssize_t hs_codec_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct audiometrics_priv_type *priv = NULL;
	int counts = 0;

	if (IS_ERR_OR_NULL(dev))
		return -EINVAL;

	priv = dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(priv))
		return -EINVAL;

	mutex_lock(&priv->lock);
	counts = scnprintf(buf, PAGE_SIZE, "%d", priv->sz.hs_codec_state);
	mutex_unlock(&priv->lock);

	dev_dbg(dev, "%s: %s\n", __func__, buf);

	return counts;
}


static ssize_t speaker_impedance_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct audiometrics_priv_type *priv = NULL;
	int counts = 0;
	int scale = 100000;

	if (IS_ERR_OR_NULL(dev))
		return -EINVAL;

	priv = dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(priv))
		return -EINVAL;

	mutex_lock(&priv->lock);

	counts = scnprintf(buf, PAGE_SIZE, "%d.%05d,%d.%05d",
			priv->sz.speaker_impedance[0] / scale,
			priv->sz.speaker_impedance[0] % scale,
			priv->sz.speaker_impedance[1] / scale,
			priv->sz.speaker_impedance[1] % scale);

	mutex_unlock(&priv->lock);

	dev_dbg(dev, "%s: %s\n", __func__, buf);

	return counts;
}

static ssize_t speaker_temp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct audiometrics_priv_type *priv = NULL;
	int counts = 0;
	int scale = 100000;

	if (IS_ERR_OR_NULL(dev))
		return -EINVAL;

	priv = dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(priv))
		return -EINVAL;

	mutex_lock(&priv->lock);

	counts = scnprintf(buf, PAGE_SIZE, "%d.%05d,%d.%05d",
			priv->sz.speaker_temp[0] / scale,
			priv->sz.speaker_temp[0] % scale,
			priv->sz.speaker_temp[1] / scale,
			priv->sz.speaker_temp[1] % scale);

	mutex_unlock(&priv->lock);

	dev_dbg(dev, "%s: %s\n", __func__, buf);

	return counts;
}


static ssize_t speaker_excursion_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct audiometrics_priv_type *priv = NULL;
	int counts = 0;

	if (IS_ERR_OR_NULL(dev))
		return -EINVAL;

	priv = dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(priv))
		return -EINVAL;

	mutex_lock(&priv->lock);

	counts = scnprintf(buf, PAGE_SIZE, "%d,%d",
			priv->sz.speaker_excursion[0],
			priv->sz.speaker_excursion[1]);

	mutex_unlock(&priv->lock);

	dev_dbg(dev, "%s: %s\n", __func__, buf);

	return counts;

}


static ssize_t speaker_heartbeat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct audiometrics_priv_type *priv = NULL;
	int counts = 0;

	if (IS_ERR_OR_NULL(dev))
		return -EINVAL;

	priv = dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(priv))
		return -EINVAL;

	mutex_lock(&priv->lock);

	counts = scnprintf(buf, PAGE_SIZE, "%d,%d",
			priv->sz.speaker_heartbeat[0],
			priv->sz.speaker_heartbeat[1]);

	mutex_unlock(&priv->lock);

	dev_dbg(dev, "%s: %s\n", __func__, buf);

	return counts;
}



static ssize_t hwinfo_part_number_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t wdsp_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t mic_broken_degrade_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct audiometrics_priv_type *priv = NULL;
	int counts = 0;

	if (IS_ERR_OR_NULL(dev))
		return -EINVAL;

	priv = dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(priv))
		return -EINVAL;

	mutex_lock(&priv->lock);

	counts = scnprintf(buf, PAGE_SIZE, "%d", priv->sz.mic_broken_degrade);

	mutex_unlock(&priv->lock);

	dev_dbg(dev, "%s: %s\n", __func__, buf);

	return counts;
}

static int amcs_cdev_open(struct inode *inode, struct file *file)
{
	struct audiometrics_priv_type *priv = container_of(inode->i_cdev,
					struct audiometrics_priv_type, cdev);

	file->private_data = priv;
	return 0;
}

static int amcs_cdev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long amcs_cdev_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct audiometrics_priv_type *priv = file->private_data;
	long ret = -EINVAL;
	int i = 0;
	struct amcs_params params;

	dev_dbg(priv->device, "%s cmd = 0x%x", __func__, cmd);

	if (IS_ERR_OR_NULL(priv)) {
		dev_err(&amcs_pdev->dev, "%s: priv is null or err\n", __func__);
		return ret;
	}

	if (_IOC_TYPE(cmd) != AMCS_IOCTL_MAGIC) {
		dev_err(&amcs_pdev->dev, "%s: cmd 0x%08x is not AMCS IOCTL\n", __func__, cmd);
		return ret;
	}

	if (copy_from_user(&params, (struct amcs_params *)arg, _IOC_SIZE(cmd)))
		return ret;

	switch (cmd) {
	case AMCS_IOCTL_METRIC_UPDATE:
		switch (params.op) {

		case AMCS_OP_CODEC_STATUS:
			mutex_lock(&priv->lock);
			priv->sz.codec_state = params.val[0];
			mutex_unlock(&priv->lock);
			ret = 0;
		break;

		case AMCS_OP_CODEC_STATUS_HS:
			mutex_lock(&priv->lock);
			priv->sz.hs_codec_state = params.val[0];
			mutex_unlock(&priv->lock);
			ret = 0;
		break;

		case AMCS_OP_SPEAKER_IMP:
			mutex_lock(&priv->lock);
			for (i = 0; i < SPEAKER_MAX_COUNT && i < sizeof(params.val); i++)
				priv->sz.speaker_impedance[i] = params.val[i];
			mutex_unlock(&priv->lock);

			ret = 0;
		break;

		case AMCS_OP_SPEAKER_TEMP:
			mutex_lock(&priv->lock);
			for (i = 0; i < SPEAKER_MAX_COUNT && i < sizeof(params.val); i++)
				priv->sz.speaker_temp[i] = params.val[i];
			mutex_unlock(&priv->lock);

			ret = 0;
		break;

		case AMCS_OP_SPEAKER_HEART:
			mutex_lock(&priv->lock);
			for (i = 0; i < SPEAKER_MAX_COUNT && i < sizeof(params.val); i++)
				priv->sz.speaker_heartbeat[i] = params.val[i];
			mutex_unlock(&priv->lock);

			ret = 0;
		break;

		case AMCS_OP_SPEAKER_EXCUR:
			mutex_lock(&priv->lock);
			for (i = 0; i < SPEAKER_MAX_COUNT && i < sizeof(params.val); i++)
				priv->sz.speaker_excursion[i] = params.val[i];
			mutex_unlock(&priv->lock);

			ret = 0;
		break;

		case AMCS_OP_MIC_BROKEN_DEGRADE:
			mutex_lock(&priv->lock);
			priv->sz.mic_broken_degrade = (uint32_t)params.val[0];
			mutex_unlock(&priv->lock);
			ret = 0;
		break;

		case AMCS_OP_COUNTER:
			mutex_lock(&priv->lock);
			if (params.val[0] == AMCS_OP2_GET) {
				params.val[1] =	priv->sz.codec_crashed_counter;
				params.val[2] =	priv->sz.hs_codec_crashed_counter;
				params.val[AMCS_PARAMS_LENGTH_MAX-1] =	(int32_t)AMCS_IOCTL_MAGIC;
			} else if (params.val[0] == AMCS_OP2_SET) {
				priv->sz.codec_crashed_counter = params.val[1];
				priv->sz.hs_codec_crashed_counter = params.val[2];
				params.val[AMCS_PARAMS_LENGTH_MAX-1] =	(int32_t)AMCS_IOCTL_MAGIC;
			}
			mutex_unlock(&priv->lock);

			if (!copy_to_user((struct amcs_params *)arg, &params, _IOC_SIZE(cmd)))
				ret = 0;
			else
				ret = -EINVAL;

		break;

		default:
			dev_warn(priv->device, "%s, unsupported op = %d\n", __func__, params.op);
			ret = -EINVAL;
		break;

		}
	break;

	default:
		dev_err(priv->device, "Received IOCTL with invalid ID (%d) returning ENOTTY", cmd);
		ret = -ENOTTY;
	break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long amcs_cdev_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) != AMCS_IOCTL_MAGIC) {
		dev_err(&amcs_pdev->dev, "%s: cmd 0x%08x is not AMCS IOCTL\n", __func__, cmd);
		return -ENOTTY;
	}

	return amcs_cdev_unlocked_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define amcs_cdev_compat_ioctl NULL;
#endif

static char *amcs_devnode(struct device *dev, umode_t *mode)
{
	struct audiometrics_priv_type *priv = NULL;

	if (!mode || !dev)
		return NULL;

	priv = dev_get_drvdata(dev);

	if (!priv)
		return NULL;

	if (MAJOR(dev->devt) == priv->amcs_major)
		*mode = 0666;

	return kasprintf(GFP_KERNEL, "%s", dev_name(dev));
}

static DEVICE_ATTR_RO(codec_state);
static DEVICE_ATTR_RO(hs_codec_state);
static DEVICE_ATTR_RO(speaker_impedance);
static DEVICE_ATTR_RO(speaker_temp);
static DEVICE_ATTR_RO(speaker_excursion);
static DEVICE_ATTR_RO(speaker_heartbeat);
static DEVICE_ATTR_RO(hwinfo_part_number);
static DEVICE_ATTR_RO(wdsp_stat);
static DEVICE_ATTR_RO(mic_broken_degrade);


static struct attribute *audiometrics_fs_attrs[] = {
	&dev_attr_codec_state.attr,
	&dev_attr_hs_codec_state.attr,
	&dev_attr_speaker_impedance.attr,
	&dev_attr_speaker_temp.attr,
	&dev_attr_speaker_excursion.attr,
	&dev_attr_speaker_heartbeat.attr,
	&dev_attr_hwinfo_part_number.attr,
	&dev_attr_wdsp_stat.attr,
	&dev_attr_mic_broken_degrade.attr,
	NULL,
};

static struct attribute_group audiometrics_fs_attr_group = {
	.attrs = audiometrics_fs_attrs,
};

static const struct file_operations amcs_fops = {
	.open = amcs_cdev_open,
	.release = amcs_cdev_release,
	.unlocked_ioctl = amcs_cdev_unlocked_ioctl,
	.compat_ioctl = amcs_cdev_compat_ioctl,

	.owner = THIS_MODULE,
};

static int amcs_init_cdev(struct audiometrics_priv_type *priv)
{
	int ret;

	ret = alloc_chrdev_region(&priv->amcs_dev, 0, AMCS_MAX_MINOR, AMCS_CDEV_NAME);
	if (ret != 0) {
		dev_err(&amcs_pdev->dev, "Failed to alloc chrdev region\n");
		goto err_init_cdev;
	}

	cdev_init(&priv->cdev, &amcs_fops);
	priv->cdev.owner = THIS_MODULE;

	ret = cdev_add(&priv->cdev, priv->amcs_dev, AMCS_MAX_MINOR);
	if (ret) {
		dev_err(&amcs_pdev->dev, "Failed to register chrdev\n");
		goto err_cdev_add;
	}

	priv->amcs_major = MAJOR(priv->amcs_dev);

	priv->class = class_create(THIS_MODULE, AMCS_CDEV_NAME);
	if (!priv->class) {
		dev_err(&amcs_pdev->dev, "Failed to create amcs class\n");
		ret = -ENXIO;
		goto err_class_create;
	}

	priv->class->devnode = amcs_devnode;

	priv->device = device_create(priv->class, NULL,
					 MKDEV(priv->amcs_major, 0),
					 priv, AMCS_CDEV_NAME);
	if (!priv->device) {
		dev_err(&amcs_pdev->dev, "Failed to create amcs device\n");
		ret = -ENXIO;
		goto err_device_create;
	}

	dev_dbg(&amcs_pdev->dev, "cdev registered\n");

	return ret;

err_device_create:
	class_destroy(priv->class);
err_class_create:
	cdev_del(&priv->cdev);
err_cdev_add:
	unregister_chrdev_region(priv->amcs_dev, 1);
err_init_cdev:

	return ret;
}

static void amcs_deinit_cdev(struct audiometrics_priv_type *priv)
{
	if (!priv)
		return;

	device_destroy(priv->class, priv->amcs_dev);
	class_destroy(priv->class);
	cdev_del(&priv->cdev);
	unregister_chrdev_region(priv->amcs_dev, AMCS_MAX_MINOR);
}


static int audiometrics_platform_probe(struct platform_device *pdev)
{
	int err;
	struct audiometrics_priv_type *priv = NULL;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);

	if (!priv)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, priv);

	mutex_init(&priv->lock);

	/* create cdev to export ioctl to ahal & metric service. */
	err = amcs_init_cdev(priv);
	if (err) {
		dev_err(&pdev->dev, "Failed to initialize cdev: %d\n", err);
		goto err_amcs_init_cdev;
	}

	/* create audio sysfs for suez */
	err = sysfs_create_group(&pdev->dev.kobj, &audiometrics_fs_attr_group);
	if (err) {
		dev_err(&pdev->dev, "Failed to initialize fs attrs: %d\n", err);
		goto audiometrics_fs_attr_group_err;
	}

	dev_dbg(&pdev->dev, "%s registered\n", __func__);
	return 0;

audiometrics_fs_attr_group_err:
	dev_err(&pdev->dev, "%s create sysfs failed, err=%d\n", __func__, err);
err_amcs_init_cdev:
	mutex_destroy(&priv->lock);
	amcs_deinit_cdev(priv);
	devm_kfree(&pdev->dev, priv);
	return err;
}

static int audiometrics_platform_remove(struct platform_device *pdev)
{
	struct audiometrics_priv_type *priv = dev_get_drvdata(&pdev->dev);

	amcs_deinit_cdev(priv);
	sysfs_remove_group(&pdev->dev.kobj, &audiometrics_fs_attr_group);
	mutex_destroy(&priv->lock);
	devm_kfree(&pdev->dev, priv);
	return 0;
}

struct platform_driver audiometrics_driver = {
	.probe = audiometrics_platform_probe,
	.remove = audiometrics_platform_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	}
};

static int amcs_init(void)
{
	int ret;

	amcs_pdev = platform_device_register_simple(DRIVER_NAME, -1, NULL, 0);

	if (IS_ERR(amcs_pdev))
		return PTR_ERR(amcs_pdev);

	ret = platform_driver_register(&audiometrics_driver);
	if (ret != 0)
		platform_device_unregister(amcs_pdev);

	return ret;
}

static void amcs_exit(void)
{
	platform_driver_unregister(&audiometrics_driver);
	platform_device_unregister(amcs_pdev);
}

module_init(amcs_init);
module_exit(amcs_exit);

MODULE_DESCRIPTION("Google AudioMetrics driver");
MODULE_LICENSE("GPL");
