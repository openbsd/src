/*	$OpenBSD: drm_dp_helper.c,v 1.5 2015/04/08 04:24:40 jsg Exp $	*/
/*
 * Copyright © 2009 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include "drmP.h"
#include "drm_dp_helper.h"

/**
 * DOC: dp helpers
 *
 * These functions contain some common logic and helpers at various abstraction
 * levels to deal with Display Port sink devices and related things like DP aux
 * channel transfers, EDID reading over DP aux channels, decoding certain DPCD
 * blocks, ...
 */

/* Run a single AUX_CH I2C transaction, writing/reading data as necessary */
static int
i2c_algo_dp_aux_transaction(struct i2c_controller *adapter, int mode,
			    uint8_t write_byte, uint8_t *read_byte)
{
	struct i2c_algo_dp_aux_data *algo_data = adapter->ic_cookie;
	int ret;

	ret = (*algo_data->aux_ch)(adapter, mode,
				   write_byte, read_byte);
	return ret;
}

/*
 * I2C over AUX CH
 */

/*
 * Send the address. If the I2C link is running, this 'restarts'
 * the connection with the new address, this is used for doing
 * a write followed by a read (as needed for DDC)
 */
static int
i2c_algo_dp_aux_address(struct i2c_controller *adapter, u16 address, bool reading)
{
	struct i2c_algo_dp_aux_data *algo_data = adapter->ic_cookie;
	int mode = MODE_I2C_START;
	int ret;

	if (reading)
		mode |= MODE_I2C_READ;
	else
		mode |= MODE_I2C_WRITE;
	algo_data->address = address;
	algo_data->running = true;
	ret = i2c_algo_dp_aux_transaction(adapter, mode, 0, NULL);
	return ret;
}

/*
 * Stop the I2C transaction. This closes out the link, sending
 * a bare address packet with the MOT bit turned off
 */
static void
i2c_algo_dp_aux_stop(struct i2c_controller *adapter, bool reading)
{
	struct i2c_algo_dp_aux_data *algo_data = adapter->ic_cookie;
	int mode = MODE_I2C_STOP;

	if (reading)
		mode |= MODE_I2C_READ;
	else
		mode |= MODE_I2C_WRITE;
	if (algo_data->running) {
		(void) i2c_algo_dp_aux_transaction(adapter, mode, 0, NULL);
		algo_data->running = false;
	}
}

/*
 * Write a single byte to the current I2C address, the
 * the I2C link must be running or this returns -EIO
 */
static int
i2c_algo_dp_aux_put_byte(struct i2c_controller *adapter, u8 byte)
{
	struct i2c_algo_dp_aux_data *algo_data = adapter->ic_cookie;
	int ret;

	if (!algo_data->running)
		return -EIO;

	ret = i2c_algo_dp_aux_transaction(adapter, MODE_I2C_WRITE, byte, NULL);
	return ret;
}

/*
 * Read a single byte from the current I2C address, the
 * I2C link must be running or this returns -EIO
 */
static int
i2c_algo_dp_aux_get_byte(struct i2c_controller *adapter, u8 *byte_ret)
{
	struct i2c_algo_dp_aux_data *algo_data = adapter->ic_cookie;
	int ret;

	if (!algo_data->running)
		return -EIO;

	ret = i2c_algo_dp_aux_transaction(adapter, MODE_I2C_READ, 0, byte_ret);
	return ret;
}

