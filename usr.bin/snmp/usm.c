/*	$OpenBSD: usm.c,v 1.7 2022/01/05 16:41:07 tb Exp $	*/

/*
 * Copyright (c) 2019 Martijn van Duren <martijn@openbsd.org>
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

#include <sys/time.h>

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <ber.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "smi.h"
#include "snmp.h"
#include "usm.h"

#define USM_MAX_DIGESTLEN 48
#define USM_MAX_TIMEWINDOW 150
#define USM_SALTOFFSET 8

struct usm_sec {
	struct snmp_sec snmp;
	char *user;
	size_t userlen;
	int engineidset;
	char *engineid;
	size_t engineidlen;
	enum usm_key_level authlevel;
	const EVP_MD *digest;
	char *authkey;
	enum usm_key_level privlevel;
	const EVP_CIPHER *cipher;
	char *privkey;
	int bootsset;
	uint32_t boots;
	int timeset;
	uint32_t time;
	struct timespec timecheck;
};

struct usm_cookie {
	size_t digestoffset;
	long long salt;
	uint32_t boots;
	uint32_t time;
};

static int usm_doinit(struct snmp_agent *);
static char *usm_genparams(struct snmp_agent *, size_t *, void **);
static int usm_finalparams(struct snmp_agent *, char *, size_t, size_t, void *);
static struct ber_element *usm_encpdu(struct snmp_agent *agent,
    struct ber_element *pdu, void *cookie);
static char *usm_crypt(const EVP_CIPHER *, int, char *, struct usm_cookie *,
    char *, size_t, size_t *);
static int usm_parseparams(struct snmp_agent *, char *, size_t, off_t, char *,
    size_t, uint8_t, void **);
struct ber_element *usm_decpdu(struct snmp_agent *, char *, size_t, void *);
static void usm_digest_pos(void *, size_t);
static void usm_free(void *);
static char *usm_passwd2mkey(const EVP_MD *, const char *);
static char *usm_mkey2lkey(struct usm_sec *, const EVP_MD *, const char *);
static size_t usm_digestlen(const EVP_MD *);

struct snmp_sec *
usm_init(const char *user, size_t userlen)
{
	struct snmp_sec *sec;
	struct usm_sec *usm;

	if (user == NULL || user[0] == '\0') {
		errno = EINVAL;
		return NULL;
	}

	if ((sec = malloc(sizeof(*sec))) == NULL)
		return NULL;

	if ((usm = calloc(1, sizeof(struct usm_sec))) == NULL) {
		free(sec);
		return NULL;
	}
	if ((usm->user = malloc(userlen)) == NULL) {
		free(sec);
		free(usm);
		return NULL;
	}
	memcpy(usm->user, user, userlen);
	usm->userlen = userlen;

	sec->model = SNMP_SEC_USM;
	sec->init = usm_doinit;
	sec->genparams = usm_genparams;
	sec->encpdu = usm_encpdu;
	sec->parseparams = usm_parseparams;
	sec->decpdu = usm_decpdu;
	sec->finalparams = usm_finalparams;
	sec->free = usm_free;
	sec->freecookie = free;
	sec->data = usm;
	return sec;
}

static int
usm_doinit(struct snmp_agent *agent)
{
	struct ber_element *ber;
	struct usm_sec *usm = agent->v3->sec->data;
	int level;
	size_t userlen;

	if (usm->engineidset && usm->bootsset && usm->timeset)
		return 0;

	level = agent->v3->level;
	agent->v3->level = SNMP_MSGFLAG_REPORT;
	userlen = usm->userlen;
	usm->userlen = 0;

	if ((ber = snmp_get(agent, NULL, 0)) == NULL) {
		agent->v3->level = level;
		usm->userlen = userlen;
		return -1;
	}
	ober_free_element(ber);

	agent->v3->level = level;
	usm->userlen = userlen;

	 /*
	  * Ugly hack for HP Laserjet:
	  * This device returns the engineid on probing, but only returns boots
	  * and time after a packet has been sent with full auth/enc.
	  */
	if (!usm->engineidset || !usm->bootsset || !usm->timeset) {
		if ((ber = snmp_get(agent, NULL, 0)) == NULL)
			return -1;
		ober_free_element(ber);
	}
	return 0;
}

