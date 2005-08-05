/*	$OpenBSD: apsvar.h,v 1.1 2005/08/05 03:52:32 jsg Exp $	*/
/*
 * Copyright (c) 2005 Jonathan Gray <jsg@openbsd.org>
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

#define APS_NUM_SENSORS		10

#define APS_SENSOR_XACCEL	0
#define APS_SENSOR_YACCEL	1
#define APS_SENSOR_XVAR		2
#define APS_SENSOR_YVAR		3
#define APS_SENSOR_TEMP1	4
#define APS_SENSOR_TEMP2	5
#define APS_SENSOR_KBACT	6
#define APS_SENSOR_MSACT	7
#define APS_SENSOR_LIDOPEN	8
#define APS_SENSOR_UNK		9

struct aps_softc {
	struct device sc_dev;

	bus_space_tag_t aps_iot;
	bus_space_handle_t aps_ioh;

	struct sensor sensors[APS_NUM_SENSORS];
	u_int numsensors;
	void (*refresh_sensor_data)(struct aps_softc *);

	struct sensor_rec aps_data;
};
