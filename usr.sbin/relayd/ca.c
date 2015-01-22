/*	$OpenBSD: ca.c,v 1.12 2015/01/22 17:42:09 reyk Exp $	*/

/*
 * Copyright (c) 2014 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/queue.h>
#include <sys/uio.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <imsg.h>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>

#include "relayd.h"

void	 ca_init(struct privsep *, struct privsep_proc *p, void *);
void	 ca_launch(void);

int	 ca_dispatch_parent(int, struct privsep_proc *, struct imsg *);
int	 ca_dispatch_relay(int, struct privsep_proc *, struct imsg *);

int	 rsae_pub_enc(int, const u_char *, u_char *, RSA *, int);
int	 rsae_pub_dec(int,const u_char *, u_char *, RSA *, int);
int	 rsae_priv_enc(int, const u_char *, u_char *, RSA *, int);
int	 rsae_priv_dec(int, const u_char *, u_char *, RSA *, int);
int	 rsae_mod_exp(BIGNUM *, const BIGNUM *, RSA *, BN_CTX *);
int	 rsae_bn_mod_exp(BIGNUM *, const BIGNUM *, const BIGNUM *,
	    const BIGNUM *, BN_CTX *, BN_MONT_CTX *);
int	 rsae_init(RSA *);
int	 rsae_finish(RSA *);
int	 rsae_sign(int, const u_char *, u_int, u_char *, u_int *,
	    const RSA *);
int	 rsae_verify(int dtype, const u_char *m, u_int, const u_char *,
	    u_int, const RSA *);
int	 rsae_keygen(RSA *, int, BIGNUM *, BN_GENCB *);

static struct relayd *env = NULL;
extern int		 proc_id;

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	ca_dispatch_parent },
	{ "relay",	PROC_RELAY,	ca_dispatch_relay },
};

pid_t
ca(struct privsep *ps, struct privsep_proc *p)
{
	env = ps->ps_env;

	return (proc_run(ps, p, procs, nitems(procs), ca_init, NULL));
}

void
ca_init(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	if (config_init(ps->ps_env) == -1)
		fatal("failed to initialize configuration");

	proc_id = p->p_instance;
	env->sc_id = getpid() & 0xffff;
}

void
ca_launch(void)
{
	BIO		*in = NULL;
	EVP_PKEY	*pkey = NULL;
	struct relay	*rlay;

	TAILQ_FOREACH(rlay, env->sc_relays, rl_entry) {
		if ((rlay->rl_conf.flags & (F_TLS|F_TLSCLIENT)) == 0)
			continue;

		if (rlay->rl_conf.tls_key_len) {
			if ((in = BIO_new_mem_buf(rlay->rl_tls_key,
			    rlay->rl_conf.tls_key_len)) == NULL)
				fatalx("ca_launch: key");

			if ((pkey = PEM_read_bio_PrivateKey(in,
			    NULL, NULL, NULL)) == NULL)
				fatalx("ca_launch: PEM");
			BIO_free(in);

			rlay->rl_tls_pkey = pkey;

			if (pkey_add(env, pkey,
			    rlay->rl_conf.tls_keyid) == NULL)
				fatalx("tls pkey");

			purge_key(&rlay->rl_tls_key,
			    rlay->rl_conf.tls_key_len);
		}
		if (rlay->rl_conf.tls_cert_len) {
			purge_key(&rlay->rl_tls_cert,
			    rlay->rl_conf.tls_cert_len);
		}
		if (rlay->rl_conf.tls_cakey_len) {
			if ((in = BIO_new_mem_buf(rlay->rl_tls_cakey,
			    rlay->rl_conf.tls_cakey_len)) == NULL)
				fatalx("ca_launch: key");

			if ((pkey = PEM_read_bio_PrivateKey(in,
			    NULL, NULL, NULL)) == NULL)
				fatalx("ca_launch: PEM");
			BIO_free(in);

			rlay->rl_tls_capkey = pkey;

			if (pkey_add(env, pkey,
			    rlay->rl_conf.tls_cakeyid) == NULL)
				fatalx("ca pkey");

			purge_key(&rlay->rl_tls_cakey,
			    rlay->rl_conf.tls_cakey_len);
		}
		if (rlay->rl_conf.tls_cacert_len) {
			purge_key(&rlay->rl_tls_cacert,
			    rlay->rl_conf.tls_cacert_len);
		}
	}
}

int
ca_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	switch (imsg->hdr.type) {
	case IMSG_CFG_RELAY:
		config_getrelay(env, imsg);
		break;
	case IMSG_CFG_DONE:
		config_getcfg(env, imsg);
		break;
	case IMSG_CTL_START:
		ca_launch();
		break;
	case IMSG_CTL_RESET:
		config_getreset(env, imsg);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
ca_dispatch_relay(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct ctl_keyop	 cko;
	EVP_PKEY		*pkey;
	RSA			*rsa;
	u_char			*from = NULL, *to = NULL;
	struct iovec		 iov[2];
	int			 c = 0;

	switch (imsg->hdr.type) {
	case IMSG_CA_PRIVENC:
	case IMSG_CA_PRIVDEC:
		IMSG_SIZE_CHECK(imsg, (&cko));
		bcopy(imsg->data, &cko, sizeof(cko));
		if (cko.cko_proc > env->sc_prefork_relay)
			fatalx("ca_dispatch_relay: "
			    "invalid relay proc");
		if (IMSG_DATA_SIZE(imsg) != (sizeof(cko) + cko.cko_flen))
			fatalx("ca_dispatch_relay: "
			    "invalid key operation");
		if ((pkey = pkey_find(env, cko.cko_id)) == NULL ||
		    (rsa = EVP_PKEY_get1_RSA(pkey)) == NULL)
			fatalx("ca_dispatch_relay: "
			    "invalid relay key or id");

		DPRINTF("%s:%d: key id %d", __func__, __LINE__, cko.cko_id);

		from = (u_char *)imsg->data + sizeof(cko);
		if ((to = calloc(1, cko.cko_tlen)) == NULL)
			fatalx("ca_dispatch_relay: calloc");

		switch (imsg->hdr.type) {
		case IMSG_CA_PRIVENC:
			cko.cko_tlen = RSA_private_encrypt(cko.cko_flen,
			    from, to, rsa, cko.cko_padding);
			break;
		case IMSG_CA_PRIVDEC:
			cko.cko_tlen = RSA_private_decrypt(cko.cko_flen,
			    from, to, rsa, cko.cko_padding);
			break;
		}

		iov[c].iov_base = &cko;
		iov[c++].iov_len = sizeof(cko);
		if (cko.cko_tlen) {
			iov[c].iov_base = to;
			iov[c++].iov_len = cko.cko_tlen;
		}

		proc_composev_imsg(env->sc_ps, PROC_RELAY, cko.cko_proc,
		    imsg->hdr.type, -1, iov, c);

		free(to);
		RSA_free(rsa);
		break;
	default:
		return (-1);
	}

	return (0);
}

/*
 * RSA privsep engine (called from unprivileged processes)
 */