static char *
usm_genparams(struct snmp_agent *agent, size_t *len, void **cookie)
{
	struct ber ber;
	struct ber_element *params, *digestelm;
	struct usm_sec *usm = agent->v3->sec->data;
	char digest[USM_MAX_DIGESTLEN];
	size_t digestlen = 0, saltlen = 0;
	char *secparams = NULL;
	ssize_t berlen = 0;
	struct usm_cookie *usmcookie;
	struct timespec now, timediff;

	bzero(digest, sizeof(digest));

	if ((usmcookie = calloc(1, sizeof(*usmcookie))) == NULL)
		return NULL;
	*cookie = usmcookie;

	arc4random_buf(&(usmcookie->salt), sizeof(usmcookie->salt));
	if (usm->timeset) {
		if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
			free(usmcookie);
			return NULL;
		}
		timespecsub(&now, &(usm->timecheck), &timediff);
		usmcookie->time = usm->time + timediff.tv_sec;
	} else
		usmcookie->time = 0;
	usmcookie->boots = usm->boots;

	if (agent->v3->level & SNMP_MSGFLAG_AUTH)
		digestlen = usm_digestlen(usm->digest);
	if (agent->v3->level & SNMP_MSGFLAG_PRIV)
	    saltlen = sizeof(usmcookie->salt);

	if ((params = ober_printf_elements(NULL, "{xddxxx}", usm->engineid,
	    usm->engineidlen, usmcookie->boots, usmcookie->time, usm->user,
	    usm->userlen, digest, digestlen, &(usmcookie->salt),
	    saltlen)) == NULL) {
		free(usmcookie);
		return NULL;
	}

	if (ober_scanf_elements(params, "{SSSSe",  &digestelm) == -1) {
		ober_free_element(params);
		free(usmcookie);
		return NULL;
	}

	ober_set_writecallback(digestelm, usm_digest_pos, usmcookie);

	bzero(&ber, sizeof(ber));
	ober_set_application(&ber, smi_application);
	if (ober_write_elements(&ber, params) != -1)
	    berlen = ber_copy_writebuf(&ber, (void **)&secparams);

	*len = berlen;
	ober_free_element(params);
	ober_free(&ber);
	return secparams;
}

static struct ber_element *
usm_encpdu(struct snmp_agent *agent, struct ber_element *pdu, void *cookie)
{
	struct usm_sec *usm = agent->v3->sec->data;
	struct usm_cookie *usmcookie = cookie;
	struct ber ber;
	struct ber_element *retpdu;
	char *serialpdu, *encpdu;
	ssize_t pdulen;
	size_t encpdulen;

	bzero(&ber, sizeof(ber));
	ober_set_application(&ber, smi_application);
	pdulen = ober_write_elements(&ber, pdu);
	if (pdulen == -1)
		return NULL;

	ober_get_writebuf(&ber, (void **)&serialpdu);

	encpdu = usm_crypt(usm->cipher, 1, usm->privkey, usmcookie, serialpdu,
	    pdulen, &encpdulen);
	ober_free(&ber);
	if (encpdu == NULL)
		return NULL;

	retpdu = ober_add_nstring(NULL, encpdu, encpdulen);
	free(encpdu);
	return retpdu;
}

static char *
usm_crypt(const EVP_CIPHER *cipher, int do_enc, char *key,
    struct usm_cookie *cookie, char *serialpdu, size_t pdulen, size_t *outlen)
{
	EVP_CIPHER_CTX *ctx;
	size_t i;
	char iv[EVP_MAX_IV_LENGTH];
	char *salt = (char *)&(cookie->salt);
	char *outtext;
	int len, len2, bs;
	uint32_t ivv;

	switch (EVP_CIPHER_type(cipher)) {
	case NID_des_cbc:
		/* RFC3414, chap 8.1.1.1. */
		for (i = 0; i < 8; i++)
			iv[i] = salt[i] ^ key[USM_SALTOFFSET + i];
		break;
	case NID_aes_128_cfb128:
		/* RFC3826, chap 3.1.2.1. */
		ivv = htobe32(cookie->boots);
		memcpy(iv, &ivv, sizeof(ivv));
		ivv = htobe32(cookie->time);
		memcpy(iv + sizeof(ivv), &ivv, sizeof(ivv));
		memcpy(iv + 2 * sizeof(ivv), &(cookie->salt),
		    sizeof(cookie->salt));
		break;
	default:
		return NULL;
	}

	if ((ctx = EVP_CIPHER_CTX_new()) == NULL)
		return NULL;

