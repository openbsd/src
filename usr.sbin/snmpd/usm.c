/*	$OpenBSD: usm.c,v 1.8 2014/11/19 11:14:06 blambert Exp $	*/

/*
 * Copyright (c) 2012 GeNUA mbH
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

#include <sys/queue.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/tree.h>

#include <net/if.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#ifdef DEBUG
#include <assert.h>
#endif

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "snmpd.h"
#include "mib.h"

extern struct snmpd	*env;

SLIST_HEAD(, usmuser)	usmuserlist;

const EVP_MD		*usm_get_md(enum usmauth);
const EVP_CIPHER	*usm_get_cipher(enum usmpriv);
void			 usm_cb_digest(void *, size_t);
int			 usm_valid_digest(struct snmp_message *, off_t, char *,
			    size_t);
struct ber_element	*usm_decrypt(struct snmp_message *,
			    struct ber_element *);
ssize_t			 usm_crypt(struct snmp_message *, u_char *, int,
			    u_char *, int);
char			*usm_passwd2key(const EVP_MD *, char *, int *);

void
usm_generate_keys(void)
{
	struct usmuser	*up;
	const EVP_MD	*md;
	char		*key;
	int		 len;

	SLIST_FOREACH(up, &usmuserlist, uu_next) {
		if ((md = usm_get_md(up->uu_auth)) == NULL)
			continue;

		/* convert auth password to key */
		len = 0;
		key = usm_passwd2key(md, up->uu_authkey, &len);
		free(up->uu_authkey);
		up->uu_authkey = key;
		up->uu_authkeylen = len;

		/* optionally convert privacy password to key */
		if (up->uu_priv != PRIV_NONE) {
			arc4random_buf(&up->uu_salt, sizeof(up->uu_salt));

			len = SNMP_CIPHER_KEYLEN;
			key = usm_passwd2key(md, up->uu_privkey, &len);
			free(up->uu_privkey);
			up->uu_privkey = key;
		}
	}
	return;
}

const EVP_MD *
usm_get_md(enum usmauth ua)
{
	switch (ua) {
	case AUTH_MD5:
		return EVP_md5();
	case AUTH_SHA1:
		return EVP_sha1();
	case AUTH_NONE:
	default:
		return NULL;
	}
}

const EVP_CIPHER *
usm_get_cipher(enum usmpriv up)
{
	switch (up) {
	case PRIV_DES:
		return EVP_des_cbc();
	case PRIV_AES:
		return EVP_aes_128_cfb128();
	case PRIV_NONE:
	default:
		return NULL;
	}
}

struct usmuser *
usm_newuser(char *name, const char **errp)
{
	struct usmuser *up = usm_finduser(name);
	if (up != NULL) {
		*errp = "user redefined";
		return NULL;
	}
	if ((up = calloc(1, sizeof(*up))) == NULL)
		fatal("usm");
	up->uu_name = name;
	SLIST_INSERT_HEAD(&usmuserlist, up, uu_next);
	return up;
}

struct usmuser *
usm_finduser(char *name)
{
	struct usmuser *up;

	SLIST_FOREACH(up, &usmuserlist, uu_next) {
		if (!strcmp(up->uu_name, name))
			return up;
	}
	return NULL;
}

int
usm_checkuser(struct usmuser *up, const char **errp)
{
	char	*auth = NULL, *priv = NULL;

	if (up->uu_auth != AUTH_NONE && up->uu_authkey == NULL) {
		*errp = "missing auth passphrase";
		goto fail;
	} else if (up->uu_auth == AUTH_NONE && up->uu_authkey != NULL)
		up->uu_auth = AUTH_DEFAULT;

	if (up->uu_priv != PRIV_NONE && up->uu_privkey == NULL) {
		*errp = "missing priv passphrase";
		goto fail;
	} else if (up->uu_priv == PRIV_NONE && up->uu_privkey != NULL)
		up->uu_priv = PRIV_DEFAULT;

	if (up->uu_auth == AUTH_NONE && up->uu_priv != PRIV_NONE) {
		/* Standard prohibits noAuthPriv */
		*errp = "auth is mandatory with enc";
		goto fail;
	}

	switch (up->uu_auth) {
	case AUTH_NONE:
		auth = "none";
		break;
	case AUTH_MD5:
		up->uu_seclevel |= SNMP_MSGFLAG_AUTH;
		auth = "HMAC-MD5-96";
		break;
	case AUTH_SHA1:
		up->uu_seclevel |= SNMP_MSGFLAG_AUTH;
		auth = "HMAC-SHA1-96";
		break;
	}

	switch (up->uu_priv) {
	case PRIV_NONE:
		priv = "none";
		break;
	case PRIV_DES:
		up->uu_seclevel |= SNMP_MSGFLAG_PRIV;
		priv = "CBC-DES";
		break;
	case PRIV_AES:
		up->uu_seclevel |= SNMP_MSGFLAG_PRIV;
		priv = "CFB128-AES-128";
		break;
	}

	log_debug("user \"%s\" auth %s enc %s", up->uu_name, auth, priv);
	return 0;

fail:
	free(up->uu_name);
	free(up->uu_authkey);
	free(up->uu_privkey);
	SLIST_REMOVE(&usmuserlist, up, usmuser, uu_next);
	free(up);
	return -1;
}

