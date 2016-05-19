/*
 * Copyright (c) 2016, NVIDIA CORPORATION.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef __SANDBOX_CLK_H
#define __SANDBOX_CLK_H

#include <common.h>

struct udevice;

enum {
	SANDBOX_CLK_ID_SPI,
	SANDBOX_CLK_ID_I2C,

	SANDBOX_CLK_ID_COUNT,
};

enum {
	SANDBOX_CLK_TEST_ID_FIXED,
	SANDBOX_CLK_TEST_ID_SPI,
	SANDBOX_CLK_TEST_ID_I2C,

	SANDBOX_CLK_TEST_ID_COUNT,
};

ulong sandbox_clk_query_rate(struct udevice *dev, int id);
int sandbox_clk_query_enable(struct udevice *dev, int id);

int sandbox_clk_test_get(struct udevice *dev);
ulong sandbox_clk_test_get_rate(struct udevice *dev, int id);
ulong sandbox_clk_test_set_rate(struct udevice *dev, int id, ulong rate);
int sandbox_clk_test_enable(struct udevice *dev, int id);
int sandbox_clk_test_disable(struct udevice *dev, int id);
int sandbox_clk_test_free(struct udevice *dev);

#endif
