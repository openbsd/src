/*	$OpenBSD: wpa-psk.c,v 1.1 2008/04/15 16:29:05 damien Exp $	*/

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
#include <err.h>

/*
 * HMAC-SHA-1 (from RFC 2202).
 */
static void
hmac_sha1(const u_int8_t *text, size_t text_len, const u_int8_t *key,
    size_t key_len, u_int8_t digest[SHA1_DIGEST_LENGTH])
{
	SHA1_CTX ctx;
	u_int8_t k_pad[SHA1_BLOCK_LENGTH];
	u_int8_t tk[SHA1_DIGEST_LENGTH];
	int i;

	if (key_len > SHA1_BLOCK_LENGTH) {
		SHA1Init(&ctx);
		SHA1Update(&ctx, key, key_len);
		SHA1Final(tk, &ctx);

		key = tk;
		key_len = SHA1_DIGEST_LENGTH;
	}

	bzero(k_pad, sizeof k_pad);
	bcopy(key, k_pad, key_len);
	for (i = 0; i < SHA1_BLOCK_LENGTH; i++)
		k_pad[i] ^= 0x36;

	SHA1Init(&ctx);
	SHA1Update(&ctx, k_pad, SHA1_BLOCK_LENGTH);
	SHA1Update(&ctx, text, text_len);
	SHA1Final(digest, &ctx);

	bzero(k_pad, sizeof k_pad);
	bcopy(key, k_pad, key_len);
	for (i = 0; i < SHA1_BLOCK_LENGTH; i++)
		k_pad[i] ^= 0x5c;

	SHA1Init(&ctx);
	SHA1Update(&ctx, k_pad, SHA1_BLOCK_LENGTH);
	SHA1Update(&ctx, digest, SHA1_DIGEST_LENGTH);
	SHA1Final(digest, &ctx);
}

/*
 * Password-Based Key Derivation Function 2 (PKCS #5 v2.0).
 * Code based on IEEE Std 802.11-2007, Annex H.4.2.
 */
static void
pbkdf2(const char *password, u_int len, const char *ssid, u_int ssid_len,
    u_int8_t key[32])
{
	u_int8_t keybuf[2 * SHA1_DIGEST_LENGTH];
	u_int8_t digest[IEEE80211_NWID_LEN + 4], digest1[SHA1_DIGEST_LENGTH];
	u_int8_t *output, count;
	int i, j;

	output = keybuf;
	for (count = 1; count <= 2; count++) {
		memcpy(digest, ssid, ssid_len);
		digest[ssid_len + 0] = 0;
		digest[ssid_len + 1] = 0;
		digest[ssid_len + 2] = 0;
		digest[ssid_len + 3] = count;
		hmac_sha1(digest, ssid_len + 4, password, len, digest1);
		memcpy(output, digest1, SHA1_DIGEST_LENGTH);

		for (i = 1; i < 4096; i++) {
			hmac_sha1(digest1, SHA1_DIGEST_LENGTH, password,
			    len, digest);
			memcpy(digest1, digest, SHA1_DIGEST_LENGTH);
			for (j = 0; j < SHA1_DIGEST_LENGTH; j++)
				output[j] ^= digest[j];
		}
		output += SHA1_DIGEST_LENGTH;
	}
	/* truncate output to its 256MSBs */
	memcpy(key, keybuf, 32);
}

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

	pbkdf2(pass, len, ssid, ssid_len, keybuf);

	printf("0x");
	for (i = 0; i < 32; i++)
		printf("%02x", keybuf[i]);
	printf("\n");

	return 0;
}