const RSA_METHOD *rsa_default = NULL;

static RSA_METHOD rsae_method = {
	"RSA privsep engine",
	rsae_pub_enc,
	rsae_pub_dec,
	rsae_priv_enc,
	rsae_priv_dec,
	rsae_mod_exp,
	rsae_bn_mod_exp,
	rsae_init,
	rsae_finish,
	0,
	NULL,
	rsae_sign,
	rsae_verify,
	rsae_keygen
};

static int
rsae_send_imsg(int flen, const u_char *from, u_char *to, RSA *rsa,
    int padding, u_int cmd)
{
	struct ctl_keyop cko;
	int		 ret = 0;
	objid_t		*id;
	struct iovec	 iov[2];
	struct imsgbuf	*ibuf;
	struct imsgev	*iev;
	struct imsg	 imsg;
	int		 n, done = 0, cnt = 0;
	u_char		*toptr;

	if ((id = RSA_get_ex_data(rsa, 0)) == NULL)
		return (0);

	iev = proc_iev(env->sc_ps, PROC_CA, proc_id);
	ibuf = &iev->ibuf;

	/*
	 * XXX this could be nicer...
	 */

	cko.cko_id = *id;
	cko.cko_proc = proc_id;
	cko.cko_flen = flen;
	cko.cko_tlen = RSA_size(rsa);
	cko.cko_padding = padding;

	iov[cnt].iov_base = &cko;
	iov[cnt++].iov_len = sizeof(cko);
	iov[cnt].iov_base = (void *)from;
	iov[cnt++].iov_len = flen;

	/*
	 * Send a synchronous imsg because we cannot defer the RSA
	 * operation in OpenSSL's engine layer.
	 */
	imsg_composev(ibuf, cmd, 0, 0, -1, iov, cnt);
	imsg_flush(ibuf);

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1)
			fatalx("imsg_read");
		if (n == 0)
			fatalx("pipe closed");

		while (!done) {
			if ((n = imsg_get(ibuf, &imsg)) == -1)
				fatalx("imsg_get error");
			if (n == 0)
				break;
			if (imsg.hdr.type != cmd)
				fatalx("invalid response");

			IMSG_SIZE_CHECK(&imsg, (&cko));
			memcpy(&cko, imsg.data, sizeof(cko));
			if (IMSG_DATA_SIZE(&imsg) !=
			    (sizeof(cko) + cko.cko_tlen))
				fatalx("data size");

			ret = cko.cko_tlen;
			if (ret) {
				toptr = (u_char *)imsg.data + sizeof(cko);
				memcpy(to, toptr, ret);
			}
			done = 1;

			imsg_free(&imsg);
		}
	}
	imsg_event_add(iev);

	return (ret);
}