struct ber_element *
usm_decode(struct snmp_message *msg, struct ber_element *elm, const char **errp)
{
	struct snmp_stats	*stats = &env->sc_stats;
	off_t			 offs, offs2;
	char			*usmparams;
	size_t			 len;
	size_t			 enginelen, userlen, digestlen, saltlen;
	struct ber		 ber;
	struct ber_element	*usm = NULL, *next = NULL, *decr;
	char			*engineid;
	char			*user;
	char			*digest, *salt;
	u_long			 now;
	long long		 engine_boots, engine_time;

	bzero(&ber, sizeof(ber));
	offs = ber_getpos(elm);

	if (ber_get_nstring(elm, (void *)&usmparams, &len) < 0) {
		*errp = "cannot decode security params";
		goto done;
	}

	ber.fd = -1;
	ber_set_readbuf(&ber, usmparams, len);
	usm = ber_read_elements(&ber, NULL);
	if (usm == NULL) {
		*errp = "cannot decode security params";
		goto done;
	}

#ifdef DEBUG
	fprintf(stderr, "decode USM parameters:\n");
	smi_debug_elements(usm);
#endif

	if (ber_scanf_elements(usm, "{xiixpxx", &engineid, &enginelen,
	    &engine_boots, &engine_time, &user, &userlen, &offs2,
	    &digest, &digestlen, &salt, &saltlen) != 0) {
		*errp = "cannot decode USM params";
		goto done;
	}

	log_debug("USM: engineid '%s', engine boots %lld, engine time %lld, "
	    "user '%s'", tohexstr(engineid, enginelen), engine_boots,
	    engine_time, user);

	if (enginelen > SNMPD_MAXENGINEIDLEN ||
	    userlen > SNMPD_MAXUSERNAMELEN ||
	    (digestlen != (MSG_HAS_AUTH(msg) ? SNMP_USM_DIGESTLEN : 0)) ||
	    (saltlen != (MSG_HAS_PRIV(msg) ? SNMP_USM_SALTLEN : 0))) {
		*errp = "bad field length";
		goto done;
	}

	if (enginelen != env->sc_engineid_len ||
	    memcmp(engineid, env->sc_engineid, enginelen) != 0) {
		*errp = "unknown engine id";
		msg->sm_usmerr = OIDVAL_usmErrEngineId;
		stats->snmp_usmnosuchengine++;
		goto done;
	}

	if (engine_boots != 0LL && engine_time != 0LL) {
		now = snmpd_engine_time();
		if (engine_boots != env->sc_engine_boots ||
		    engine_time < (long long)(now - SNMP_MAX_TIMEWINDOW) ||
		    engine_time > (long long)(now + SNMP_MAX_TIMEWINDOW)) {
			*errp = "out of time window";
			msg->sm_usmerr = OIDVAL_usmErrTimeWindow;
			stats->snmp_usmtimewindow++;
			goto done;
		}
	}

	msg->sm_engine_boots = (u_int32_t)engine_boots;
	msg->sm_engine_time = (u_int32_t)engine_time;

	memcpy(msg->sm_username, user, userlen);
	msg->sm_username[userlen] = '\0';
	msg->sm_user = usm_finduser(msg->sm_username);
	if (msg->sm_user == NULL) {
		*errp = "no such user";
		msg->sm_usmerr = OIDVAL_usmErrUserName;
		stats->snmp_usmnosuchuser++;
		goto done;
	}
	if (MSG_SECLEVEL(msg) > msg->sm_user->uu_seclevel) {
		*errp = "unsupported security model";
		msg->sm_usmerr = OIDVAL_usmErrSecLevel;
		stats->snmp_usmbadseclevel++;
		goto done;
	}

	/*
	 * offs is the offset of the USM string within the serialized msg
	 * and offs2 the offset of the digest within the USM string.
	 */
	if (!usm_valid_digest(msg, offs + offs2, digest, digestlen)) {
		*errp = "bad msg digest";
		msg->sm_usmerr = OIDVAL_usmErrDigest;
		stats->snmp_usmwrongdigest++;
		goto done;
	}

	if (MSG_HAS_PRIV(msg)) {
		memcpy(msg->sm_salt, salt, saltlen);
		if ((decr = usm_decrypt(msg, elm->be_next)) == NULL) {
			*errp = "cannot decrypt msg";
			msg->sm_usmerr = OIDVAL_usmErrDecrypt;
			stats->snmp_usmdecrypterr++;
			goto done;
		}
		ber_replace_elements(elm, decr);
	}
	next = elm->be_next;

done:
	ber_free(&ber);
	if (usm != NULL)
		ber_free_elements(usm);
	return next;
}

