/*	$OpenBSD: sdp_query.c,v 1.1 2008/11/26 21:48:30 uwe Exp $	*/
/*	$NetBSD: sdp.c,v 1.5 2008/04/20 19:34:23 plunky Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 2004 Maksim Yevmenkin <m_evmenkin@yahoo.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>

#include <dev/bluetooth/btdev.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <err.h>
#include <errno.h>
#include <sdp.h>
#include <stdlib.h>
#include <usbhid.h>

#include "btd.h"

static int hid_mode(uint8_t *, int32_t);

static int32_t parse_l2cap_psm(sdp_attr_t *);
static int32_t parse_rfcomm_channel(sdp_attr_t *);
static int32_t parse_hid_descriptor(sdp_attr_t *);
static int32_t parse_boolean(sdp_attr_t *);

static int config_hid(struct btdev_attach_args *);
static int config_hset(struct btdev_attach_args *);
static int config_hf(struct btdev_attach_args *);

uint16_t hid_services[] = {
	SDP_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE
};

uint32_t hid_attrs[] = {
	SDP_ATTR_RANGE(	SDP_ATTR_PROTOCOL_DESCRIPTOR_LIST,
			SDP_ATTR_PROTOCOL_DESCRIPTOR_LIST),
	SDP_ATTR_RANGE( SDP_ATTR_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS,
			SDP_ATTR_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS),
	SDP_ATTR_RANGE(	0x0205,		/* HIDReconnectInitiate */
			0x0206),	/* HIDDescriptorList */
	SDP_ATTR_RANGE(	0x0209,		/* HIDBatteryPower */
			0x0209),
	SDP_ATTR_RANGE(	0x020d,		/* HIDNormallyConnectable */
			0x020d)
};

uint16_t hset_services[] = {
	SDP_SERVICE_CLASS_HEADSET
};

uint32_t hset_attrs[] = {
	SDP_ATTR_RANGE(	SDP_ATTR_PROTOCOL_DESCRIPTOR_LIST,
			SDP_ATTR_PROTOCOL_DESCRIPTOR_LIST),
};

uint16_t hf_services[] = {
	SDP_SERVICE_CLASS_HANDSFREE_AUDIO_GATEWAY
};

uint32_t hf_attrs[] = {
	SDP_ATTR_RANGE(	SDP_ATTR_PROTOCOL_DESCRIPTOR_LIST,
			SDP_ATTR_PROTOCOL_DESCRIPTOR_LIST),
};

#define NUM(v)		(sizeof(v) / sizeof(v[0]))

static struct {
	const char		*name;
	int			(*handler)(struct btdev_attach_args *);
	const char		*description;
	uint16_t		*services;
	int			nservices;
	uint32_t		*attrs;
	int			nattrs;
} cfgtype[] = {
    {
	"HID",		config_hid,	"Human Interface Device",
	hid_services,	NUM(hid_services),
	hid_attrs,	NUM(hid_attrs),
    },
    {
	"HSET",		config_hset,	"Headset",
	hset_services,	NUM(hset_services),
	hset_attrs,	NUM(hset_attrs),
    },
    {
	"HF",		config_hf,	"Handsfree",
	hf_services,	NUM(hf_services),
	hf_attrs,	NUM(hf_attrs),
    },
};

static sdp_attr_t	values[8];
static uint8_t		buffer[NUM(values)][512];

int
sdp_query(struct sdp_session *sess, struct btdev_attach_args *dict,
    bdaddr_t *laddr, bdaddr_t *raddr, const char *service)
{
	int rv, i;

	for (i = 0 ; i < NUM(values) ; i++) {
		values[i].flags = SDP_ATTR_INVALID;
		values[i].attr = 0;
		values[i].vlen = sizeof(buffer[i]);
		values[i].value = buffer[i];
	}

	for (i = 0 ; i < NUM(cfgtype) ; i++) {
		if (strcasecmp(service, cfgtype[i].name) == 0) {
			rv = sdp_search(sess,
				cfgtype[i].nservices, cfgtype[i].services,
				cfgtype[i].nattrs, cfgtype[i].attrs,
				NUM(values), values);

			if (rv != 0) {
				errno = sess->error;
				return -1;
			}

			rv = (*cfgtype[i].handler)(dict);
			if (rv != 0)
				return -1;

			return 0;
		}
	}

	fatalx("bad device type");
	/* NOTREACHED */
	return -1;
}

/*
 * Configure HID results
 */
