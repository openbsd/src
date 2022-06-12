/*	$OpenBSD: rtkit.c,v 1.4 2022/06/12 16:00:12 kettenis Exp $	*/
/*
 * Copyright (c) 2021 Mark Kettenis <kettenis@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

#include <arm64/dev/aplmbox.h>
#include <arm64/dev/rtkit.h>

#define RTKIT_EP_MGMT			0
#define RTKIT_EP_CRASHLOG		1
#define RTKIT_EP_SYSLOG			2
#define RTKIT_EP_DEBUG			3
#define RTKIT_EP_IOREPORT		4

#define RTKIT_MGMT_TYPE(x)		(((x) >> 52) & 0xff)
#define RTKIT_MGMT_TYPE_SHIFT		52

#define RTKIT_MGMT_PWR_STATE(x)		(((x) >> 0) & 0xff)
#define RTKIT_MGMT_PWR_STATE_ON		0x20

#define RTKIT_MGMT_HELLO		1
#define RTKIT_MGMT_HELLO_ACK		2
#define RTKIT_MGMT_STARTEP		5
#define RTKIT_MGMT_IOP_PWR_STATE	6
#define RTKIT_MGMT_IOP_PWR_STATE_ACK	7
#define RTKIT_MGMT_EPMAP		8

#define RTKIT_MGMT_HELLO_MINVER(x)	(((x) >> 0) & 0xffff)
#define RTKIT_MGMT_HELLO_MINVER_SHIFT	0
#define RTKIT_MGMT_HELLO_MAXVER(x)	(((x) >> 16) & 0xffff)
#define RTKIT_MGMT_HELLO_MAXVER_SHIFT	16

#define RTKIT_MGMT_STARTEP_EP_SHIFT	32
#define RTKIT_MGMT_STARTEP_START	(1ULL << 1)

#define RTKIT_MGMT_EPMAP_LAST		(1ULL << 51)
#define RTKIT_MGMT_EPMAP_BASE(x)	(((x) >> 32) & 0x7)
#define RTKIT_MGMT_EPMAP_BASE_SHIFT	32
#define RTKIT_MGMT_EPMAP_BITMAP(x)	(((x) >> 0) & 0xffffffff)
#define RTKIT_MGMT_EPMAP_MORE		(1ULL << 0)

#define RTKIT_BUFFER_REQUEST		1
#define RTKIT_BUFFER_ADDR(x)		(((x) >> 0) & 0xfffffffffff)
#define RTKIT_BUFFER_SIZE(x)		(((x) >> 44) & 0xff)
#define RTKIT_BUFFER_SIZE_SHIFT		44

#define RTKIT_IOREPORT_UNKNOWN1		8
#define RTKIT_IOREPORT_UNKNOWN2		12

/* Versions we support. */
#define RTKIT_MINVER			11
#define RTKIT_MAXVER			12

struct rtkit_state {
	struct mbox_channel	*mc;
	struct rtkit		*rk;
	int			pwrstate;
	uint64_t		epmap;
	void			(*callback[32])(void *, uint64_t);
	void			*arg[32];
};

int
rtkit_recv(struct mbox_channel *mc, struct aplmbox_msg *msg)
{
	int error, timo;

	for (timo = 0; timo < 10000; timo++) {
		error = mbox_recv(mc, msg, sizeof(*msg));
		if (error == 0)
			break;
		delay(10);
	}

	return error;
}

int
rtkit_send(struct mbox_channel *mc, uint32_t endpoint,
    uint64_t type, uint64_t data)
{
	struct aplmbox_msg msg;

	msg.data0 = (type << RTKIT_MGMT_TYPE_SHIFT) | data;
	msg.data1 = endpoint;
	return mbox_send(mc, &msg, sizeof(msg));
}

bus_addr_t
rtkit_alloc(struct rtkit *rk, bus_size_t size)
{
	bus_dma_segment_t seg;
	int nsegs;

	if (bus_dmamem_alloc(rk->rk_dmat, size, 16384, 0,
	    &seg, 1, &nsegs, BUS_DMA_WAITOK | BUS_DMA_ZERO))
		return (bus_addr_t)-1;

	return seg.ds_addr;
}

int
rtkit_start(struct rtkit_state *state, uint32_t endpoint)
{
	struct mbox_channel *mc = state->mc;
	uint64_t reply;

	reply = ((uint64_t)endpoint << RTKIT_MGMT_STARTEP_EP_SHIFT);
	reply |= RTKIT_MGMT_STARTEP_START;
	return rtkit_send(mc, RTKIT_EP_MGMT, RTKIT_MGMT_STARTEP, reply);
}

