/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_P0_DSP_HW_P0_BUS_H__
#define __HW_P0_DSP_HW_P0_BUS_H__

#include "dsp-bus.h"

enum dsp_p0_bus_scenario_id {
	DSP_P0_MO_SCENARIO_COUNT,
};

int dsp_hw_p0_bus_register_ops(void);

#endif