	if (!EVP_CipherInit(ctx, cipher, key, iv, do_enc)) {
		EVP_CIPHER_CTX_free(ctx);
		return NULL;
	}

	EVP_CIPHER_CTX_set_padding(ctx, do_enc);

	bs = EVP_CIPHER_block_size(cipher);
	/* Maximum output size */
	*outlen = pdulen + (bs - (pdulen % bs));

	if ((outtext = malloc(*outlen)) == NULL) {
		EVP_CIPHER_CTX_free(ctx);
		return NULL;
	}

	if (EVP_CipherUpdate(ctx, outtext, &len, serialpdu, pdulen) &&
	    EVP_CipherFinal_ex(ctx, outtext + len, &len2))
		*outlen = len + len2;
	else {
		free(outtext);
		outtext = NULL;
	}

	EVP_CIPHER_CTX_free(ctx);

	return outtext;
}

static int
usm_finalparams(struct snmp_agent *agent, char *buf, size_t buflen,
    size_t secparamsoffset, void *cookie)
{
	struct usm_sec *usm = agent->v3->sec->data;
	struct usm_cookie *usmcookie = cookie;
	u_char digest[EVP_MAX_MD_SIZE];

	if ((agent->v3->level & SNMP_MSGFLAG_AUTH) == 0)
		return 0;

	if (usm->authlevel != USM_KEY_LOCALIZED)
		return -1;

	if (HMAC(usm->digest, usm->authkey, EVP_MD_size(usm->digest), buf,
	    buflen, digest, NULL) == NULL)
		return -1;

	memcpy(buf + secparamsoffset + usmcookie->digestoffset, digest,
	    usm_digestlen(usm->digest));
	return 0;
}

static int
usm_parseparams(struct snmp_agent *agent, char *packet, size_t packetlen,
    off_t secparamsoffset, char *buf, size_t buflen, uint8_t level,
    void **cookie)
{
	struct usm_sec *usm = agent->v3->sec->data;
	struct ber ber;
	struct ber_element *secparams;
	char *engineid, *user, *digest, *salt;
	size_t engineidlen, userlen, digestlen, saltlen;
	struct timespec now, timediff;
	off_t digestoffset;
	char exp_digest[EVP_MAX_MD_SIZE];
	struct usm_cookie *usmcookie;

	bzero(&ber, sizeof(ber));
	bzero(exp_digest, sizeof(exp_digest));

	ober_set_application(&ber, smi_application);
	ober_set_readbuf(&ber, buf, buflen);
	if ((secparams = ober_read_elements(&ber, NULL)) == NULL)
		return -1;
	ober_free(&ber);

	if ((usmcookie = malloc(sizeof(*usmcookie))) == NULL)
		goto fail;
	*cookie = usmcookie;

	if (ober_scanf_elements(secparams, "{xddxpxx}", &engineid, &engineidlen,
	    &(usmcookie->boots), &(usmcookie->time), &user, &userlen,
	    &digestoffset, &digest, &digestlen, &salt, &saltlen) == -1)
		goto fail;
	if (saltlen != sizeof(usmcookie->salt) && saltlen != 0)
		goto fail;
	memcpy(&(usmcookie->salt), salt, saltlen);

	if (!usm->engineidset) {
		if (usm_setengineid(agent->v3->sec, engineid,
		    engineidlen) == -1)
			goto fail;
	} else {
		if (usm->engineidlen != engineidlen)
			goto fail;
		if (memcmp(usm->engineid, engineid, engineidlen) != 0)
			goto fail;
	}

	if (!usm->bootsset) {
		usm->boots = usmcookie->boots;
		usm->bootsset = 1;
	} else {
		if (usmcookie->boots < usm->boots)
			goto fail;
		if (usmcookie->boots > usm->boots) {
			usm->bootsset = 0;
			usm->timeset = 0;
			usm_doinit(agent);
			goto fail;
		}
	}

	if (!usm->timeset) {
		usm->time = usmcookie->time;
		if (clock_gettime(CLOCK_MONOTONIC, &usm->timecheck) == -1)
			goto fail;
		usm->timeset = 1;
	} else {
		if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
			goto fail;
		timespecsub(&now, &(usm->timecheck), &timediff);
		if (usmcookie->time <
		    usm->time + timediff.tv_sec - USM_MAX_TIMEWINDOW ||
		    usmcookie->time >
		    usm->time + timediff.tv_sec + USM_MAX_TIMEWINDOW) {
			usm->bootsset = 0;
			usm->timeset = 0;
			usm_doinit(agent);
			goto fail;
		}
	}
	/*
	 * Don't assume these are set if both are zero.
	 * Ugly hack for HP Laserjet
	 */
	if (usm->boots == 0 && usm->time == 0) {
		usm->bootsset = 0;
		usm->timeset = 0;
	}

	if (userlen != usm->userlen ||
	    memcmp(user, usm->user, userlen) != 0)
		goto fail;

	if (level & SNMP_MSGFLAG_AUTH) {
		if (digestlen != usm_digestlen(usm->digest))
			goto fail;
	}
	if ((agent->v3->level & SNMP_MSGFLAG_AUTH)) {
		bzero(packet + secparamsoffset + digestoffset, digestlen);
		if (HMAC(usm->digest, usm->authkey, EVP_MD_size(usm->digest), packet,
		    packetlen, exp_digest, NULL) == NULL)
			goto fail;

		if (memcmp(exp_digest, digest, digestlen) != 0)
			goto fail;
	} else
		if (digestlen != 0)
			goto fail;

	ober_free_element(secparams);
	return 0;

fail:
	free(usmcookie);
	ober_free_element(secparams);
	return -1;
}