int
rtkit_handle_mgmt(struct rtkit_state *state, struct aplmbox_msg *msg)
{
	struct mbox_channel *mc = state->mc;
	uint64_t minver, maxver, ver;
	uint64_t base, bitmap, reply;
	uint32_t endpoint;
	int error;

	switch (RTKIT_MGMT_TYPE(msg->data0)) {
	case RTKIT_MGMT_HELLO:
		minver = RTKIT_MGMT_HELLO_MINVER(msg->data0);
		maxver = RTKIT_MGMT_HELLO_MAXVER(msg->data0);
		if (minver > RTKIT_MAXVER) {
			printf("%s: unsupported minimum firmware version %lld\n",
			    __func__, minver);
			return EINVAL;
		}
		if (maxver < RTKIT_MINVER) {
			printf("%s: unsupported maximum firmware version %lld\n",
			    __func__, maxver);
			return EINVAL;
		}
		ver = min(RTKIT_MAXVER, maxver);
		error = rtkit_send(mc, RTKIT_EP_MGMT, RTKIT_MGMT_HELLO_ACK,
		    (ver << RTKIT_MGMT_HELLO_MINVER_SHIFT) |
		    (ver << RTKIT_MGMT_HELLO_MAXVER_SHIFT));
		if (error)
			return error;
		break;

	case RTKIT_MGMT_IOP_PWR_STATE_ACK:
		state->pwrstate = RTKIT_MGMT_PWR_STATE(msg->data0);
		break;

	case RTKIT_MGMT_EPMAP:
		base = RTKIT_MGMT_EPMAP_BASE(msg->data0);
		bitmap = RTKIT_MGMT_EPMAP_BITMAP(msg->data0);
		state->epmap |= (bitmap << (base * 32));
		reply = (base << RTKIT_MGMT_EPMAP_BASE_SHIFT);
		if (msg->data0 & RTKIT_MGMT_EPMAP_LAST)
			reply |= RTKIT_MGMT_EPMAP_LAST;
		else
			reply |= RTKIT_MGMT_EPMAP_MORE;
		error = rtkit_send(state->mc, RTKIT_EP_MGMT,
		    RTKIT_MGMT_EPMAP, reply);
		if (error)
			return error;
		if (msg->data0 & RTKIT_MGMT_EPMAP_LAST) {
			for (endpoint = 1; endpoint < 32; endpoint++) {
				if ((state->epmap & (1ULL << endpoint)) == 0)
					continue;

				switch (endpoint) {
				case RTKIT_EP_CRASHLOG:
				case RTKIT_EP_SYSLOG:
				case RTKIT_EP_DEBUG:
				case RTKIT_EP_IOREPORT:
					error = rtkit_start(state, endpoint);
					if (error)
						return error;
					break;
				default:
					printf("%s: skipping endpoint %d\n",
					    __func__, endpoint);
					break;
				}
			}
		}
		break;
	default:
		printf("%s: unhandled management event 0x%016lld\n",
		    __func__, msg->data0);
		return EIO;
	}

	return 0;
}

int
rtkit_handle_crashlog(struct rtkit_state *state, struct aplmbox_msg *msg)
{
	struct mbox_channel *mc = state->mc;
	struct rtkit *rk = state->rk;
	bus_addr_t addr;
	bus_size_t size;
	int error;

	switch (RTKIT_MGMT_TYPE(msg->data0)) {
	case RTKIT_BUFFER_REQUEST:
		addr = RTKIT_BUFFER_ADDR(msg->data0);
		size = RTKIT_BUFFER_SIZE(msg->data0);
		if (addr)
			break;

		if (rk) {
			addr = rtkit_alloc(rk, size << PAGE_SHIFT);
			if (addr == (bus_addr_t)-1)
				return ENOMEM;
			error = rk->rk_map(rk->rk_cookie, addr,
			    size << PAGE_SHIFT);
			if (error)
				return error;
		}

		error = rtkit_send(mc, RTKIT_EP_CRASHLOG, RTKIT_BUFFER_REQUEST,
		    (size << RTKIT_BUFFER_SIZE_SHIFT) | addr);
		if (error)
			return error;
		break;
	default:
		printf("%s: unhandled crashlog event 0x%016llx\n",
		    __func__, msg->data0);
		return EIO;
	}

	return 0;
}

