/*	$OpenBSD: i2c.h,v 1.1 2019/04/14 10:14:53 jsg Exp $	*/
/*
 * Copyright (c) 2017 Mark Kettenis
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _LINUX_I2C_H
#define _LINUX_I2C_H

#include <sys/stdint.h>
#include <sys/rwlock.h>
#include <linux/workqueue.h>
#include <linux/seq_file.h>

#include <dev/i2c/i2cvar.h>

struct i2c_algorithm;

#define I2C_FUNC_I2C			0
#define I2C_FUNC_SMBUS_EMUL		0
#define I2C_FUNC_SMBUS_READ_BLOCK_DATA	0
#define I2C_FUNC_SMBUS_BLOCK_PROC_CALL	0
#define I2C_FUNC_10BIT_ADDR		0

struct i2c_adapter {
	struct i2c_controller ic;

	char name[48];
	const struct i2c_algorithm *algo;
	void *algo_data;
	int retries;

	void *data;
};

#define I2C_NAME_SIZE	20

struct i2c_msg {
	uint16_t addr;
	uint16_t flags;
	uint16_t len;
	uint8_t *buf;
};

#define I2C_M_RD	0x0001
#define I2C_M_NOSTART	0x0002
#define I2C_M_STOP	0x0004

struct i2c_algorithm {
	int (*master_xfer)(struct i2c_adapter *, struct i2c_msg *, int);
	uint32_t (*functionality)(struct i2c_adapter *);
};

extern struct i2c_algorithm i2c_bit_algo;

struct i2c_algo_bit_data {
	struct i2c_controller ic;
};

int i2c_transfer(struct i2c_adapter *, struct i2c_msg *, int);
#define i2c_add_adapter(x) 0
#define i2c_del_adapter(x)
#define __i2c_transfer(adap, msgs, num)	i2c_transfer(adap, msgs, num)

static inline void *
i2c_get_adapdata(struct i2c_adapter *adap)
{
	return adap->data;
}

static inline void
i2c_set_adapdata(struct i2c_adapter *adap, void *data)
{
	adap->data = data;
}

int i2c_bit_add_bus(struct i2c_adapter *);

#endif
