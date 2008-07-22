/*	$OpenBSD: wpa-psk.c,v 1.3 2008/07/22 07:37:25 djm Exp $	*/

/*-
 * Copyright (c) 2008 Damien Bergamini <damien.bergamini@free.fr>
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

#include <sys/types.h>
#include <net80211/ieee80211.h>
#include <crypto/sha1.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>

#include "pbkdf2.h"

int
main(int argc, char **argv)
{
	extern char *__progname;
	const char *pass, *ssid;
	u_int len, ssid_len;
	u_int8_t keybuf[32];
	int i;

	if (argc != 3) {
		(void)fprintf(stderr, "usage: %s <ssid> <passphrase>\n",
		    __progname);
		exit(1);
	}
	ssid = argv[1];
	pass = argv[2];

	/* validate passphrase */
	len = strlen(pass);
	if (len < 8 || len > 63)
		errx(1, "passphrase must be between 8 and 63 characters");

	/* validate SSID */
	ssid_len = strlen(ssid);
	if (ssid_len == 0)
		errx(1, "invalid SSID");
	if (ssid_len > IEEE80211_NWID_LEN) {
		ssid_len = IEEE80211_NWID_LEN;
		warnx("truncating SSID to its first %d characters", ssid_len);
	}

	pkcs5_pbkdf2(pass, len, ssid, ssid_len, keybuf, sizeof(keybuf), 4096);

	printf("0x");
	for (i = 0; i < 32; i++)
		printf("%02x", keybuf[i]);
	printf("\n");

	return 0;
}
