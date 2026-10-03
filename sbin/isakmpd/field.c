/* $OpenBSD: field.c,v 1.23 2026/10/03 01:31:30 deraadt Exp $	 */
/* $EOM: field.c,v 1.11 2000/02/20 19:58:37 niklas Exp $	 */

/*
 * Copyright (c) 1998, 1999 Niklas Hallqvist.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "constants.h"
#include "field.h"
#include "log.h"
#include "util.h"

/*
 * Convert the unsigned LEN-sized number at BUF of network byteorder to a
 * 32-bit unsigned integer of host byteorder pointed to by VAL.
 */
static int
extract_val(u_int8_t *buf, size_t len, u_int32_t *val)
{
	switch (len) {
	case 1:
		*val = *buf;
		break;
	case 2:
		*val = decode_16(buf);
		break;
	case 4:
		*val = decode_32(buf);
		break;
	default:
		return -1;
	}
	return 0;
}

/* Return the numeric value of the field F of BUF.  */
u_int32_t
field_get_num(struct field *f, u_int8_t *buf)
{
	u_int32_t       val;

	if (extract_val(buf + f->offset, f->len, &val))
		return 0;
	return val;
}

/* Stash the number VAL into BUF's field F.  */
void
field_set_num(struct field *f, u_int8_t *buf, u_int32_t val)
{
	switch (f->len) {
	case 1:
		buf[f->offset] = val;
		break;
	case 2:
		encode_16(buf + f->offset, val);
		break;
	case 4:
		encode_32(buf + f->offset, val);
		break;
	}
}

/* Stash BUF's raw field F into VAL.  */
void
field_get_raw(struct field *f, u_int8_t *buf, u_int8_t *val)
{
	memcpy(val, buf + f->offset, f->len);
}

/* Stash the buffer VAL into BUF's field F.  */
void
field_set_raw(struct field *f, u_int8_t *buf, u_int8_t *val)
{
	memcpy(buf + f->offset, val, f->len);
}