int
rsae_pub_enc(int flen,const u_char *from, u_char *to, RSA *rsa,int padding)
{
	DPRINTF("%s:%d", __func__, __LINE__);
	return (rsa_default->rsa_pub_enc(flen, from, to, rsa, padding));
}

int
rsae_pub_dec(int flen,const u_char *from, u_char *to, RSA *rsa,int padding)
{
	DPRINTF("%s:%d", __func__, __LINE__);
	return (rsa_default->rsa_pub_dec(flen, from, to, rsa, padding));
}

int
rsae_priv_enc(int flen, const u_char *from, u_char *to, RSA *rsa, int padding)
{
	DPRINTF("%s:%d", __func__, __LINE__);
	return (rsae_send_imsg(flen, from, to, rsa, padding,
	    IMSG_CA_PRIVENC));
}

int
rsae_priv_dec(int flen, const u_char *from, u_char *to, RSA *rsa, int padding)
{
	DPRINTF("%s:%d", __func__, __LINE__);
	return (rsae_send_imsg(flen, from, to, rsa, padding,
	    IMSG_CA_PRIVDEC));
}

int
rsae_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx)
{
	DPRINTF("%s:%d", __func__, __LINE__);
	return (rsa_default->rsa_mod_exp(r0, I, rsa, ctx));
}

int
rsae_bn_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx)
{
	DPRINTF("%s:%d", __func__, __LINE__);
	return (rsa_default->bn_mod_exp(r, a, p, m, ctx, m_ctx));
}

int
rsae_init(RSA *rsa)
{
	DPRINTF("%s:%d", __func__, __LINE__);
	if (rsa_default->init == NULL)
		return (1);
	return (rsa_default->init(rsa));
}

int
rsae_finish(RSA *rsa)
{
	DPRINTF("%s:%d", __func__, __LINE__);
	if (rsa_default->finish == NULL)
		return (1);
	return (rsa_default->finish(rsa));
}

int
rsae_sign(int type, const u_char *m, u_int m_length, u_char *sigret,
    u_int *siglen, const RSA *rsa)
{
	DPRINTF("%s:%d", __func__, __LINE__);
	return (rsa_default->rsa_sign(type, m, m_length,
	    sigret, siglen, rsa));
}

int
rsae_verify(int dtype, const u_char *m, u_int m_length, const u_char *sigbuf,
    u_int siglen, const RSA *rsa)
{
	DPRINTF("%s:%d", __func__, __LINE__);
	return (rsa_default->rsa_verify(dtype, m, m_length,
	    sigbuf, siglen, rsa));
}

int
rsae_keygen(RSA *rsa, int bits, BIGNUM *e, BN_GENCB *cb)
{
	DPRINTF("%s:%d", __func__, __LINE__);
	return (rsa_default->rsa_keygen(rsa, bits, e, cb));
}

void
ca_engine_init(struct relayd *x_env)
{
	ENGINE		*e;
	const char	*errstr, *name;

	if (env == NULL)
		env = x_env;

	if ((e = ENGINE_get_default_RSA()) == NULL) {
		if ((e = ENGINE_new()) == NULL) {
			errstr = "ENGINE_new";
			goto fail;
		}
		if (!ENGINE_set_name(e, rsae_method.name)) {
			errstr = "ENGINE_set_name";
			goto fail;
		}
		if ((rsa_default = RSA_get_default_method()) == NULL) {
			errstr = "RSA_get_default_method";
			goto fail;
		}
	} else if ((rsa_default = ENGINE_get_RSA(e)) == NULL) {
		errstr = "ENGINE_get_RSA";
		goto fail;
	}

	if ((name = ENGINE_get_name(e)) == NULL)
		name = "unknown RSA engine";

	log_debug("%s: using %s", __func__, name);

	if (rsa_default->flags & RSA_FLAG_SIGN_VER)
		fatalx("unsupported RSA engine");

	if (rsa_default->rsa_mod_exp == NULL)
		rsae_method.rsa_mod_exp = NULL;
	if (rsa_default->bn_mod_exp == NULL)
		rsae_method.bn_mod_exp = NULL;
	if (rsa_default->rsa_keygen == NULL)
		rsae_method.rsa_keygen = NULL;
	rsae_method.flags = rsa_default->flags |
	    RSA_METHOD_FLAG_NO_CHECK;
	rsae_method.app_data = rsa_default->app_data;

	if (!ENGINE_set_RSA(e, &rsae_method)) {
		errstr = "ENGINE_set_RSA";
		goto fail;
	}
	if (!ENGINE_set_default_RSA(e)) {
		errstr = "ENGINE_set_default_RSA";
		goto fail;
	}

	return;

 fail:
	ssl_error(__func__, errstr);
	fatalx(errstr);
}