struct ber_element *
usm_encode(struct snmp_message *msg, struct ber_element *e)
{
	struct ber		 ber;
	struct ber_element	*usm, *a, *res = NULL;
	void			*ptr;
	char			 digest[SNMP_USM_DIGESTLEN];
	size_t			 digestlen, saltlen, len;

	msg->sm_digest_offs = 0;
	bzero(&ber, sizeof(ber));
	ber.fd = -1;

	usm = ber_add_sequence(NULL);

	if (MSG_HAS_AUTH(msg)) {
		/*
		 * Fill in enough zeroes and remember the position within the
		 * messages. The digest will be calculated once the message
		 * is complete.
		 */
#ifdef DEBUG
		assert(msg->sm_user != NULL);
#endif
		bzero(digest, sizeof(digest));
		digestlen = sizeof(digest);
	} else
		digestlen = 0;

	if (MSG_HAS_PRIV(msg)) {
#ifdef DEBUG
		assert(msg->sm_user != NULL);
#endif
		++(msg->sm_user->uu_salt);
		memcpy(msg->sm_salt, &msg->sm_user->uu_salt,
		    sizeof(msg->sm_salt));
		saltlen = sizeof(msg->sm_salt);
	} else
		saltlen = 0;

	msg->sm_engine_boots = (u_int32_t)env->sc_engine_boots;
	msg->sm_engine_time = (u_int32_t)snmpd_engine_time();
	if ((a = ber_printf_elements(usm, "xdds",
	    env->sc_engineid, env->sc_engineid_len, msg->sm_engine_boots,
	    msg->sm_engine_time, msg->sm_username)) == NULL)
		goto done;

	if ((a = ber_add_nstring(a, digest, digestlen)) == NULL)
		goto done;
	if (digestlen > 0)
		ber_set_writecallback(a, usm_cb_digest, msg);

	if ((a = ber_add_nstring(a, msg->sm_salt, saltlen)) == NULL)
		goto done;

#ifdef DEBUG
	fprintf(stderr, "encode USM parameters:\n");
	smi_debug_elements(usm);
#endif
	len = ber_write_elements(&ber, usm);
	if (ber_get_writebuf(&ber, &ptr) > 0) {
		res = ber_add_nstring(e, (char *)ptr, len);
		if (digestlen > 0)
			ber_set_writecallback(res, usm_cb_digest, msg);
	}

done:
	ber_free(&ber);
	ber_free_elements(usm);
	return res;
}

void
usm_cb_digest(void *arg, size_t offs)
{
	struct snmp_message *msg = arg;
	msg->sm_digest_offs += offs;
}

