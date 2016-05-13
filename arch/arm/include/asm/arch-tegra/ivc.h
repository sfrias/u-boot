/*
 * Copyright (c) 2016, NVIDIA CORPORATION.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef _ASM_ARCH_TEGRA_IVC_H
#define _ASM_ARCH_TEGRA_IVC_H

#include <common.h>

/*
 * Tegra IVC is a communication protocol that transfers fixed-size frames
 * bi-directionally and in-order between the local CPU and some remote entity.
 * Communication is via a statically sized and allocated buffer in shared
 * memory and a notification mechanism.
 *
 * This API handles all aspects of the shared memory buffer's metadata, and
 * leaves all aspects of the frame content to the calling code; frames
 * typically contain some higher-level protocol. The notification mechanism is
 * also handled externally to this API, since it can vary from instance to
 * instance.
 *
 * The client model is to first find some free (for TX) or filled (for RX)
 * frame, process that frame's memory buffer (fill or read it), and then
 * inform the protocol that the frame has been filled/read, i.e. advance the
 * write/read pointer. If the channel is full, there may be no available frames
 * to fill/read. In this case, client code may either poll for an available
 * frame, or wait for the remote entity to send a notification to the local
 * CPU.
 */

/**
 * struct tegra_ivc - In-memory shared memory layout.
 *
 * This is described in detail in ivc.c.
 */
struct tegra_ivc_channel_header;

/**
 * struct tegra_ivc - Software state of an IVC channel.
 *
 * This state is internal to the IVC code and should not be accessed directly
 * by clients. It is public solely so clients can allocate storage for the
 * structure.
 */
struct tegra_ivc {
	/**
	 * rx_channel - Pointer to the shared memory region used to receive
	 * messages from the remote entity.
	 */
	struct tegra_ivc_channel_header *rx_channel;
	/**
	 * tx_channel - Pointer to the shared memory region used to send
	 * messages to the remote entity.
	 */
	struct tegra_ivc_channel_header *tx_channel;
	/**
	 * r_pos - The position in list of frames in rx_channel that we are
	 * reading from.
	 */
	uint32_t r_pos;
	/**
	 * w_pos - The position in list of frames in tx_channel that we are
	 * writing to.
	 */
	uint32_t w_pos;
	/**
	 * nframes - The number of frames allocated (in each direction) in
	 * shared memory.
	 */
	uint32_t nframes;
	/**
	 * frame_size - The size of each frame in shared memory.
	 */
	uint32_t frame_size;
};

/**
 * tegra_ivc_read_get_next_frame - Locate the next frame to receive.
 *
 * Locate the next frame to be received/processed, return the address of the
 * frame, and do not remove it from the queue. Repeated calls to this function
 * will return the same address until tegra_ivc_read_advance() is called.
 *
 * @ivc		The IVC channel.
 * @frame	Pointer to be filled with the address of the frame to receive.
 *
 * @return 0 if a frame is available, else a negative error code.
 */
int tegra_ivc_read_get_next_frame(struct tegra_ivc *ivc, void **frame);

/**
 * tegra_ivc_read_advance - Advance the read queue.
 *
 * Inform the protocol and remote entity that the frame returned by
 * tegra_ivc_read_get_next_frame() has been processed. The remote end may then
 * re-use it to transmit further data. Subsequent to this function returning,
 * tegra_ivc_read_get_next_frame() will return a different frame.
 *
 * @ivc		The IVC channel.
 *
 * @return 0 if OK, else a negative error code.
 */
int tegra_ivc_read_advance(struct tegra_ivc *ivc);

/**
 * tegra_ivc_write_get_next_frame - Locate the next frame to fill for transmit.
 *
 * Locate the next frame to be filled for transmit, return the address of the
 * frame, and do not add it to the queue. Repeated calls to this function
 * will return the same address until tegra_ivc_read_advance() is called.
 *
 * @ivc		The IVC channel.
 * @frame	Pointer to be filled with the address of the frame to fill.
 *
 * @return 0 if a frame is available, else a negative error code.
 */
int tegra_ivc_write_get_next_frame(struct tegra_ivc *ivc, void **frame);

/**
 * tegra_ivc_write_advance - Advance the write queue.
 *
 * Inform the protocol and remote entity that the frame returned by
 * tegra_ivc_write_get_next_frame() has been filled and should be transmitted.
 * The remote end may then read data from it. Subsequent to this function
 * returning, tegra_ivc_write_get_next_frame() will return a different frame.
 *
 * @ivc		The IVC channel.
 *
 * @return 0 if OK, else a negative error code.
 */
int tegra_ivc_write_advance(struct tegra_ivc *ivc);

/**
 * tegra_ivc_init - Initialize a channel's software state.
 *
 * @ivc		The IVC channel.
 * @rx_base	Address of the the RX shared memory buffer.
 * @tx_base	Address of the the TX shared memory buffer.
 * @nframes	Number of frames in each shared memory buffer.
 * @frame_size	Size of each frame.
 *
 * @return 0 if OK, else a negative error code.
 */
int tegra_ivc_init(struct tegra_ivc *ivc, ulong rx_base, ulong tx_base,
		   uint32_t nframes, uint32_t frame_size);

#endif
