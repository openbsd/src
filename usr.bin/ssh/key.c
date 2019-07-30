/* $OpenBSD: key.c,v 1.132 2017/12/18 02:25:15 djm Exp $ */
/*
 * placed in the public domain
 */

#include <sys/types.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>

#define SSH_KEY_NO_DEFINE
#include "key.h"

#include "compat.h"
#include "sshkey.h"
#include "ssherr.h"
#include "log.h"
#include "authfile.h"

static void
fatal_on_fatal_errors(int r, const char *func, int extra_fatal)
{
	if (r == SSH_ERR_INTERNAL_ERROR ||
	    r == SSH_ERR_ALLOC_FAIL ||
	    (extra_fatal != 0 && r == extra_fatal))
		fatal("%s: %s", func, ssh_err(r));
}

Key *
key_from_blob(const u_char *blob, u_int blen)
{
	int r;
	Key *ret = NULL;

	if ((r = sshkey_from_blob(blob, blen, &ret)) != 0) {
		fatal_on_fatal_errors(r, __func__, 0);
		error("%s: %s", __func__, ssh_err(r));
		return NULL;
	}
	return ret;
}

int
key_to_blob(const Key *key, u_char **blobp, u_int *lenp)
{
	u_char *blob;
	size_t blen;
	int r;

	if (blobp != NULL)
		*blobp = NULL;
	if (lenp != NULL)
		*lenp = 0;
	if ((r = sshkey_to_blob(key, &blob, &blen)) != 0) {
		fatal_on_fatal_errors(r, __func__, 0);
		error("%s: %s", __func__, ssh_err(r));
		return 0;
	}
	if (blen > INT_MAX)
		fatal("%s: giant len %zu", __func__, blen);
	if (blobp != NULL)
		*blobp = blob;
	if (lenp != NULL)
		*lenp = blen;
	return blen;
}

int
key_sign(const Key *key, u_char **sigp, u_int *lenp,
    const u_char *data, u_int datalen, const char *alg)
{
	int r;
	u_char *sig;
	size_t siglen;

	if (sigp != NULL)
		*sigp = NULL;
	if (lenp != NULL)
		*lenp = 0;
	if ((r = sshkey_sign(key, &sig, &siglen,
	    data, datalen, alg, datafellows)) != 0) {
		fatal_on_fatal_errors(r, __func__, 0);
		error("%s: %s", __func__, ssh_err(r));
		return -1;
	}
	if (siglen > INT_MAX)
		fatal("%s: giant len %zu", __func__, siglen);
	if (sigp != NULL)
		*sigp = sig;
	if (lenp != NULL)
		*lenp = siglen;
	return 0;
}

Key *
key_demote(const Key *k)
{
	int r;
	Key *ret = NULL;

	if ((r = sshkey_demote(k, &ret)) != 0)
		fatal("%s: %s", __func__, ssh_err(r));
	return ret;
}

int
key_drop_cert(Key *k)
{
	int r;

	if ((r = sshkey_drop_cert(k)) != 0) {
		fatal_on_fatal_errors(r, __func__, 0);
		error("%s: %s", __func__, ssh_err(r));
		return -1;
	}
	return 0;
}

int
key_cert_check_authority(const Key *k, int want_host, int require_principal,
    const char *name, const char **reason)
{
	int r;

	if ((r = sshkey_cert_check_authority(k, want_host, require_principal,
	    name, reason)) != 0) {
		fatal_on_fatal_errors(r, __func__, 0);
		error("%s: %s", __func__, ssh_err(r));
		return -1;
	}
	return 0;
}

/* authfile.c */

Key *
key_load_cert(const char *filename)
{
	int r;
	Key *ret = NULL;

	if ((r = sshkey_load_cert(filename, &ret)) != 0) {
		fatal_on_fatal_errors(r, __func__, SSH_ERR_LIBCRYPTO_ERROR);
		/* Old authfile.c ignored all file errors. */
		if (r == SSH_ERR_SYSTEM_ERROR)
			debug("%s: %s", __func__, ssh_err(r));
		else
			error("%s: %s", __func__, ssh_err(r));
		return NULL;
	}
	return ret;

}

Key *
key_load_public(const char *filename, char **commentp)
{
	int r;
	Key *ret = NULL;

	if ((r = sshkey_load_public(filename, &ret, commentp)) != 0) {
		fatal_on_fatal_errors(r, __func__, SSH_ERR_LIBCRYPTO_ERROR);
		/* Old authfile.c ignored all file errors. */
		if (r == SSH_ERR_SYSTEM_ERROR)
			debug("%s: %s", __func__, ssh_err(r));
		else
			error("%s: %s", __func__, ssh_err(r));
		return NULL;
	}
	return ret;
}

Key *
key_load_private(const char *path, const char *passphrase,
    char **commentp)
{
	int r;
	Key *ret = NULL;

	if ((r = sshkey_load_private(path, passphrase, &ret, commentp)) != 0) {
		fatal_on_fatal_errors(r, __func__, SSH_ERR_LIBCRYPTO_ERROR);
		/* Old authfile.c ignored all file errors. */
		if (r == SSH_ERR_SYSTEM_ERROR ||
		    r == SSH_ERR_KEY_WRONG_PASSPHRASE)
			debug("%s: %s", __func__, ssh_err(r));
		else
			error("%s: %s", __func__, ssh_err(r));
		return NULL;
	}
	return ret;
}

Key *
key_load_private_cert(int type, const char *filename, const char *passphrase,
    int *perm_ok)
{
	int r;
	Key *ret = NULL;

	if ((r = sshkey_load_private_cert(type, filename, passphrase,
	    &ret, perm_ok)) != 0) {
		fatal_on_fatal_errors(r, __func__, SSH_ERR_LIBCRYPTO_ERROR);
		/* Old authfile.c ignored all file errors. */
		if (r == SSH_ERR_SYSTEM_ERROR ||
		    r == SSH_ERR_KEY_WRONG_PASSPHRASE)
			debug("%s: %s", __func__, ssh_err(r));
		else
			error("%s: %s", __func__, ssh_err(r));
		return NULL;
	}
	return ret;
}

Key *
key_load_private_type(int type, const char *filename, const char *passphrase,
    char **commentp, int *perm_ok)
{
	int r;
	Key *ret = NULL;

	if ((r = sshkey_load_private_type(type, filename, passphrase,
	    &ret, commentp, perm_ok)) != 0) {
		fatal_on_fatal_errors(r, __func__, SSH_ERR_LIBCRYPTO_ERROR);
		/* Old authfile.c ignored all file errors. */
		if (r == SSH_ERR_SYSTEM_ERROR ||
		    (r == SSH_ERR_KEY_WRONG_PASSPHRASE))
			debug("%s: %s", __func__, ssh_err(r));
		else
			error("%s: %s", __func__, ssh_err(r));
		return NULL;
	}
	return ret;
}
