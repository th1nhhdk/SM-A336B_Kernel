/*
 * Samsung Exynos5 SoC series Actuator driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_DW9800_H
#define IS_DEVICE_DW9800_H

#define DW9800_POS_SIZE_BIT		ACTUATOR_POS_SIZE_10BIT
#define DW9800_POS_MAX_SIZE		((1 << DW9800_POS_SIZE_BIT) - 1)
#define DW9800_POS_DIRECTION		ACTUATOR_RANGE_INF_TO_MAC

#endif
