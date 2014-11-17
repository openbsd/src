/* $OpenBSD: cryptutil.c,v 1.2 2014/11/17 16:47:28 tedu Exp $ */
/*
 * Copyright (c) 2014 Ted Unangst <tedu@openbsd.org>
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <login_cap.h>
#include <errno.h>

int
crypt_checkpass(const char *pass, const char *goodhash)
{
	char dummy[_PASSWORD_LEN];
	char *res;

	if (goodhash == NULL) {
		/* fake it */
		bcrypt_newhash(pass, 8, dummy, sizeof(dummy));
		goto fail;
	}

	/* empty password */
	if (strlen(goodhash) == 0 && strlen(pass) == 0)
		return 0;

	if (goodhash[0] == '$' && goodhash[1] == '2') {
		return bcrypt_checkpass(pass, goodhash);
	}

	/* have to do it the hard way */
	res = crypt(pass, goodhash);
	if (strlen(res) != strlen(goodhash) ||
	    timingsafe_bcmp(res, goodhash, strlen(goodhash)) != 0) {
		goto fail;
	}

	return 0;
fail:
	errno = EACCES;
	return -1;
}

int
crypt_newhash(const char *pass, login_cap_t *lc, char *hash, size_t hashlen)
{
	int rv = -1;
	char *pref;
	char *defaultpref = "blowfish,8";
	const char *errstr;
	int rounds;

	if (lc == NULL ||
	    (pref = login_getcapstr(lc, "localcipher", NULL, NULL)) == NULL)
		pref = defaultpref;
	if (strncmp(pref, "blowfish,", 9) != 0) {
		errno = EINVAL;
		goto err;
	}
	rounds = strtonum(pref + 9, 4, 31, &errstr);
	if (errstr)
		goto err;
	rv = bcrypt_newhash(pass, rounds, hash, hashlen);

err:
	if (pref != defaultpref)
		free(pref);
	return rv;
}
