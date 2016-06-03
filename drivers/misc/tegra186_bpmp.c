/*
 * Copyright (c) 2016, NVIDIA CORPORATION.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include <common.h>
#include <dm.h>
#include <dm/lists.h>
#include <mailbox_client.h>
#include <asm/arch-tegra/bpmp_abi.h>
#include <asm/arch-tegra/ivc.h>
#include <asm/arch-tegra/tegra186_bpmp.h>

#define BPMP_IVC_FRAME_COUNT 1
#define BPMP_IVC_FRAME_SIZE 128

#define BPMP_FLAG_DO_ACK	BIT(0)
#define BPMP_FLAG_RING_DOORBELL	BIT(1)

DECLARE_GLOBAL_DATA_PTR;

struct tegra186_bpmp {
	struct mbox_chan mbox;
	struct tegra_ivc ivc;
};

int tegra186_bpmp_call(struct udevice *dev, uint32_t mrq,
		       void *tx_msg, uint32_t tx_size,
		       void *rx_msg, uint32_t rx_size)
{
	struct tegra186_bpmp *priv = dev_get_priv(dev);
	int ret, err;
	void *ivc_frame;
	struct mrq_request *req;
	struct mrq_response *resp;
	ulong start_time;

	debug("%s(dev=%p, mrq=%u, tx_msg=%p, tx_size=%u, rx_msg=%p, rx_size=%u) (priv=%p)\n",
	      __func__, dev, mrq, tx_msg, tx_size, rx_msg, rx_size, priv);

	if ((tx_size > BPMP_IVC_FRAME_SIZE) || (rx_size > BPMP_IVC_FRAME_SIZE))
		return -EINVAL;

	ret = tegra_ivc_write_get_next_frame(&priv->ivc, &ivc_frame);
	if (ret) {
		debug("tegra_ivc_write_get_next_frame() failed: %d\n", ret);
		return ret;
	}

	req = ivc_frame;
	req->mrq = mrq;
	req->flags = BPMP_FLAG_DO_ACK | BPMP_FLAG_RING_DOORBELL;
	memcpy(req + 1, tx_msg, tx_size);

	ret = tegra_ivc_write_advance(&priv->ivc);
	if (ret) {
		debug("tegra_ivc_write_advance() failed: %d\n", ret);
		return ret;
	}

	ret = mbox_send(&priv->mbox, NULL);
	if (ret) {
		debug("mbox_send() failed: %d\n", ret);
		return ret;
	}

	start_time = timer_get_us();
	for (;;) {
		ret = tegra_ivc_read_get_next_frame(&priv->ivc, &ivc_frame);
		if (!ret)
			break;
		if ((timer_get_us() - start_time) > 1000) {
			debug("tegra_ivc_read_get_next_frame() timed out\n");
			return -ETIMEDOUT;
		}
	}

	resp = ivc_frame;
	err = resp->err;
	if (!err && rx_msg && rx_size)
		memcpy(rx_msg, resp + 1, rx_size);

	ret = tegra_ivc_read_advance(&priv->ivc);
	if (ret) {
		debug("tegra_ivc_write_advance() failed: %d\n", ret);
		return ret;
	}

	if (err) {
		debug("BPMP responded with error %d\n", err);
		/* err isn't a U-Boot error code, so don't that */
		return -EIO;
	}

	return 0;
}

/**
 * The BPMP exposes multiple different services. We create a sub-device for
 * each separate type of service, since each device must be of the appropriate
 * UCLASS.
 */
static int tegra186_bpmp_bind(struct udevice *dev)
{
	int ret;
	struct udevice *child;

	debug("%s(dev=%p)\n", __func__, dev);

	ret = device_bind_driver_to_node(dev, "tegra186_clk", "tegra186_clk",
					 dev->of_offset, &child);
	if (ret)
		return ret;

	ret = device_bind_driver_to_node(dev, "tegra186_reset",
					 "tegra186_reset", dev->of_offset,
					 &child);
	if (ret)
		return ret;

	return 0;
}

static ulong tegra186_bpmp_get_shmem(struct udevice *dev, int index)
{
	int ret;
	struct fdtdec_phandle_args args;
	struct udevice fakedev;
	fdt_addr_t reg;

	ret = fdtdec_parse_phandle_with_args(gd->fdt_blob, dev->of_offset,
					      "shmem", NULL, 0, index, &args);
	if (ret < 0) {
		debug("fdtdec_parse_phandle_with_args() failed: %d\n", ret);
		return ret;
	}

	fakedev.of_offset = args.node;
	reg = dev_get_addr_index(&fakedev, 0);
	if (reg == FDT_ADDR_T_NONE) {
		debug("dev_get_addr_index() failed\n");
		return -ENODEV;
	}

	return reg;
}

static int tegra186_bpmp_probe(struct udevice *dev)
{
	struct tegra186_bpmp *priv = dev_get_priv(dev);
	int ret;
	ulong tx_base, rx_base;

	debug("%s(dev=%p) (priv=%p)\n", __func__, dev, priv);

	ret = mbox_get_by_index(dev, 0, &priv->mbox);
	if (ret) {
		debug("mbox_get_by_index() failed: %d\n", ret);
		return ret;
	}

	tx_base = tegra186_bpmp_get_shmem(dev, 0);
	if (IS_ERR_VALUE(tx_base)) {
		debug("tegra186_bpmp_get_shmem failed for tx_base\n");
		return tx_base;
	}
	rx_base = tegra186_bpmp_get_shmem(dev, 1);
	if (IS_ERR_VALUE(rx_base)) {
		debug("tegra186_bpmp_get_shmem failed for rx_base\n");
		return rx_base;
	}
	debug("shmem: rx=%lx, rx=%lx\n", rx_base, tx_base);

	ret = tegra_ivc_init(&priv->ivc, rx_base, tx_base, BPMP_IVC_FRAME_COUNT,
			     BPMP_IVC_FRAME_SIZE);
	if (ret) {
		debug("tegra_ivc_init() failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int tegra186_bpmp_remove(struct udevice *dev)
{
	struct tegra186_bpmp *priv = dev_get_priv(dev);

	debug("%s(dev=%p) (priv=%p)\n", __func__, dev, priv);

	mbox_free(&priv->mbox);

	return 0;
}

static const struct udevice_id tegra186_bpmp_ids[] = {
	{ .compatible = "nvidia,tegra186-bpmp" },
	{ }
};

U_BOOT_DRIVER(tegra186_bpmp) = {
	.name		= "tegra186_bpmp",
	.id		= UCLASS_MISC,
	.of_match	= tegra186_bpmp_ids,
	.bind		= tegra186_bpmp_bind,
	.probe		= tegra186_bpmp_probe,
	.remove		= tegra186_bpmp_remove,
	.priv_auto_alloc_size = sizeof(struct tegra186_bpmp),
};