static int
config_hid(struct btdev_attach_args *dict)
{
	int32_t control_psm, interrupt_psm,
		reconnect_initiate, battery_power,
		normally_connectable, hid_length;
	uint8_t *hid_descriptor;
	int i;

	control_psm = -1;
	interrupt_psm = -1;
	reconnect_initiate = -1;
	normally_connectable = 0;
	battery_power = 0;
	hid_descriptor = NULL;
	hid_length = -1;

	for (i = 0; i < NUM(values) ; i++) {
		if (values[i].flags != SDP_ATTR_OK)
			continue;

		switch (values[i].attr) {
		case SDP_ATTR_PROTOCOL_DESCRIPTOR_LIST:
			control_psm = parse_l2cap_psm(&values[i]);
			break;

		case SDP_ATTR_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS:
			interrupt_psm = parse_l2cap_psm(&values[i]);
			break;

		case 0x0205: /* HIDReconnectInitiate */
			reconnect_initiate = parse_boolean(&values[i]);
			break;

		case 0x0206: /* HIDDescriptorList */
			if (parse_hid_descriptor(&values[i]) == 0) {
				hid_descriptor = values[i].value;
				hid_length = values[i].vlen;
			}
			break;

		case 0x0209: /* HIDBatteryPower */
			battery_power = parse_boolean(&values[i]);
			break;

		case 0x020d: /* HIDNormallyConnectable */
			normally_connectable = parse_boolean(&values[i]);
			break;
		}
	}

	if (control_psm == -1
	    || interrupt_psm == -1
	    || reconnect_initiate == -1
	    || hid_descriptor == NULL
	    || hid_length == -1)
		return ENOATTR;

	dict->bd_type = BTDEV_HID;
	dict->bd_hid.hid_ctl = control_psm;
	dict->bd_hid.hid_int = interrupt_psm;
	dict->bd_hid.hid_desc = hid_descriptor;
	dict->bd_hid.hid_dlen = hid_length;
	dict->bd_mode = hid_mode(hid_descriptor, hid_length);

	if (!reconnect_initiate)
		dict->bd_hid.hid_flags |= BTHID_INITIATE;

	return 0;
}

/*
 * Configure HSET results
 */
static int
config_hset(struct btdev_attach_args *dict)
{
	uint32_t channel;
	int i;

	channel = -1;

	for (i = 0; i < NUM(values) ; i++) {
		if (values[i].flags != SDP_ATTR_OK)
			continue;

		switch (values[i].attr) {
		case SDP_ATTR_PROTOCOL_DESCRIPTOR_LIST:
			channel = parse_rfcomm_channel(&values[i]);
			break;
		}
	}

	if (channel == -1)
		return ENOATTR;

	dict->bd_type = BTDEV_HSET;
	dict->bd_hset.hset_channel = channel;

	return 0;
}

/*
 * Configure HF results
 */
static int
config_hf(struct btdev_attach_args *dict)
{
	uint32_t channel;
	int i;

	channel = -1;

	for (i = 0 ; i < NUM(values) ; i++) {
		if (values[i].flags != SDP_ATTR_OK)
			continue;

		switch (values[i].attr) {
		case SDP_ATTR_PROTOCOL_DESCRIPTOR_LIST:
			channel = parse_rfcomm_channel(&values[i]);
			break;
		}
	}

	if (channel == -1)
		return ENOATTR;

	dict->bd_type = BTDEV_HF;
	dict->bd_hf.hf_listen = 1;
	dict->bd_hf.hf_channel = channel;
	return 0;
}

/*
 * Parse [additional] protocol descriptor list for L2CAP PSM
 *
 * seq8 len8				2
 *	seq8 len8			2
 *		uuid16 value16		3	L2CAP
 *		uint16 value16		3	PSM
 *	seq8 len8			2
 *		uuid16 value16		3	HID Protocol
 *				      ===
 *				       15
 */