struct ber_element *
usm_encrypt(struct snmp_message *msg, struct ber_element *pdu)
{
	struct ber		 ber;
	struct ber_element	*encrpdu = NULL;
	void			*ptr;
	int			 len;
	ssize_t			 elen;
	u_char			 encbuf[READ_BUF_SIZE];

	if (!MSG_HAS_PRIV(msg))
		return pdu;

	bzero(&ber, sizeof(ber));
	ber.fd = -1;

#ifdef DEBUG
	fprintf(stderr, "encrypted PDU:\n");
	smi_debug_elements(pdu);
#endif

	len = ber_write_elements(&ber, pdu);
	if (ber_get_writebuf(&ber, &ptr) > 0) {
		elen = usm_crypt(msg, ptr, len, encbuf, 1);
		if (elen > 0)
			encrpdu = ber_add_nstring(NULL, (char *)encbuf, elen);
	}

	ber_free(&ber);
	ber_free_elements(pdu);
	return encrpdu;
}

/*
 * Calculate message digest and replace within message
 */
void
usm_finalize_digest(struct snmp_message *msg, char *buf, ssize_t len)
{
	const EVP_MD	*md;
	u_char		 digest[EVP_MAX_MD_SIZE];
	unsigned	 hlen;

	if (msg->sm_resp == NULL ||
	    !MSG_HAS_AUTH(msg) ||
	    msg->sm_user == NULL ||
	    msg->sm_digest_offs == 0 ||
	    len <= 0)
		return;
	bzero(digest, SNMP_USM_DIGESTLEN);
#ifdef DEBUG
	assert(msg->sm_digest_offs + SNMP_USM_DIGESTLEN <= (size_t)len);
	assert(!memcmp(buf + msg->sm_digest_offs, digest, SNMP_USM_DIGESTLEN));
#endif

	if ((md = usm_get_md(msg->sm_user->uu_auth)) == NULL)
		return;

	HMAC(md, msg->sm_user->uu_authkey, (int)msg->sm_user->uu_authkeylen,
	    (u_char*)buf, (size_t)len, digest, &hlen);

	memcpy(buf + msg->sm_digest_offs, digest, SNMP_USM_DIGESTLEN);
	return;
}

void
usm_make_report(struct snmp_message *msg)
{
	struct ber_oid		 usmstat = OID(MIB_usmStats, 0, 0);

	/* Always send report in clear-text */
	msg->sm_flags = 0;
	msg->sm_context = SNMP_C_REPORT;
	msg->sm_username[0] = '\0';
	usmstat.bo_id[OIDIDX_usmStats] = msg->sm_usmerr;
	usmstat.bo_n = OIDIDX_usmStats + 2;
	if (msg->sm_varbindresp != NULL)
		ber_free_elements(msg->sm_varbindresp);
	msg->sm_varbindresp = ber_add_sequence(NULL);
	mps_getreq(NULL, msg->sm_varbindresp, &usmstat, msg->sm_version);
	return;
}

int
usm_valid_digest(struct snmp_message *msg, off_t offs,
	char *digest, size_t digestlen)
{
	const EVP_MD	*md;
	u_char		 exp_digest[EVP_MAX_MD_SIZE];
	unsigned	 hlen;

	if (!MSG_HAS_AUTH(msg))
		return 1;

	if (digestlen != SNMP_USM_DIGESTLEN)
		return 0;

#ifdef DEBUG
	assert(offs + digestlen <= msg->sm_datalen);
	assert(bcmp(&msg->sm_data[offs], digest, digestlen) == 0);
#endif

	if ((md = usm_get_md(msg->sm_user->uu_auth)) == NULL)
		return 0;

	memset(&msg->sm_data[offs], 0, digestlen);
	HMAC(md, msg->sm_user->uu_authkey, (int)msg->sm_user->uu_authkeylen,
	    msg->sm_data, msg->sm_datalen, exp_digest, &hlen);
	/* we don't bother to restore the original message */

	if (hlen < digestlen)
		return 0;

	return memcmp(digest, exp_digest, digestlen) == 0;
}

struct ber_element *
usm_decrypt(struct snmp_message *msg, struct ber_element *encr)
{
	u_char			*privstr;
	size_t			 privlen;
	u_char			 buf[READ_BUF_SIZE];
	struct ber		 ber;
	struct ber_element	*scoped_pdu = NULL;
	ssize_t			 scoped_pdu_len;

	if (ber_get_nstring(encr, (void *)&privstr, &privlen) < 0)
		return NULL;

	scoped_pdu_len = usm_crypt(msg, privstr, (int)privlen, buf, 0);
	if (scoped_pdu_len < 0)
		return NULL;

	bzero(&ber, sizeof(ber));
	ber.fd = -1;
	ber_set_readbuf(&ber, buf, scoped_pdu_len);
	scoped_pdu = ber_read_elements(&ber, NULL);

#ifdef DEBUG
	if (scoped_pdu != NULL) {
		fprintf(stderr, "decrypted scoped PDU:\n");
		smi_debug_elements(scoped_pdu);
	}
#endif

	ber_free(&ber);
	return scoped_pdu;
}

