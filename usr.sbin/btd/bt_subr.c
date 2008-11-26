/*	$OpenBSD: bt_subr.c,v 1.1 2008/11/26 06:51:43 uwe Exp $	*/
/*	$NetBSD: bluetooth.c,v 1.1 2006/06/19 15:44:36 gdamore Exp $	*/

/*
 * Copyright (c) 2001-2003 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 *
 * $Id: bt_subr.c,v 1.1 2008/11/26 06:51:43 uwe Exp $
 * $FreeBSD: src/lib/libbluetooth/bluetooth.c,v 1.2 2004/03/05 08:10:17 markm Exp $
 */

#include <stdio.h>
#include <string.h>

#include "btd.h"

static int bt_hex_byte   (char const *str);
static int bt_hex_nibble (char nibble);

char const *
bt_ntoa(bdaddr_t const *ba, char str[18])
{
	static char	buffer[24];

	if (str == NULL)
		str = buffer;

	snprintf(str, 18, "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
	    ba->b[5], ba->b[4], ba->b[3], ba->b[2], ba->b[1], ba->b[0]);

	return (str);
}

int
bt_aton(char const *str, bdaddr_t *ba)
{
	int	 i, b;
	char	*end = NULL;

	memset(ba, 0, sizeof(*ba));

	for (i = 5, end = strchr(str, ':');
	     i > 0 && *str != '\0' && end != NULL;
	     i --, str = end + 1, end = strchr(str, ':')) {
		switch (end - str) {
		case 1:
			b = bt_hex_nibble(str[0]);
			break;

		case 2:
			b = bt_hex_byte(str);
			break;

		default:
			b = -1;
			break;
		}

		if (b < 0)
			return (0);

		ba->b[i] = b;
	}

	if (i != 0 || end != NULL || *str == 0)
		return (0);

	switch (strlen(str)) {
	case 1:
		b = bt_hex_nibble(str[0]);
		break;

	case 2:
		b = bt_hex_byte(str);
		break;

	default:
		b = -1;
		break;
	}

	if (b < 0)
		return (0);

	ba->b[i] = b;

	return (1);
}

static int
bt_hex_byte(char const *str)
{
	int	n1, n2;

	if ((n1 = bt_hex_nibble(str[0])) < 0)
		return (-1);

	if ((n2 = bt_hex_nibble(str[1])) < 0)
		return (-1);

	return ((((n1 & 0x0f) << 4) | (n2 & 0x0f)) & 0xff);
}

static int
bt_hex_nibble(char nibble)
{
	if ('0' <= nibble && nibble <= '9')
		return (nibble - '0');

	if ('a' <= nibble && nibble <= 'f')
		return (nibble - 'a' + 0xa);

	if ('A' <= nibble && nibble <= 'F')
		return (nibble - 'A' + 0xa);

	return (-1);
}
