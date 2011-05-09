/*	$OpenBSD: ca.c,v 1.16 2011/05/09 11:15:18 reyk Exp $	*/
/*	$vantronix: ca.c,v 1.29 2010/06/02 12:22:58 reyk Exp $	*/

/*
 * Copyright (c) 2010 Reyk Floeter <reyk@vantronix.net>
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <err.h>
#include <pwd.h>
#include <event.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include "iked.h"
#include "ikev2.h"

void	 ca_reset(struct privsep *, void *);
int	 ca_reload(struct iked *);

int	 ca_getreq(struct iked *, struct imsg *);
int	 ca_getcert(struct iked *, struct imsg *);
int	 ca_getauth(struct iked *, struct imsg *);
X509	*ca_by_subjectpubkey(X509_STORE *, u_int8_t *, size_t);
X509	*ca_by_issuer(X509_STORE *, X509_NAME *, struct iked_static_id *);
int	 ca_subjectpubkey_digest(X509 *, u_int8_t *, u_int *);
int	 ca_validate_pubkey(struct iked *, struct iked_static_id *,
	    void *, size_t);
int	 ca_validate_cert(struct iked *, struct iked_static_id *,
	    void *, size_t);
struct ibuf *
	 ca_x509_serialize(X509 *);
int	 ca_x509_subjectaltname_cmp(X509 *, struct iked_static_id *);
int	 ca_x509_subjectaltname(X509 *cert, struct iked_id *);
int	 ca_key_serialize(EVP_PKEY *, struct iked_id *);
int	 ca_dispatch_parent(int, struct privsep_proc *, struct imsg *);
int	 ca_dispatch_ikev1(int, struct privsep_proc *, struct imsg *);
int	 ca_dispatch_ikev2(int, struct privsep_proc *, struct imsg *);

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	ca_dispatch_parent },
	{ "ikev1",	PROC_IKEV1,	ca_dispatch_ikev1 },
	{ "ikev2",	PROC_IKEV2,	ca_dispatch_ikev2 }
};

struct ca_store {
	X509_STORE	*ca_cas;
	X509_LOOKUP	*ca_calookup;

	X509_STORE	*ca_certs;
	X509_LOOKUP	*ca_certlookup;

	struct iked_id	 ca_privkey;
};

pid_t
caproc(struct privsep *ps, struct privsep_proc *p)
{
	struct ca_store	*store;
	FILE		*fp = NULL;
	EVP_PKEY	*key;

	/*
	 * This function runs code before privsep
	 */
	if ((store = calloc(1, sizeof(*store))) == NULL)
		fatal("ca: failed to allocate cert store");

	if ((fp = fopen(IKED_PRIVKEY, "r")) == NULL)
		fatal("ca: failed to open private key");

	if ((key = PEM_read_PrivateKey(fp, NULL, NULL, NULL)) == NULL)
		fatalx("ca: failed to read private key");
	fclose(fp);

	if (ca_key_serialize(key, &store->ca_privkey) != 0)
		fatalx("ca: failed to serialize private key");

	return (proc_run(ps, p, procs, nitems(procs), ca_reset, store));
}

void
ca_reset(struct privsep *ps, void *arg)
{
	struct iked	*env = ps->ps_env;
	struct ca_store	*store = arg;

	if (store->ca_cas != NULL)
		X509_STORE_free(store->ca_cas);
	if (store->ca_certs != NULL)
		X509_STORE_free(store->ca_certs);

	if ((store->ca_cas = X509_STORE_new()) == NULL)
		fatalx("ca_reset: failed to get ca store");
	if ((store->ca_calookup = X509_STORE_add_lookup(store->ca_cas,
	    X509_LOOKUP_file())) == NULL)
		fatalx("ca_reset: failed to add ca lookup");

	if ((store->ca_certs = X509_STORE_new()) == NULL)
		fatalx("ca_reset: failed to get cert store");
	if ((store->ca_certlookup = X509_STORE_add_lookup(store->ca_certs,
	    X509_LOOKUP_file())) == NULL)
		fatalx("ca_reset: failed to add cert lookup");

	env->sc_priv = store;

	if (ca_reload(env) != 0)
		fatal("ca_reset: reload");
}

