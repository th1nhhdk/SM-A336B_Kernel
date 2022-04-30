/*
 * sb_batt_dump.c
 * Samsung Mobile Battery Dump Driver
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/slab.h>

#include <linux/battery/sb_sysfs.h>
#include <linux/battery/sb_notify.h>

#include "sec_battery.h"
#include "sec_charging_common.h"
#include "sb_batt_dump.h"

#define bd_log(str, ...) pr_info("[BATT-DUMP]:%s: "str, __func__, ##__VA_ARGS__)

#define BD_MODULE_NAME	"batt-dump"

struct sb_bd {
	struct notifier_block nb;
};

static ssize_t show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

#define BD_SYSFS_ATTR(_name)						\
{									\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = show_attrs,					\
	.store = store_attrs,					\
}

static struct device_attribute bd_attr[] = {
	BD_SYSFS_ATTR(battery_dump),
};

enum sb_bd_attrs {
	BATTERY_DUMP = 0,
};

static ssize_t show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - bd_attr;
	ssize_t count = 0;
	union power_supply_propval value = {0, };

	switch (offset) {
	case BATTERY_DUMP:
	{
		char temp_buf[1024] = {0,};
		int size = 1024;

		snprintf(temp_buf+strlen(temp_buf), size,
			"%d,%d,%d,%d,%d,%d,%s,%s,%s,%s,%d,%s,%d,%d,%lu,%d,",
			battery->voltage_now, battery->current_now,
			battery->current_max, battery->charging_current,
			battery->capacity, battery->temperature,
			sb_get_bst_str(battery->status),
			sb_get_cm_str(battery->charging_mode),
			sb_get_hl_str(battery->health),
			sb_get_ct_str(battery->cable_type),
			battery->muic_cable_type,
			sb_get_tz_str(battery->thermal_zone),
			is_slate_mode(battery),
			battery->store_mode,
			(battery->expired_time / 1000),
			sec_bat_get_lpmode());
		size = sizeof(temp_buf) - strlen(temp_buf);

#if defined(CONFIG_BATTERY_AGE_FORECAST)
		snprintf(temp_buf+strlen(temp_buf), size, "%d,", battery->batt_cycle);
		size = sizeof(temp_buf) - strlen(temp_buf);
#endif
		psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_EXT_PROP_BATT_DUMP, value);
		snprintf(temp_buf+strlen(temp_buf), size, "%s", value.strval);
		size = sizeof(temp_buf) - strlen(temp_buf);

		count += scnprintf(buf + count, PAGE_SIZE - count, "%s\n", temp_buf);
	}
		break;
	default:
		break;
	}

	return count;
}

static ssize_t store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	const ptrdiff_t offset = attr - bd_attr;

	switch (offset) {
	case BATTERY_DUMP:
		break;
	default:
		break;
	}

	return count;
}

int sb_bd_init(void)
{
	struct sb_bd *bd;
	int ret = 0;

	bd = kzalloc(sizeof(struct sb_bd), GFP_KERNEL);
	if (!bd)
		return -ENOMEM;

	ret = sb_sysfs_add_attrs(BD_MODULE_NAME, bd, bd_attr, ARRAY_SIZE(bd_attr));
	bd_log("sb_sysfs_add_attrs ret = %s\n", (ret) ? "fail" : "success");

	return ret;
}
EXPORT_SYMBOL(sb_bd_init);