static int32_t
parse_l2cap_psm(sdp_attr_t *a)
{
	uint8_t	*ptr = a->value;
	uint8_t	*end = a->value + a->vlen;
	int32_t	 type, len, uuid, psm;

	if (end - ptr < 15)
		return (-1);

	if (a->attr == SDP_ATTR_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS) {
		SDP_GET8(type, ptr);
		switch (type) {
		case SDP_DATA_SEQ8:
			SDP_GET8(len, ptr);
			break;

		case SDP_DATA_SEQ16:
			SDP_GET16(len, ptr);
			break;

		case SDP_DATA_SEQ32:
			SDP_GET32(len, ptr);
			break;

		default:
			return (-1);
		}
		if (ptr + len > end)
			return (-1);
	}

	SDP_GET8(type, ptr);
	switch (type) {
	case SDP_DATA_SEQ8:
		SDP_GET8(len, ptr);
		break;

	case SDP_DATA_SEQ16:
		SDP_GET16(len, ptr);
		break;

	case SDP_DATA_SEQ32:
		SDP_GET32(len, ptr);
		break;

	default:
		return (-1);
	}
	if (ptr + len > end)
		return (-1);

	/* Protocol */
	SDP_GET8(type, ptr);
	switch (type) {
	case SDP_DATA_SEQ8:
		SDP_GET8(len, ptr);
		break;

	case SDP_DATA_SEQ16:
		SDP_GET16(len, ptr);
		break;

	case SDP_DATA_SEQ32:
		SDP_GET32(len, ptr);
		break;

	default:
		return (-1);
	}
	if (ptr + len > end)
		return (-1);

	/* UUID */
	if (ptr + 3 > end)
		return (-1);
	SDP_GET8(type, ptr);
	switch (type) {
	case SDP_DATA_UUID16:
		SDP_GET16(uuid, ptr);
		if (uuid != SDP_UUID_PROTOCOL_L2CAP)
			return (-1);
		break;

	case SDP_DATA_UUID32:  /* XXX FIXME can we have 32-bit UUID */
	case SDP_DATA_UUID128: /* XXX FIXME can we have 128-bit UUID */
	default:
		return (-1);
	}

	/* PSM */
	if (ptr + 3 > end)
		return (-1);
	SDP_GET8(type, ptr);
	if (type != SDP_DATA_UINT16)
		return (-1);
	SDP_GET16(psm, ptr);

	return (psm);
}

/*
 * Parse HID descriptor string
 *
 * seq8 len8			2
 *	seq8 len8		2
 *		uint8 value8	2
 *		str value	3
 *			      ===
 *			        9
 */

static int32_t
parse_hid_descriptor(sdp_attr_t *a)
{
	uint8_t	*ptr = a->value;
	uint8_t	*end = a->value + a->vlen;
	int32_t	 type, len, descriptor_type;

	if (end - ptr < 9)
		return (-1);

	SDP_GET8(type, ptr);
	switch (type) {
	case SDP_DATA_SEQ8:
		SDP_GET8(len, ptr);
		break;

	case SDP_DATA_SEQ16:
		SDP_GET16(len, ptr);
		break;

	case SDP_DATA_SEQ32:
		SDP_GET32(len, ptr);
		break;

	default:
		return (-1);
	}
	if (ptr + len > end)
		return (-1);

	while (ptr < end) {
		/* Descriptor */
		SDP_GET8(type, ptr);
		switch (type) {
		case SDP_DATA_SEQ8:
			if (ptr + 1 > end)
				return (-1);
			SDP_GET8(len, ptr);
			break;

		case SDP_DATA_SEQ16:
			if (ptr + 2 > end)
				return (-1);
			SDP_GET16(len, ptr);
			break;

		case SDP_DATA_SEQ32:
			if (ptr + 4 > end)
				return (-1);
			SDP_GET32(len, ptr);
			break;

		default:
			return (-1);
		}

		/* Descripor type */
		if (ptr + 1 > end)
			return (-1);
		SDP_GET8(type, ptr);
		if (type != SDP_DATA_UINT8 || ptr + 1 > end)
			return (-1);
		SDP_GET8(descriptor_type, ptr);

		/* Descriptor value */
		if (ptr + 1 > end)
			return (-1);
		SDP_GET8(type, ptr);
		switch (type) {
		case SDP_DATA_STR8:
			if (ptr + 1 > end)
				return (-1);
			SDP_GET8(len, ptr);
			break;

		case SDP_DATA_STR16:
			if (ptr + 2 > end)
				return (-1);
			SDP_GET16(len, ptr);
			break;

		case SDP_DATA_STR32:
			if (ptr + 4 > end)
				return (-1);
			SDP_GET32(len, ptr);
			break;

		default:
			return (-1);
		}
		if (ptr + len > end)
			return (-1);

		if (descriptor_type == UDESC_REPORT && len > 0) {
			a->value = ptr;
			a->vlen = len;

			return (0);
		}

		ptr += len;
	}

	return (-1);
}

