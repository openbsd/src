/*	$OpenBSD: radius_eapmsk.c,v 1.1 2015/07/20 23:52:29 yasuoka Exp $ */

/*-
 * Copyright (c) 2013 Internet Initiative Japan Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE"AUTHOR" AND CONTRIBUTORS AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/md5.h>

#include "radius.h"

#include "radius_local.h"

int 
radius_get_eap_msk(const RADIUS_PACKET * packet, void *buf, size_t * len,
    const char *secret)
{
	/*
	 * Unfortunately, the way to pass EAP MSK/EMSK over RADIUS
	 * is not standardized.
	 */
	uint8_t	 buf0[256];
	uint8_t	 buf1[256];
	size_t	 len0, len1;

	/*
	 * EAP MSK via MPPE keys
	 *
	 * MSK = MPPE-Recv-Key + MPPE-Send-Key + 32byte zeros
	 * http://msdn.microsoft.com/en-us/library/cc224635.aspx
	 */
	len0 = sizeof(buf0);
	len1 = sizeof(buf1);
	if (radius_get_mppe_recv_key_attr(packet, buf0, &len0, secret) == 0 &&
	    radius_get_mppe_send_key_attr(packet, buf1, &len1, secret) == 0) {
		if (len0 < 16 || len1 < 16)
			return (-1);
		if (*len < 64)
			return (-1);
		memcpy(buf, buf0, 16);
		memcpy(((char *)buf) + 16, buf1, 16);
		memset(((char *)buf) + 32, 0, 32);
		*len = 64;
		return (0);
	}

	return (-1);
}