int
ca_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct iked		*env = p->p_env;
	struct ca_store	*store = env->sc_priv;
	u_int			 mode;

	switch (imsg->hdr.type) {
	case IMSG_CTL_RESET:
		IMSG_SIZE_CHECK(imsg, &mode);
		memcpy(&mode, imsg->data, sizeof(mode));
		if (mode == RESET_ALL || mode == RESET_CA) {
			log_debug("%s: config reload", __func__);
			ca_reset(&env->sc_ps, store);
		}
		break;
	default:
		return (-1);
	}

	return (0);
}

int
ca_dispatch_ikev1(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	return (-1);
}

int
ca_dispatch_ikev2(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct iked	*env = p->p_env;

	switch (imsg->hdr.type) {
	case IMSG_CERTREQ:
		ca_getreq(env, imsg);
		break;
	case IMSG_CERT:
		ca_getcert(env, imsg);
		break;
	case IMSG_AUTH:
		ca_getauth(env, imsg);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
ca_setcert(struct iked *env, struct iked_sahdr *sh, struct iked_id *id,
    u_int8_t type, u_int8_t *data, size_t len, enum privsep_procid procid)
{
	struct iovec		iov[4];
	int			iovcnt = 0;
	struct iked_static_id	idb;

	/* Must send the cert and a valid Id to the ca process */
	if (procid == PROC_CERT) {
		if (id == NULL || id->id_type == IKEV2_ID_NONE ||
		    ibuf_length(id->id_buf) > IKED_ID_SIZE)
			return (-1);
		bzero(&idb, sizeof(idb));

		/* Convert to a static Id */
		idb.id_type = id->id_type;
		idb.id_offset = id->id_offset;
		idb.id_length = ibuf_length(id->id_buf);
		memcpy(&idb.id_data, ibuf_data(id->id_buf),
		    ibuf_length(id->id_buf));

		iov[iovcnt].iov_base = &idb;
		iov[iovcnt].iov_len = sizeof(idb);
		iovcnt++;
	}

	iov[iovcnt].iov_base = sh;
	iov[iovcnt].iov_len = sizeof(*sh);
	iovcnt++;
	iov[iovcnt].iov_base = &type;
	iov[iovcnt].iov_len = sizeof(type);
	iovcnt++;
	iov[iovcnt].iov_base = data;
	iov[iovcnt].iov_len = len;
	iovcnt++;

	if (proc_composev_imsg(env, procid, IMSG_CERT, -1, iov, iovcnt) == -1)
		return (-1);
	return (0);
}

int
ca_setreq(struct iked *env, struct iked_sahdr *sh,
    struct iked_static_id *localid, u_int8_t type, u_int8_t *data,
    size_t len, enum privsep_procid procid)
{
	struct iovec		iov[4];
	int			iovcnt = 0;
	struct iked_static_id	idb;
	struct iked_id		id;
	int			ret = -1;

	/* Convert to a static Id */
	bzero(&id, sizeof(id));
	if (ikev2_policy2id(localid, &id, 0) != 0)
		return (-1);

	bzero(&idb, sizeof(idb));
	idb.id_type = id.id_type;
	idb.id_offset = id.id_offset;
	idb.id_length = ibuf_length(id.id_buf);
	memcpy(&idb.id_data, ibuf_data(id.id_buf),
	    ibuf_length(id.id_buf));
	iov[iovcnt].iov_base = &idb;
	iov[iovcnt].iov_len = sizeof(idb);
	iovcnt++;

	iov[iovcnt].iov_base = sh;
	iov[iovcnt].iov_len = sizeof(*sh);
	iovcnt++;
	iov[iovcnt].iov_base = &type;
	iov[iovcnt].iov_len = sizeof(type);
	iovcnt++;
	iov[iovcnt].iov_base = data;
	iov[iovcnt].iov_len = len;
	iovcnt++;

	if (proc_composev_imsg(env, procid,
	    IMSG_CERTREQ, -1, iov, iovcnt) == -1)
		goto done;

	ret = 0;
 done:
	ibuf_release(id.id_buf);
	return (ret);
}

int
ca_setauth(struct iked *env, struct iked_sa *sa,
    struct ibuf *authmsg, enum privsep_procid id)
{
	struct iovec		 iov[3];
	int			 iovcnt = 3;
	struct iked_policy	*policy = sa->sa_policy;
	u_int8_t		 type = policy->pol_auth.auth_method;

	if (type == IKEV2_AUTH_SHARED_KEY_MIC) {
		sa->sa_stateflags |= IKED_REQ_AUTH;
		return (ikev2_msg_authsign(env, sa,
		    &policy->pol_auth, authmsg));
	}

	iov[0].iov_base = &sa->sa_hdr;
	iov[0].iov_len = sizeof(sa->sa_hdr);
	iov[1].iov_base = &type;
	iov[1].iov_len = sizeof(type);
	if (type == IKEV2_AUTH_NONE)
		iovcnt--;
	else {
		iov[2].iov_base = ibuf_data(authmsg);
		iov[2].iov_len = ibuf_size(authmsg);
		log_debug("%s: auth length %d", __func__, ibuf_size(authmsg));
	}

	if (proc_composev_imsg(env, id, IMSG_AUTH, -1, iov, iovcnt) == -1)
		return (-1);
	return (0);
}

int
ca_getcert(struct iked *env, struct imsg *imsg)
{
	struct iked_sahdr	 sh;
	u_int8_t		 type;
	u_int8_t		*ptr;
	size_t			 len;
	struct iked_static_id	 id;
	u_int			 i;
	struct iovec		 iov[2];
	int			 iovcnt = 2, cmd, ret = 0;

	ptr = (u_int8_t *)imsg->data;
	len = IMSG_DATA_SIZE(imsg);
	i = sizeof(id) + sizeof(sh) + sizeof(type);
	if (len <= i)
		return (-1);

	memcpy(&id, ptr, sizeof(id));
	if (id.id_type == IKEV2_ID_NONE)
		return (-1);
	memcpy(&sh, ptr + sizeof(id), sizeof(sh));
	memcpy(&type, ptr + sizeof(id) + sizeof(sh), sizeof(u_int8_t));

	ptr += i;
	len -= i;

	switch (type) {
	case IKEV2_CERT_X509_CERT:
		ret = ca_validate_cert(env, &id, ptr, len);
		break;
	case IKEV2_CERT_RSA_KEY:
		ret = ca_validate_pubkey(env, &id, ptr, len);
		break;
	default:
		log_debug("%s: unsupported cert type %d", __func__, type);
		ret = -1;
		break;
	}

	if (ret == 0)
		cmd = IMSG_CERTVALID;
	else
		cmd = IMSG_CERTINVALID;

	iov[0].iov_base = &sh;
	iov[0].iov_len = sizeof(sh);
	iov[1].iov_base = &type;
	iov[1].iov_len = sizeof(type);

	if (proc_composev_imsg(env, PROC_IKEV2, cmd, -1, iov, iovcnt) == -1)
		return (-1);
	return (0);
}

int
ca_getreq(struct iked *env, struct imsg *imsg)
{
	struct ca_store	*store = env->sc_priv;
	struct iked_sahdr	 sh;
	u_int8_t		 type;
	u_int8_t		*ptr;
	size_t			 len;
	u_int			 i, n;
	X509			*ca = NULL, *cert = NULL;
	struct ibuf		*buf;
	struct iked_static_id	 id;

	ptr = (u_int8_t *)imsg->data;
	len = IMSG_DATA_SIZE(imsg);
	i = sizeof(id) + sizeof(u_int8_t) + sizeof(sh);
	if (len < i || ((len - i) % SHA_DIGEST_LENGTH) != 0)
		return (-1);

	memcpy(&id, ptr, sizeof(id));
	if (id.id_type == IKEV2_ID_NONE)
		return (-1);
	memcpy(&sh, ptr + sizeof(id), sizeof(sh));
	memcpy(&type, ptr + sizeof(id) + sizeof(sh), sizeof(u_int8_t));
	if (type != IKEV2_CERT_X509_CERT)
		return (-1);

	for (n = 1; i < len; n++, i += SHA_DIGEST_LENGTH) {
		if ((ca = ca_by_subjectpubkey(store->ca_cas,
		    ptr + i, SHA_DIGEST_LENGTH)) == NULL) {
			log_debug("%s: CA %d not found", __func__, n);
			print_hex(ptr, i, SHA_DIGEST_LENGTH);
			continue;
		}

		log_debug("%s: found CA %s", __func__, ca->name);

		if ((cert = ca_by_issuer(store->ca_certs,
		    X509_get_subject_name(ca), &id)) != NULL) {
			/* XXX should we re-validate our own cert here? */
			break;
		}

		log_debug("%s: no valid certificate for this CA", __func__);
	}
	if (ca == NULL || cert == NULL) {
		log_warnx("%s: no valid local certificate found", __func__);
		type = IKEV2_CERT_NONE;
		ca_setcert(env, &sh, NULL, type, NULL, 0, PROC_IKEV2);
		return (0);
	}

	log_debug("%s: found local certificate %s", __func__, cert->name);

	if ((buf = ca_x509_serialize(cert)) == NULL)
		return (-1);

	type = IKEV2_CERT_X509_CERT;
	ca_setcert(env, &sh, NULL, type,
	    ibuf_data(buf), ibuf_size(buf), PROC_IKEV2);

	return (0);
}

int
ca_getauth(struct iked *env, struct imsg *imsg)
{
	struct ca_store	*store = env->sc_priv;
	struct iked_sahdr	 sh;
	u_int8_t		 method;
	u_int8_t		*ptr;
	size_t			 len;
	u_int			 i;
	int			 ret = -1;
	struct iked_sa		 sa;
	struct iked_policy	 policy;
	struct iked_id		*id;
	struct ibuf		*authmsg;

	ptr = (u_int8_t *)imsg->data;
	len = IMSG_DATA_SIZE(imsg);
	i = sizeof(method) + sizeof(sh);
	if (len <= i)
		return (-1);

	memcpy(&sh, ptr, sizeof(sh));
	memcpy(&method, ptr + sizeof(sh), sizeof(u_int8_t));
	if (method == IKEV2_AUTH_SHARED_KEY_MIC)
		return (-1);

	ptr += i;
	len -= i;

	if ((authmsg = ibuf_new(ptr, len)) == NULL)
		return (-1);

	/*
	 * Create fake SA and policy
	 */
	bzero(&sa, sizeof(sa));
	bzero(&policy, sizeof(policy));
	memcpy(&sa.sa_hdr, &sh, sizeof(sh));
	sa.sa_policy = &policy;
	policy.pol_auth.auth_method = method;
	if (sh.sh_initiator)
		id = &sa.sa_icert;
	else
		id = &sa.sa_rcert;
	memcpy(id, &store->ca_privkey, sizeof(*id));

	if (ikev2_msg_authsign(env, &sa, &policy.pol_auth, authmsg) != 0) {
		log_debug("%s: AUTH sign failed", __func__);
		policy.pol_auth.auth_method = IKEV2_AUTH_NONE;
	}

	ret = ca_setauth(env, &sa, sa.sa_localauth.id_buf, PROC_IKEV2);

	ibuf_release(sa.sa_localauth.id_buf);
	ibuf_release(authmsg);

	return (ret);
}

int
ca_reload(struct iked *env)
{
	struct ca_store	*store = env->sc_priv;
	DIR			*dir;
	struct dirent		*entry;
	char			 file[PATH_MAX];
	STACK_OF(X509_OBJECT)	*h;
	X509_OBJECT		*xo;
	X509			*x509;
	int			 i, len;
	u_int8_t		 md[EVP_MAX_MD_SIZE];
	struct iovec		 iov[2];
	int			 iovcnt = 2;

	/*
	 * Load CAs
	 */
	if ((dir = opendir(IKED_CA_DIR)) == NULL)
		return (-1);

	while ((entry = readdir(dir)) != NULL) {
		if ((entry->d_type != DT_REG) &&
		    (entry->d_type != DT_LNK))
			continue;

		if (snprintf(file, sizeof(file), "%s%s",
		    IKED_CA_DIR, entry->d_name) == -1)
			continue;

		if (!X509_load_cert_file(store->ca_calookup, file,
		    X509_FILETYPE_PEM)) {
			log_debug("%s: failed to load ca file %s", __func__,
			    entry->d_name);
			ca_sslerror();
			continue;
		}
		log_debug("%s: loaded ca file %s", __func__, entry->d_name);
	}
	closedir(dir);

	/*
	 * Load CRLs for the CAs
	 */
	if ((dir = opendir(IKED_CRL_DIR)) == NULL)
		return (-1);

	while ((entry = readdir(dir)) != NULL) {
		if ((entry->d_type != DT_REG) &&
		    (entry->d_type != DT_LNK))
			continue;

		if (snprintf(file, sizeof(file), "%s%s",
		    IKED_CRL_DIR, entry->d_name) == -1)
			continue;

		if (!X509_load_crl_file(store->ca_calookup, file,
		    X509_FILETYPE_PEM)) {
			log_debug("%s: failed to load crl file %s", __func__,
			    entry->d_name);
			ca_sslerror();
			continue;
		}

		/* Only enable CRL checks if we actually loaded a CRL */
		X509_STORE_set_flags(store->ca_cas, X509_V_FLAG_CRL_CHECK);

		log_debug("%s: loaded crl file %s", __func__, entry->d_name);
	}
	closedir(dir);

	/*
	 * Save CAs signatures for the IKEv2 CERTREQ
	 */
	ibuf_release(env->sc_certreq);
	if ((env->sc_certreq = ibuf_new(NULL, 0)) == NULL)
		return (-1);

	h = store->ca_cas->objs;
	for (i = 0; i < sk_X509_OBJECT_num(h); i++) {
		xo = sk_X509_OBJECT_value(h, i);
		if (xo->type != X509_LU_X509)
			continue;

		x509 = xo->data.x509;
		len = sizeof(md);
		ca_subjectpubkey_digest(x509, md, &len);
		log_debug("%s: %s", __func__, x509->name);

		if (ibuf_add(env->sc_certreq, md, len) != 0) {
			ibuf_release(env->sc_certreq);
			return (-1);
		}
	}

	if (ibuf_length(env->sc_certreq)) {
		env->sc_certreqtype = IKEV2_CERT_X509_CERT;
		iov[0].iov_base = &env->sc_certreqtype;
		iov[0].iov_len = sizeof(env->sc_certreqtype);
		iov[1].iov_base = ibuf_data(env->sc_certreq);
		iov[1].iov_len = ibuf_length(env->sc_certreq);

		log_debug("%s: loaded %d ca certificate%s", __func__,
		    ibuf_length(env->sc_certreq) / SHA_DIGEST_LENGTH,
		    ibuf_length(env->sc_certreq) == SHA_DIGEST_LENGTH ?
		    "" : "s");

		(void)proc_composev_imsg(env, PROC_IKEV2, IMSG_CERTREQ, -1,
		    iov, iovcnt);
	}

	/*
	 * Load certificates
	 */
	if ((dir = opendir(IKED_CERT_DIR)) == NULL)
		return (-1);

	while ((entry = readdir(dir)) != NULL) {
		if ((entry->d_type != DT_REG) &&
		    (entry->d_type != DT_LNK))
			continue;

		if (snprintf(file, sizeof(file), "%s%s",
		    IKED_CERT_DIR, entry->d_name) == -1)
			continue;

		if (!X509_load_cert_file(store->ca_certlookup, file,
		    X509_FILETYPE_PEM)) {
			log_debug("%s: failed to load cert file %s", __func__,
			    entry->d_name);
			ca_sslerror();
			continue;
		}
		log_debug("%s: loaded cert file %s", __func__, entry->d_name);
	}
	closedir(dir);

	h = store->ca_certs->objs;
	for (i = 0; i < sk_X509_OBJECT_num(h); i++) {
		xo = sk_X509_OBJECT_value(h, i);
		if (xo->type != X509_LU_X509)
			continue;

		x509 = xo->data.x509;

		(void)ca_validate_cert(env, NULL, x509, 0);
	}

	return (0);
}

X509 *
ca_by_subjectpubkey(X509_STORE *ctx, u_int8_t *sig, size_t siglen)
{
	STACK_OF(X509_OBJECT)	*h;
	X509_OBJECT		*xo;
	X509			*ca;
	int			 i;
	u_int			 len;
	u_int8_t		 md[EVP_MAX_MD_SIZE];

	h = ctx->objs;

	for (i = 0; i < sk_X509_OBJECT_num(h); i++) {
		xo = sk_X509_OBJECT_value(h, i);
		if (xo->type != X509_LU_X509)
			continue;

		ca = xo->data.x509;
		len = sizeof(md);
		ca_subjectpubkey_digest(ca, md, &len);

		if (len == siglen && memcmp(md, sig, len) == 0)
			return (ca);
	}

	return (NULL);
}

X509 *
ca_by_issuer(X509_STORE *ctx, X509_NAME *subject, struct iked_static_id *id)
{
	STACK_OF(X509_OBJECT)	*h;
	X509_OBJECT		*xo;
	X509			*cert;
	int			 i;
	X509_NAME		*issuer;

	if (subject == NULL)
		return (NULL);

	h = ctx->objs;
	for (i = 0; i < sk_X509_OBJECT_num(h); i++) {
		xo = sk_X509_OBJECT_value(h, i);
		if (xo->type != X509_LU_X509)
			continue;

		cert = xo->data.x509;
		if ((issuer = X509_get_issuer_name(cert)) == NULL)
			continue;
		else if (X509_NAME_cmp(subject, issuer) == 0) {
			if (ca_x509_subjectaltname_cmp(cert, id) != 0)
				continue;
			return (cert);
		}
	}

	return (NULL);
}

int
ca_subjectpubkey_digest(X509 *x509, u_int8_t *md, u_int *size)
{
	u_int8_t	*buf = NULL;
	int		 buflen;

	if (*size < SHA_DIGEST_LENGTH)
		return (-1);

	/*
	 * Generate a SHA-1 digest of the Subject Public Key Info
	 * element in the X.509 certificate, an ASN.1 sequence
	 * that includes the public key type (eg. RSA) and the
	 * public key value (see 3.7 of RFC4306).
	 */
	buflen = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(x509), &buf);
	if (!buflen)
		return (-1);
	if (!EVP_Digest(buf, buflen, md, size, EVP_sha1(), NULL)) {
		free(buf);
		return (-1);
	}
	free(buf);

	return (0);
}

struct ibuf *
ca_x509_serialize(X509 *x509)
{
	long		 len;
	struct ibuf	*buf;
	u_int8_t	*d = NULL;
	BIO		*out;

	if ((out = BIO_new(BIO_s_mem())) == NULL)
		return (NULL);
	if (!i2d_X509_bio(out, x509)) {
		BIO_free(out);
		return (NULL);
	}

	len = BIO_get_mem_data(out, &d);
	buf = ibuf_new(d, len);

	return (buf);
}

int
ca_key_serialize(EVP_PKEY *key, struct iked_id *id)
{
	int		 len;
	u_int8_t	*d;
	RSA		*rsa;

	switch (key->type) {
	case EVP_PKEY_RSA:
		id->id_type = 0;
		id->id_offset = 0;
		ibuf_release(id->id_buf);

		if ((rsa = EVP_PKEY_get1_RSA(key)) == NULL)
			return (-1);
		if ((len = i2d_RSAPrivateKey(rsa, NULL)) <= 0)
			return (-1);
		if ((id->id_buf = ibuf_new(NULL, len)) == NULL)
			return (-1);

		d = ibuf_data(id->id_buf);
		if (i2d_RSAPrivateKey(rsa, &d) != len) {
			ibuf_release(id->id_buf);
			return (-1);
		}

		id->id_type = IKEV2_CERT_RSA_KEY;
		break;
	default:
		log_debug("%s: unsupported key type %d", __func__, key->type);
		return (-1);
	}

	return (0);
}

char *
ca_asn1_name(u_int8_t *asn1, size_t len)
{
	X509_NAME	*name = NULL;
	char		*str = NULL;
	const u_int8_t	*p;

	p = asn1;
	if ((name = d2i_X509_NAME(NULL, &p, len)) == NULL)
		return (NULL);
	str = ca_x509_name(name);
	X509_NAME_free(name);

	return (str);
}

char *
ca_x509_name(void *ptr)
{
	char		 buf[BUFSIZ];
	X509_NAME	*name = ptr;

	bzero(buf, sizeof(buf));
	if (!X509_NAME_oneline(name, buf, sizeof(buf) - 1))
		return (NULL);

	return (strdup(buf));
}

int
ca_validate_pubkey(struct iked *env, struct iked_static_id *id,
    void *data, size_t len)
{
	BIO		*rawcert = NULL;
	RSA		*rsa = NULL;
	EVP_PKEY	*peerkey = NULL, *localkey = NULL;
	int		 ret = -1;
	FILE		*fp = NULL;
	char		 idstr[IKED_ID_SIZE];
	char		 file[MAXPATHLEN];
	struct iked_id	 idp;

	if (len == 0 && data == NULL)
		return (-1);

	switch (id->id_type) {
	case IKEV2_ID_IPV4:
	case IKEV2_ID_FQDN:
	case IKEV2_ID_UFQDN:
	case IKEV2_ID_IPV6:
		break;
	default:
		/* Some types like ASN1_DN will not be mapped to file names */
		return (-1);
	}

	bzero(&idp, sizeof(idp));
	if ((idp.id_buf = ibuf_new(id->id_data, id->id_length)) == NULL)
		goto done;

	idp.id_type = id->id_type;
	idp.id_offset = id->id_offset;
	if (ikev2_print_id(&idp, idstr, sizeof(idstr)) == -1)
		goto done;

	if (len == 0) {
		/* Data is already an public key */
		peerkey = (EVP_PKEY *)data;
	} else {
		if ((rawcert = BIO_new_mem_buf(data, len)) == NULL)
			goto done;

		if ((rsa = d2i_RSAPublicKey_bio(rawcert, NULL)) == NULL)
			goto sslerr;
		if ((peerkey = EVP_PKEY_new()) == NULL)
			goto sslerr;
		if (!EVP_PKEY_set1_RSA(peerkey, rsa))
			goto sslerr;
	}

	lc_string(idstr);
	if (strlcpy(file, IKED_PUBKEY_DIR, sizeof(file)) >= sizeof(file) ||
	    strlcpy(file, idstr, sizeof(file)) >= sizeof(file))
		goto done;

	log_debug("%s: looking up %s", __func__, file);

	if ((fp = fopen(file, "r")) == NULL)
		goto done;

	localkey = PEM_read_PUBKEY(fp, NULL, NULL, NULL);
	fclose(fp);
	if (localkey == NULL)
		goto sslerr;

	if (!EVP_PKEY_cmp(peerkey, localkey))
		goto done;

	ret = 0;
 sslerr:
	if (ret != 0)
		ca_sslerror();
 done:
	ibuf_release(idp.id_buf);
	if (peerkey != NULL)
		EVP_PKEY_free(peerkey);
	if (rsa != NULL)
		RSA_free(rsa);
	if (rawcert != NULL)
		BIO_free(rawcert);

	return (ret);
}

int
ca_validate_cert(struct iked *env, struct iked_static_id *id,
    void *data, size_t len)
{
	struct ca_store	*store = env->sc_priv;
	X509_STORE_CTX	 csc;
	BIO		*rawcert = NULL;
	X509		*cert = NULL;
	int		 ret = -1, result, error;
	size_t		 idlen, idoff;
	const u_int8_t	*idptr;
	X509_NAME	*idname = NULL, *subject;
	const char	*errstr = "failed";

	if (len == 0) {
		/* Data is already an X509 certificate */
		cert = (X509 *)data;
	} else {
		/* Convert data to X509 certificate */
		if ((rawcert = BIO_new_mem_buf(data, len)) == NULL)
			goto done;
		if ((cert = d2i_X509_bio(rawcert, NULL)) == NULL)
			goto done;
	}

	/* Certificate needs a valid subjectName */
	if ((subject = X509_get_subject_name(cert)) == NULL) {
		errstr = "invalid subject";
		goto done;
	}

	if (id != NULL) {
		if ((ret = ca_validate_pubkey(env, id, X509_get_pubkey(cert),
		    0)) == 0) {
			errstr = "public key found, ok";
			goto done;
		}

		switch (id->id_type) {
		case IKEV2_ID_ASN1_DN:
			idoff = id->id_offset;
			if (id->id_length <= idoff) {
				errstr = "invalid ASN1_DN id length";
				goto done;
			}
			idlen = id->id_length - idoff;
			idptr = id->id_data + idoff;

			if ((idname = d2i_X509_NAME(NULL,
			    &idptr, idlen)) == NULL ||
			    X509_NAME_cmp(subject, idname) != 0) {
				errstr = "ASN1_DN identifier mismatch";
				goto done;
			}
			break;
		default:
			if (ca_x509_subjectaltname_cmp(cert, id) != 0) {
				errstr = "invalid subjectAltName extension";
				goto done;
			}
			break;
		}
	}

	bzero(&csc, sizeof(csc));
	X509_STORE_CTX_init(&csc, store->ca_cas, cert, NULL);
	if (store->ca_cas->param->flags & X509_V_FLAG_CRL_CHECK) {
		X509_STORE_CTX_set_flags(&csc, X509_V_FLAG_CRL_CHECK);
		X509_STORE_CTX_set_flags(&csc, X509_V_FLAG_CRL_CHECK_ALL);
	}

	result = X509_verify_cert(&csc);
	error = csc.error;
	X509_STORE_CTX_cleanup(&csc);
	if (error != 0) {
		errstr = X509_verify_cert_error_string(error);
		goto done;
	}

 	if (!result) {
		/* XXX should we accept self-signed certificates? */
		errstr = "rejecting self-signed certificate";
		goto done;
	}

	/* Success */
	ret = 0;

 done:
	if (cert != NULL)
		log_debug("%s: %s %.100s", __func__, cert->name,
		    ret == 0 ? "ok" : errstr);

	if (idname != NULL)
		X509_NAME_free(idname);
	if (rawcert != NULL) {
		BIO_free(rawcert);
		if (cert != NULL)
			X509_free(cert);
	}

	return (ret);
}

int
ca_x509_subjectaltname_cmp(X509 *cert, struct iked_static_id *id)
{
	struct iked_id	 sanid;
	char		 idstr[IKED_ID_SIZE];
	int		 ret = -1;

	bzero(&sanid, sizeof(sanid));

	if (ca_x509_subjectaltname(cert, &sanid) != 0)
		return (-1);

	ikev2_print_id(&sanid, idstr, sizeof(idstr));

	/* Compare id types, length and data */
	if ((id->id_type != sanid.id_type) ||
	    ((ssize_t)ibuf_size(sanid.id_buf) !=
	    (id->id_length - id->id_offset)) ||
	    (memcmp(id->id_data + id->id_offset,
	    ibuf_data(sanid.id_buf),
	    ibuf_size(sanid.id_buf)) != 0)) {
		log_debug("%s: %s mismatched", __func__, idstr);
		goto done;
	}

	ret = 0;
 done:
	ibuf_release(sanid.id_buf);
	return (ret);
}

int
ca_x509_subjectaltname(X509 *cert, struct iked_id *id)
{
	X509_EXTENSION	*san;
	u_int8_t	 sanhdr[4], *data;
	int		 ext, santype, sanlen;
	char		 idstr[IKED_ID_SIZE];

	if ((ext = X509_get_ext_by_NID(cert,
	    NID_subject_alt_name, -1)) == -1 ||
	    ((san = X509_get_ext(cert, ext)) == NULL)) {
		log_debug("%s: did not find subjectAltName in certificate",
		    __func__);
		return (-1);
	}

	if (san->value == NULL || san->value->data == NULL ||
	    san->value->length < (int)sizeof(sanhdr)) {
		log_debug("%s: invalid subjectAltName in certificate",
		    __func__);
		return (-1);
	}

	/* This is partially based on isakmpd's x509 subjectaltname code */
	data = (u_int8_t *)san->value->data;
	memcpy(&sanhdr, data, sizeof(sanhdr));
	santype = sanhdr[2] & 0x3f;
	sanlen = sanhdr[3];

	if ((sanlen + (int)sizeof(sanhdr)) > san->value->length) {
		log_debug("%s: invalid subjectAltName length", __func__);
		return (-1);
	}

	switch (santype) {
	case GEN_DNS:
		id->id_type = IKEV2_ID_FQDN;
		break;
	case GEN_EMAIL:
		id->id_type = IKEV2_ID_UFQDN;
		break;
	case GEN_IPADD:
		if (sanlen == 4)
			id->id_type = IKEV2_ID_IPV4;
		else if (sanlen == 16)
			id->id_type = IKEV2_ID_IPV6;
		else {
			log_debug("%s: invalid subjectAltName IP address",
			    __func__);
			return (-1);
		}
		break;
	default:
		log_debug("%s: unsupported subjectAltName type %d",
		    __func__, santype);
		return (-1);
	}

	ibuf_release(id->id_buf);
	if ((id->id_buf = ibuf_new(data + sizeof(sanhdr), sanlen)) == NULL) {
		log_debug("%s: failed to get id buffer", __func__);
		return (-1);
	}
	id->id_offset = 0;

	ikev2_print_id(id, idstr, sizeof(idstr));
	log_debug("%s: %s", __func__, idstr);

	return (0);
}

void
ca_sslinit(void)
{
	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();

	/* Init hardware crypto engines. */
	ENGINE_load_builtin_engines();
	ENGINE_register_all_complete();
}

void
ca_sslerror(void)
{
	u_long		 error;

	while ((error = ERR_get_error()) != 0)
		log_warn("%s: %.100s", __func__,
		    ERR_error_string(error, NULL));
}