static int
i2c_algo_dp_aux_exec(void *cookie, i2c_op_t op, i2c_addr_t addr,
    const void *cmdbuf, size_t cmdlen, void *buffer, size_t len, int flags)
{
	struct i2c_algo_dp_aux_data *algo_data = cookie;
	struct i2c_controller *adapter = algo_data->adapter;
	uint8_t *buf;
	int i, ret;

	buf = (void *)cmdbuf;
	if (cmdlen > 0) {
		ret = i2c_algo_dp_aux_address(adapter, addr, false);
		if (ret < 0)
			goto out;
		for (i = 0; i < cmdlen; i++) {
			ret = i2c_algo_dp_aux_put_byte(adapter, buf[i]);
			if (ret < 0)
				goto out;
		}
	}

	buf = buffer;
	ret = i2c_algo_dp_aux_address(adapter, addr, I2C_OP_READ_P(op));
	if (ret < 0)
		goto out;
	if (I2C_OP_READ_P(op)) {
		for (i = 0; i < len; i++) {
			ret = i2c_algo_dp_aux_get_byte(adapter, &buf[i]);
			if (ret < 0)
				break;
		}
	} else {
		for (i = 0; i < len; i++) {
			ret = i2c_algo_dp_aux_put_byte(adapter, buf[i]);
			if (ret < 0)
				break;
		}
	}

out:
	if (I2C_OP_STOP_P(op))
		i2c_algo_dp_aux_stop(adapter, I2C_OP_READ_P(op));

	if (ret >= 0)
		ret = 0;
	DRM_DEBUG_KMS("dp_aux_exec return %d\n", ret);
	return ret;
}

static void
i2c_dp_aux_reset_bus(struct i2c_controller *adapter)
{
	(void) i2c_algo_dp_aux_address(adapter, 0, false);
	(void) i2c_algo_dp_aux_stop(adapter, false);
}

static int
i2c_dp_aux_acquire_bus(void *cookie, int flags)
{
	return (0);
}

static void
i2c_dp_aux_release_bus(void *cookie, int flags)
{
	struct i2c_algo_dp_aux_data *algo_data = cookie;

	i2c_dp_aux_reset_bus(algo_data->adapter);
}

static int
i2c_dp_aux_prepare_bus(struct i2c_controller *adapter)
{
#ifdef notyet
	adapter->algo = &i2c_dp_aux_algo;
	adapter->retries = 3;
#endif
	/* cookie set in driver */
	adapter->ic_acquire_bus = i2c_dp_aux_acquire_bus;
	adapter->ic_release_bus = i2c_dp_aux_release_bus;
	adapter->ic_exec = i2c_algo_dp_aux_exec;
	i2c_dp_aux_reset_bus(adapter);
	return 0;
}

/**
 * i2c_dp_aux_add_bus() - register an i2c adapter using the aux ch helper
 * @adapter: i2c adapter to register
 *
 * This registers an i2c adapater that uses dp aux channel as it's underlaying
 * transport. The driver needs to fill out the &i2c_algo_dp_aux_data structure
 * and store it in the algo_data member of the @adapter argument. This will be
 * used by the i2c over dp aux algorithm to drive the hardware.
 *
 * RETURNS:
 * 0 on success, -ERRNO on failure.
 */
int
i2c_dp_aux_add_bus(struct i2c_controller *adapter)
{
	int error;

	error = i2c_dp_aux_prepare_bus(adapter);
	if (error)
		return error;
#ifdef notyet
	error = i2c_add_adapter(adapter);
#endif
	return error;
}
EXPORT_SYMBOL(i2c_dp_aux_add_bus);

/* Helpers for DP link training */
static u8 dp_link_status(u8 link_status[DP_LINK_STATUS_SIZE], int r)
{
	return link_status[r - DP_LANE0_1_STATUS];
}

static u8 dp_get_lane_status(u8 link_status[DP_LINK_STATUS_SIZE],
			     int lane)
{
	int i = DP_LANE0_1_STATUS + (lane >> 1);
	int s = (lane & 1) * 4;
	u8 l = dp_link_status(link_status, i);
	return (l >> s) & 0xf;
}