struct ber_element *
usm_decpdu(struct snmp_agent *agent, char *encpdu, size_t encpdulen, void *cookie)
{
	struct usm_sec *usm = agent->v3->sec->data;
	struct usm_cookie *usmcookie = cookie;
	struct ber ber;
	struct ber_element *scopedpdu;
	char *rawpdu;
	size_t rawpdulen;

	if ((rawpdu = usm_crypt(usm->cipher, 0, usm->privkey, usmcookie,
	    encpdu, encpdulen, &rawpdulen)) == NULL)
		return NULL;

	bzero(&ber, sizeof(ber));
	ober_set_application(&ber, smi_application);
	ober_set_readbuf(&ber, rawpdu, rawpdulen);
	scopedpdu = ober_read_elements(&ber, NULL);
	ober_free(&ber);
	free(rawpdu);

	return scopedpdu;
}

static void
usm_digest_pos(void *data, size_t offset)
{
	struct usm_cookie *usmcookie = data;

	usmcookie->digestoffset = offset;
}

static void
usm_free(void *data)
{
	struct usm_sec *usm = data;

	free(usm->user);
	free(usm->authkey);
	free(usm->privkey);
	free(usm->engineid);
	free(usm);
}

int
usm_setauth(struct snmp_sec *sec, const EVP_MD *digest, const char *key,
    size_t keylen, enum usm_key_level level)
{
	struct usm_sec *usm = sec->data;
	char *lkey;

	/*
	 * We could transform a master key to a local key here if we already
	 * have usm_setengineid called. Sine snmpc.c is the only caller at
	 * the moment there's no need, since it always calls this function
	 * first.
	 */
	if (level == USM_KEY_PASSWORD) {
		if ((usm->authkey = usm_passwd2mkey(digest, key)) == NULL)
			return -1;
		level = USM_KEY_MASTER;
		keylen = EVP_MD_size(digest);
	} else {
		if (keylen != (size_t)EVP_MD_size(digest)) {
			errno = EINVAL;
			return -1;
		}
		if ((lkey = malloc(keylen)) == NULL)
			return -1;
		memcpy(lkey, key, keylen);
		usm->authkey = lkey;
	}
	usm->digest = digest;
	usm->authlevel = level;
	return 0;
}

int
usm_setpriv(struct snmp_sec *sec, const EVP_CIPHER *cipher, const char *key,
    size_t keylen, enum usm_key_level level)
{
	struct usm_sec *usm = sec->data;
	char *lkey;

	if (usm->digest == NULL) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * We could transform a master key to a local key here if we already
	 * have usm_setengineid called. Sine snmpc.c is the only caller at
	 * the moment there's no need, since it always calls us first.
	 */
	if (level == USM_KEY_PASSWORD) {
		if ((usm->privkey = usm_passwd2mkey(usm->digest, key)) == NULL)
			return -1;
		level = USM_KEY_MASTER;
		keylen = EVP_MD_size(usm->digest);
	} else {
		if (keylen != (size_t)EVP_MD_size(usm->digest)) {
			errno = EINVAL;
			return -1;
		}
		if ((lkey = malloc(keylen)) == NULL)
			return -1;
		memcpy(lkey, key, keylen);
		usm->privkey = lkey;
	}
	usm->cipher = cipher;
	usm->privlevel = level;
	return 0;
}

