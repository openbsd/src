/*	$OpenBSD: radius_msgauth.c,v 1.1 2015/07/20 23:52:29 yasuoka Exp $ */

/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
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

#include <openssl/hmac.h>

#include "radius.h"

#include "radius_local.h"

static void
radius_calc_message_authenticator(RADIUS_PACKET * packet, const char *secret,
    void *ma)
{
	const RADIUS_ATTRIBUTE	*attr;
	const RADIUS_ATTRIBUTE	*end;
	u_char			 zero16[16];
	HMAC_CTX		 ctx;
	int			 mdlen;

	memset(zero16, 0, sizeof(zero16));

	HMAC_Init(&ctx, secret, strlen(secret), EVP_md5());

	/*
	 * Traverse the radius packet.
	 */
	if (packet->request != NULL) {
		HMAC_Update(&ctx, (const u_char *)packet->pdata, 4);
		HMAC_Update(&ctx, (unsigned char *)packet->request->pdata
		    ->authenticator, 16);
	} else {
		HMAC_Update(&ctx, (const u_char *)packet->pdata,
		    sizeof(RADIUS_PACKET_DATA));
	}

	attr = ATTRS_BEGIN(packet->pdata);
	end = ATTRS_END(packet->pdata);

	for (; attr < end; ATTRS_ADVANCE(attr)) {
		if (attr->type == RADIUS_TYPE_MESSAGE_AUTHENTICATOR) {
			HMAC_Update(&ctx, (u_char *)attr, 2);
			HMAC_Update(&ctx, (u_char *)zero16, sizeof(zero16));
		} else
			HMAC_Update(&ctx, (u_char *)attr, (int) attr->length);
	}

	HMAC_Final(&ctx, (u_char *)ma, &mdlen);

	HMAC_cleanup(&ctx);
}

int
radius_put_message_authenticator(RADIUS_PACKET * packet, const char *secret)
{
	u_char	 ma[16];

	/*
	 * It is not required to initialize ma
	 * because content of Message-Authenticator attribute is assumed zero
	 * during calculation.
	 */
	if (radius_put_raw_attr(packet, RADIUS_TYPE_MESSAGE_AUTHENTICATOR,
		ma, sizeof(ma)) != 0)
		return (-1);

	return (radius_set_message_authenticator(packet, secret));
}

int
radius_set_message_authenticator(RADIUS_PACKET * packet, const char *secret)
{
	u_char	 ma[16];

	radius_calc_message_authenticator(packet, secret, ma);

	return (radius_set_raw_attr(packet, RADIUS_TYPE_MESSAGE_AUTHENTICATOR,
	    ma, sizeof(ma)));
}

int
radius_check_message_authenticator(RADIUS_PACKET * packet, const char *secret)
{
	int	 rval;
	size_t	 len;
	u_char	 ma0[16], ma1[16];

	radius_calc_message_authenticator(packet, secret, ma0);

	len = sizeof(ma1);
	if ((rval = radius_get_raw_attr(packet,
		    RADIUS_TYPE_MESSAGE_AUTHENTICATOR, ma1, &len)) != 0)
		return (rval);

	if (len != sizeof(ma1))
		return (-1);

	return (memcmp(ma0, ma1, sizeof(ma1)));
}