int
rtkit_handle_ioreport(struct rtkit_state *state, struct aplmbox_msg *msg)
{
	struct mbox_channel *mc = state->mc;
	struct rtkit *rk = state->rk;
	bus_addr_t addr;
	bus_size_t size;
	int error;

	switch (RTKIT_MGMT_TYPE(msg->data0)) {
	case RTKIT_BUFFER_REQUEST:
		addr = RTKIT_BUFFER_ADDR(msg->data0);
		size = RTKIT_BUFFER_SIZE(msg->data0);
		if (addr)
			break;

		if (rk) {
			addr = rtkit_alloc(rk, size << PAGE_SHIFT);
			if (addr == (bus_addr_t)-1)
				return ENOMEM;
			error = rk->rk_map(rk->rk_cookie, addr,
			    size << PAGE_SHIFT);
			if (error)
				return error;
		}

		error = rtkit_send(mc, RTKIT_EP_IOREPORT, RTKIT_BUFFER_REQUEST,
		    (size << RTKIT_BUFFER_SIZE_SHIFT) | addr);
		if (error)
			return error;
		break;
	case RTKIT_IOREPORT_UNKNOWN1:
	case RTKIT_IOREPORT_UNKNOWN2:
		/* These unknown events have to be acked to make progress. */
		error = rtkit_send(mc, RTKIT_EP_IOREPORT,
		    RTKIT_MGMT_TYPE(msg->data0), msg->data0);
		if (error)
			return error;
		break;
	default:
		printf("%s: unhandled ioreport event 0x%016llx\n",
		    __func__, msg->data0);
		return EIO;
	}

	return 0;
}

int
rtkit_poll(struct rtkit_state *state)
{
	struct mbox_channel *mc = state->mc;
	struct aplmbox_msg msg;
	void (*callback)(void *, uint64_t);
	void *arg;
	uint32_t endpoint;
	int error;

	error = rtkit_recv(mc, &msg);
	if (error)
		return error;

	endpoint = msg.data1;
	switch (endpoint) {
	case RTKIT_EP_MGMT:
		error = rtkit_handle_mgmt(state, &msg);
		if (error)
			return error;
		break;
	case RTKIT_EP_CRASHLOG:
		error = rtkit_handle_crashlog(state, &msg);
		if (error)
			return error;
		break;
	case RTKIT_EP_IOREPORT:
		error = rtkit_handle_ioreport(state, &msg);
		if (error)
			return error;
		break;
	default:
		if (endpoint >= 32 && endpoint < 64 && 
		    state->callback[endpoint - 32]) {
			callback = state->callback[endpoint - 32];
			arg = state->arg[endpoint - 32];
			callback(arg, msg.data0);
			break;
		}

		printf("%s: unhandled endpoint %d\n", __func__, msg.data1);
		return EIO;
	}

	return 0;
}

void
rtkit_rx_callback(void *cookie)
{
	rtkit_poll(cookie);
}

struct rtkit_state *
rtkit_init(int node, const char *name, struct rtkit *rk)
{
	struct rtkit_state *state;
	struct mbox_client client;

	state = malloc(sizeof(*state), M_DEVBUF, M_WAITOK | M_ZERO);
	client.mc_rx_callback = rtkit_rx_callback;
	client.mc_rx_arg = state;
	state->mc = mbox_channel(node, name, &client);
	if (state->mc == NULL) {
		free(state, M_DEVBUF, sizeof(*state));
		return NULL;
	}
	state->rk = rk;

	return state;
}

int
rtkit_boot(struct rtkit_state *state)
{
	struct mbox_channel *mc = state->mc;
	int error;

	/* Wake up! */
	error = rtkit_send(mc, RTKIT_EP_MGMT, RTKIT_MGMT_IOP_PWR_STATE,
	    RTKIT_MGMT_PWR_STATE_ON);
	if (error)
		return error;

	while (state->pwrstate != RTKIT_MGMT_PWR_STATE_ON)
		rtkit_poll(state);

	return 0;
}

int
rtkit_start_endpoint(struct rtkit_state *state, uint32_t endpoint,
    void (*callback)(void *, uint64_t), void *arg)
{
	if (endpoint < 32 || endpoint >= 64)
		return EINVAL;

	if ((state->epmap & (1ULL << endpoint)) == 0)
		return EINVAL;

	state->callback[endpoint - 32] = callback;
	state->arg[endpoint - 32] = arg;
	return rtkit_start(state, endpoint);
}

int
rtkit_send_endpoint(struct rtkit_state *state, uint32_t endpoint,
    uint64_t data)
{
	return rtkit_send(state->mc, endpoint, 0, data);
}
