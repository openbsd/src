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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
/**@file
 * RADIUS 関連共通関数。
 *
 * @author	Yasuoka Masahiko
 * $Id: radius_common.c,v 1.1 2010/01/11 04:20:57 yasuoka Exp $
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <event.h>
#include <stdio.h>

#include <radius+.h>
#include <radiusconst.h>

#include "slist.h"
#include "npppd.h"
#include "npppd_local.h"

#include "radius_common.h"

/** RADIUS パケットの Framed-IP-Address アートリビュートを処理します。 */
void
ppp_proccess_radius_framed_ip_address(npppd_ppp *_this, RADIUS_PACKET *pkt)
{
	uint8_t len;
	u_char buf[256], *bufp;

	if ((_this->pppd->ip_assign_flags & NPPPD_IP_ASSIGN_RADIUS) ==  0)
		return;

	if (radius_get_raw_attr(pkt, RADIUS_TYPE_FRAMED_IP_ADDRESS, buf,
	    &len) != 0)
		return;

	bufp = buf;
	if (len == 4)
		GETLONG(_this->radius_framed_ip_address.s_addr, bufp);

}

/** npppd に設定された {@link ::radius_req_setting} を取り出します。 */
radius_req_setting *
npppd_get_radius_req_setting(npppd *_this)
{
	return &_this->rad_auth;
}