bool drm_dp_channel_eq_ok(u8 link_status[DP_LINK_STATUS_SIZE],
			  int lane_count)
{
	u8 lane_align;
	u8 lane_status;
	int lane;

	lane_align = dp_link_status(link_status,
				    DP_LANE_ALIGN_STATUS_UPDATED);
	if ((lane_align & DP_INTERLANE_ALIGN_DONE) == 0)
		return false;
	for (lane = 0; lane < lane_count; lane++) {
		lane_status = dp_get_lane_status(link_status, lane);
		if ((lane_status & DP_CHANNEL_EQ_BITS) != DP_CHANNEL_EQ_BITS)
			return false;
	}
	return true;
}
EXPORT_SYMBOL(drm_dp_channel_eq_ok);

bool drm_dp_clock_recovery_ok(u8 link_status[DP_LINK_STATUS_SIZE],
			      int lane_count)
{
	int lane;
	u8 lane_status;

	for (lane = 0; lane < lane_count; lane++) {
		lane_status = dp_get_lane_status(link_status, lane);
		if ((lane_status & DP_LANE_CR_DONE) == 0)
			return false;
	}
	return true;
}
EXPORT_SYMBOL(drm_dp_clock_recovery_ok);

u8 drm_dp_get_adjust_request_voltage(u8 link_status[DP_LINK_STATUS_SIZE],
				     int lane)
{
	int i = DP_ADJUST_REQUEST_LANE0_1 + (lane >> 1);
	int s = ((lane & 1) ?
		 DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT :
		 DP_ADJUST_VOLTAGE_SWING_LANE0_SHIFT);
	u8 l = dp_link_status(link_status, i);

	return ((l >> s) & 0x3) << DP_TRAIN_VOLTAGE_SWING_SHIFT;
}
EXPORT_SYMBOL(drm_dp_get_adjust_request_voltage);

u8 drm_dp_get_adjust_request_pre_emphasis(u8 link_status[DP_LINK_STATUS_SIZE],
					  int lane)
{
	int i = DP_ADJUST_REQUEST_LANE0_1 + (lane >> 1);
	int s = ((lane & 1) ?
		 DP_ADJUST_PRE_EMPHASIS_LANE1_SHIFT :
		 DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT);
	u8 l = dp_link_status(link_status, i);

	return ((l >> s) & 0x3) << DP_TRAIN_PRE_EMPHASIS_SHIFT;
}
EXPORT_SYMBOL(drm_dp_get_adjust_request_pre_emphasis);

void drm_dp_link_train_clock_recovery_delay(u8 dpcd[DP_RECEIVER_CAP_SIZE]) {
	if (dpcd[DP_TRAINING_AUX_RD_INTERVAL] == 0)
		udelay(100);
	else
		mdelay(dpcd[DP_TRAINING_AUX_RD_INTERVAL] * 4);
}
EXPORT_SYMBOL(drm_dp_link_train_clock_recovery_delay);

void drm_dp_link_train_channel_eq_delay(u8 dpcd[DP_RECEIVER_CAP_SIZE]) {
	if (dpcd[DP_TRAINING_AUX_RD_INTERVAL] == 0)
		udelay(400);
	else
		mdelay(dpcd[DP_TRAINING_AUX_RD_INTERVAL] * 4);
}
EXPORT_SYMBOL(drm_dp_link_train_channel_eq_delay);

u8 drm_dp_link_rate_to_bw_code(int link_rate)
{
	switch (link_rate) {
	case 162000:
	default:
		return DP_LINK_BW_1_62;
	case 270000:
		return DP_LINK_BW_2_7;
	case 540000:
		return DP_LINK_BW_5_4;
	}
}
EXPORT_SYMBOL(drm_dp_link_rate_to_bw_code);

int drm_dp_bw_code_to_link_rate(u8 link_bw)
{
	switch (link_bw) {
	case DP_LINK_BW_1_62:
	default:
		return 162000;
	case DP_LINK_BW_2_7:
		return 270000;
	case DP_LINK_BW_5_4:
		return 540000;
	}
}
EXPORT_SYMBOL(drm_dp_bw_code_to_link_rate);