/*
 * Parse boolean value
 *
 * bool8 int8
 */

static int32_t
parse_boolean(sdp_attr_t *a)
{
	if (a->vlen != 2 || a->value[0] != SDP_DATA_BOOL)
		return (-1);

	return (a->value[1]);
}

/*
 * Parse protocol descriptor list for the RFCOMM channel
 *
 * seq8 len8				2
 *	seq8 len8			2
 *		uuid16 value16		3	L2CAP
 *	seq8 len8			2
 *		uuid16 value16		3	RFCOMM
 *		uint8 value8		2	channel
 *				      ===
 *				       14
 */

static int32_t
parse_rfcomm_channel(sdp_attr_t *a)
{
	uint8_t	*ptr = a->value;
	uint8_t	*end = a->value + a->vlen;
	int32_t	 type, len, uuid, channel;

	if (end - ptr < 14)
		return (-1);

	SDP_GET8(type, ptr);
	switch (type) {
	case SDP_DATA_SEQ8:
		SDP_GET8(len, ptr);
		break;

	case SDP_DATA_SEQ16:
		SDP_GET16(len, ptr);
		break;

	case SDP_DATA_SEQ32:
		SDP_GET32(len, ptr);
		break;

	default:
		return (-1);
	}
	if (ptr + len > end)
		return (-1);

	/* Protocol */
	SDP_GET8(type, ptr);
	switch (type) {
	case SDP_DATA_SEQ8:
		SDP_GET8(len, ptr);
		break;

	case SDP_DATA_SEQ16:
		SDP_GET16(len, ptr);
		break;

	case SDP_DATA_SEQ32:
		SDP_GET32(len, ptr);
		break;

	default:
		return (-1);
	}
	if (ptr + len > end)
		return (-1);

	/* UUID */
	if (ptr + 3 > end)
		return (-1);
	SDP_GET8(type, ptr);
	switch (type) {
	case SDP_DATA_UUID16:
		SDP_GET16(uuid, ptr);
		if (uuid != SDP_UUID_PROTOCOL_L2CAP)
			return (-1);
		break;

	case SDP_DATA_UUID32:  /* XXX FIXME can we have 32-bit UUID */
	case SDP_DATA_UUID128: /* XXX FIXME can we have 128-bit UUID */
	default:
		return (-1);
	}

	/* Protocol */
	SDP_GET8(type, ptr);
	switch (type) {
	case SDP_DATA_SEQ8:
		SDP_GET8(len, ptr);
		break;

	case SDP_DATA_SEQ16:
		SDP_GET16(len, ptr);
		break;

	case SDP_DATA_SEQ32:
		SDP_GET32(len, ptr);
		break;

	default:
		return (-1);
	}
	if (ptr + len > end)
		return (-1);

	/* UUID */
	if (ptr + 3 > end)
		return (-1);
	SDP_GET8(type, ptr);
	switch (type) {
	case SDP_DATA_UUID16:
		SDP_GET16(uuid, ptr);
		if (uuid != SDP_UUID_PROTOCOL_RFCOMM)
			return (-1);
		break;

	case SDP_DATA_UUID32:  /* XXX FIXME can we have 32-bit UUID */
	case SDP_DATA_UUID128: /* XXX FIXME can we have 128-bit UUID */
	default:
		return (-1);
	}

	/* channel */
	if (ptr + 2 > end)
		return (-1);

	SDP_GET8(type, ptr);
	if (type != SDP_DATA_UINT8)
		return (-1);

	SDP_GET8(channel, ptr);

	return (channel);
}

/*
 * return appropriate mode for HID descriptor
 */
static int
hid_mode(uint8_t *desc, int32_t dlen)
{
	report_desc_t r;
	hid_data_t d;
	struct hid_item h;
	int mode;

	mode = BTDEV_MODE_AUTH;	/* default */

	r = hid_use_report_desc(desc, dlen);
	if (r == NULL)
		err(EXIT_FAILURE, "hid_use_report_desc");

	d = hid_start_parse(r, ~0, -1);
	while (hid_get_item(d, &h) > 0) {
		if (h.kind == hid_collection
		    && HID_PAGE(h.usage) == HUP_GENERIC_DESKTOP
		    && HID_USAGE(h.usage) == HUG_KEYBOARD)
			mode = BTDEV_MODE_ENCRYPT;
	}

	hid_end_parse(d);
	hid_dispose_report_desc(r);

	return mode;
}