int
usm_setengineid(struct snmp_sec *sec, char *engineid, size_t engineidlen)
{
	struct usm_sec *usm = sec->data;
	char *mkey;

	if (usm->engineid != NULL)
		free(usm->engineid);
	if ((usm->engineid = malloc(engineidlen)) == NULL)
		return -1;
	memcpy(usm->engineid, engineid, engineidlen);
	usm->engineidlen = engineidlen;
	usm->engineidset = 1;

	if (usm->authlevel == USM_KEY_MASTER) {
		mkey = usm->authkey;
		if ((usm->authkey = usm_mkey2lkey(usm, usm->digest,
		    mkey)) == NULL) {
			usm->authkey = mkey;
			return -1;
		}
		free(mkey);
		usm->authlevel = USM_KEY_LOCALIZED;
	}
	if (usm->privlevel == USM_KEY_MASTER) {
		mkey = usm->privkey;
		if ((usm->privkey = usm_mkey2lkey(usm, usm->digest,
		    mkey)) == NULL) {
			usm->privkey = mkey;
			return -1;
		}
		free(mkey);
		usm->privlevel = USM_KEY_LOCALIZED;
	}

	return 0;
}

int
usm_setbootstime(struct snmp_sec *sec, uint32_t boots, uint32_t time)
{
	struct usm_sec *usm = sec->data;

	if (clock_gettime(CLOCK_MONOTONIC, &(usm->timecheck)) == -1)
		return -1;

	usm->boots = boots;
	usm->bootsset = 1;
	usm->time = time;
	usm->timeset = 1;
	return 0;
}

static char *
usm_passwd2mkey(const EVP_MD *md, const char *passwd)
{
	EVP_MD_CTX *ctx;
	int i, count;
	const u_char *pw;
	u_char *c;
	u_char keybuf[EVP_MAX_MD_SIZE];
	unsigned dlen;
	char *key;

	if ((ctx = EVP_MD_CTX_new()) == NULL)
		return NULL;
	if (!EVP_DigestInit_ex(ctx, md, NULL)) {
		EVP_MD_CTX_free(ctx);
		return NULL;
	}

	pw = (const u_char *)passwd;
	for (count = 0; count < 1048576; count += 64) {
		c = keybuf;
		for (i = 0; i < 64; i++) {
			if (*pw == '\0')
				pw = (const u_char *)passwd;
			*c++ = *pw++;
		}
		if (!EVP_DigestUpdate(ctx, keybuf, 64)) {
			EVP_MD_CTX_free(ctx);
			return NULL;
		}
	}
	if (!EVP_DigestFinal_ex(ctx, keybuf, &dlen)) {
		EVP_MD_CTX_free(ctx);
		return NULL;
	}
	EVP_MD_CTX_free(ctx);

	if ((key = malloc(dlen)) == NULL)
		return NULL;
	memcpy(key, keybuf, dlen);
	return key;
}

static char *
usm_mkey2lkey(struct usm_sec *usm, const EVP_MD *md, const char *mkey)
{
	EVP_MD_CTX *ctx;
	u_char buf[EVP_MAX_MD_SIZE];
	u_char *lkey;
	unsigned lklen;

	if ((ctx = EVP_MD_CTX_new()) == NULL)
		return NULL;

	if (!EVP_DigestInit_ex(ctx, md, NULL) ||
	    !EVP_DigestUpdate(ctx, mkey, EVP_MD_size(md)) ||
	    !EVP_DigestUpdate(ctx, usm->engineid, usm->engineidlen) ||
	    !EVP_DigestUpdate(ctx, mkey, EVP_MD_size(md)) ||
	    !EVP_DigestFinal_ex(ctx, buf, &lklen)) {
		EVP_MD_CTX_free(ctx);
		return NULL;
	}

	EVP_MD_CTX_free(ctx);

	if ((lkey = malloc(lklen)) == NULL)
		return NULL;
	memcpy(lkey, buf, lklen);
	return lkey;
}

static size_t
usm_digestlen(const EVP_MD *md)
{
	switch (EVP_MD_type(md)) {
	case NID_md5:
	case NID_sha1:
		return 12;
	case NID_sha224:
		return 16;
	case NID_sha256:
		return 24;
	case NID_sha384:
		return 32;
	case NID_sha512:
		return 48;
	default:
		return 0;
	}
}
