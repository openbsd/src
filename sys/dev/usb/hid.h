/*	$OpenBSD: hid.h,v 1.11 2014/05/12 09:50:44 mpi Exp $ */
/*	$NetBSD: hid.h,v 1.8 2002/07/11 21:14:25 augustss Exp $	*/
/*	$FreeBSD: src/sys/dev/usb/hid.h,v 1.7 1999/11/17 22:33:40 n_hibma Exp $ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

enum hid_kind {
	hid_input,
	hid_output,
	hid_feature,
	hid_collection,
	hid_endcollection,
	hid_none
};

struct hid_location {
	u_int32_t size;
	u_int32_t count;
	u_int32_t pos;
};

struct hid_item {
	/* Global */
	int32_t _usage_page;
	int32_t logical_minimum;
	int32_t logical_maximum;
	int32_t physical_minimum;
	int32_t physical_maximum;
	int32_t unit_exponent;
	int32_t unit;
	int32_t report_ID;
	/* Local */
	int32_t usage;
	int32_t usage_minimum;
	int32_t usage_maximum;
	int32_t designator_index;
	int32_t designator_minimum;
	int32_t designator_maximum;
	int32_t string_index;
	int32_t string_minimum;
	int32_t string_maximum;
	int32_t set_delimiter;
	/* Misc */
	int32_t collection;
	int collevel;
	enum hid_kind kind;
	u_int32_t flags;
	/* Location */
	struct hid_location loc;
	/* */
	struct hid_item *next;
};

struct	hid_data *hid_start_parse(const void *, int, enum hid_kind);
void	hid_end_parse(struct hid_data *);
int	hid_get_item(struct hid_data *, struct hid_item *);
int	hid_report_size(const void *, int, enum hid_kind, uint8_t);
int	hid_locate(const void *, int, int32_t, uint8_t, enum hid_kind,
	    struct hid_location *, uint32_t *);
int32_t	hid_get_data(const uint8_t *buf, int, struct hid_location *);
int	hid_is_collection(const void *, int, uint8_t, int32_t);