ssize_t
usm_crypt(struct snmp_message *msg, u_char *inbuf, int inlen, u_char *outbuf,
	int do_encrypt)
{
	const EVP_CIPHER	*cipher;
	EVP_CIPHER_CTX		 ctx;
	u_char			*privkey;
	int			 i;
	u_char			 iv[EVP_MAX_IV_LENGTH];
	int			 len, len2;
	int			 rv;
	u_int32_t		 ivv;

	if ((cipher = usm_get_cipher(msg->sm_user->uu_priv)) == NULL)
		return -1;

	privkey = (u_char *)msg->sm_user->uu_privkey;
#ifdef DEBUG
	assert(privkey != NULL);
#endif
	switch (msg->sm_user->uu_priv) {
	case PRIV_DES:
		/* RFC3414, chap 8.1.1.1. */
		for (i = 0; i < 8; i++)
			iv[i] = msg->sm_salt[i] ^ privkey[SNMP_USM_SALTLEN + i];
		break;
	case PRIV_AES:
		/* RFC3826, chap 3.1.2.1. */
		ivv = htobe32(msg->sm_engine_boots);
		memcpy(iv, &ivv, sizeof(ivv));
		ivv = htobe32(msg->sm_engine_time);
		memcpy(iv + sizeof(ivv), &ivv, sizeof(ivv));
		memcpy(iv + 2 * sizeof(ivv), msg->sm_salt, SNMP_USM_SALTLEN);
		break;
	default:
		return -1;
	}

	if (!EVP_CipherInit(&ctx, cipher, privkey, iv, do_encrypt))
		return -1;

	if (!do_encrypt)
		EVP_CIPHER_CTX_set_padding(&ctx, 0);

	if (EVP_CipherUpdate(&ctx, outbuf, &len, inbuf, inlen) &&
	    EVP_CipherFinal(&ctx, outbuf + len, &len2))
		rv = len + len2;
	else
		rv = -1;

	EVP_CIPHER_CTX_cleanup(&ctx);
	return rv;
}

/*
 * RFC3414, Password to Key Algorithm
 */
char *
usm_passwd2key(const EVP_MD *md, char *passwd, int *maxlen)
{
	EVP_MD_CTX	 ctx;
	int		 i, count;
	u_char		*pw, *c;
	u_char		 pwbuf[2 * EVP_MAX_MD_SIZE + SNMPD_MAXENGINEIDLEN];
	u_char		 keybuf[EVP_MAX_MD_SIZE];
	unsigned	 dlen;
	char		*key;

	EVP_DigestInit(&ctx, md);
	pw = (u_char *)passwd;
	for (count = 0; count < 1048576; count += 64) {
		c = pwbuf;
		for (i = 0; i < 64; i++) {
			if (*pw == '\0')
				pw = (u_char *)passwd;
			*c++ = *pw++;
		}
		EVP_DigestUpdate(&ctx, pwbuf, 64);
	}
	EVP_DigestFinal(&ctx, keybuf, &dlen);
	EVP_MD_CTX_cleanup(&ctx);

	/* Localize the key */
#ifdef DEBUG
	assert(env->sc_engineid_len <= SNMPD_MAXENGINEIDLEN);
#endif
	memcpy(pwbuf, keybuf, dlen);
	memcpy(pwbuf + dlen, env->sc_engineid, env->sc_engineid_len);
	memcpy(pwbuf + dlen + env->sc_engineid_len, keybuf, dlen);

	EVP_DigestInit(&ctx, md);
	EVP_DigestUpdate(&ctx, pwbuf, 2 * dlen + env->sc_engineid_len);
	EVP_DigestFinal(&ctx, keybuf, &dlen);
	EVP_MD_CTX_cleanup(&ctx);

	if (*maxlen > 0 && dlen > (unsigned)*maxlen)
		dlen = (unsigned)*maxlen;
	if ((key = malloc(dlen)) == NULL)
		fatal("key");
	memcpy(key, keybuf, dlen);
	*maxlen = (int)dlen;
	return key;
}
