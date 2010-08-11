/* $OpenBSD: key.c,v 1.6 2010/08/11 18:38:30 jasper Exp $ */

/*
 * key.c
 *
 * Copyright (c) 2001 Dug Song <dugsong@arbor.net>
 * Copyright (c) 2001 Arbor Networks, Inc.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 * 
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. The names of the copyright holders may not be used to endorse or
 *      promote products derived from this software without specific
 *      prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 *   AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 *   THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Vendor: key.c,v 1.2 2005/04/01 16:47:31 dugsong Exp $
 */

#include <sys/limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <openssl/ssl.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "key.h"
#include "ssh.h"
#include "ssh2.h"
#include "util.h"
#include "x509.h"

typedef int (*key_loader)(struct key *, struct iovec *);

static key_loader pubkey_loaders[] = {
	ssh_load_public,
	ssh2_load_public,
	x509_load_public,
	NULL
};

static key_loader privkey_loaders[] = {
	ssh_load_private,
	x509_load_private,
	NULL
};

static int
load_file(struct iovec *iov, char *filename)
{
	struct stat st;
	int fd;
	int rval = -1;
	
	if ((fd = open(filename, O_RDONLY)) < 0)
		goto done;
	
	if (fstat(fd, &st) < 0)
		goto done;
	
	if (st.st_size == 0 || st.st_size >= SIZE_MAX) {
		errno = EINVAL;
		goto done;
	}
	if ((iov->iov_base = malloc(st.st_size + 1)) == NULL)
		goto done;

	iov->iov_len = st.st_size;
	((u_char *)iov->iov_base)[iov->iov_len] = '\0';
	
	if (read(fd, iov->iov_base, iov->iov_len) != iov->iov_len) {
		free(iov->iov_base);
		goto done;
	}

	rval = 0;

done:
	if (fd != -1)
	    close(fd);
	return (rval);
}

struct key *
key_new(void)
{
	return (calloc(1, sizeof(struct key)));
}

int
key_load_private(struct key *k, char *filename)
{
	struct iovec iov;
	int i;
	
	if (load_file(&iov, filename) < 0)
		return (-1);

	for (i = 0; privkey_loaders[i] != NULL; i++) {
		if (privkey_loaders[i](k, &iov) == 0)
			return (0);
	}
	return (-1);
}

int
key_load_public(struct key *k, char *filename)
{
	struct iovec iov;
	int i;

	if (load_file(&iov, filename) < 0)
		return (-1);

	for (i = 0; pubkey_loaders[i] != NULL; i++) {
		if (pubkey_loaders[i](k, &iov) == 0)
			return (0);
	}
	return (-1);
}

int
key_sign(struct key *k, u_char *msg, int mlen, u_char *sig, int slen)
{
	switch (k->type) {
	case KEY_RSA:
		if (RSA_size((RSA *)k->data) > slen) {
			fprintf(stderr, "RSA modulus too large: %d bits\n",
			    RSA_size((RSA *)k->data));
			return (-1);
		}
		if (RSA_sign(NID_sha1, msg, mlen, sig, &slen,
		    (RSA *)k->data) <= 0) {
			fprintf(stderr, "RSA signing failed\n");
			return (-1);
		}
		break;

	case KEY_DSA:
		if (DSA_size((DSA *)k->data) > slen) {
			fprintf(stderr, "DSA signature size too large: "
			    "%d bits\n", DSA_size((DSA *)k->data));
			return (-1);
		}
		if (DSA_sign(NID_sha1, msg, mlen, sig, &slen,
		    (DSA *)k->data) <= 0) {
			fprintf(stderr, "DSA signing failed\n");
			return (-1);
		}
		break;

	default:
		fprintf(stderr, "Unknown key type: %d\n", k->type);
		return (-1);
	}
	return (slen);
}

int
key_verify(struct key *k, u_char *msg, int mlen, u_char *sig, int slen)
{
	switch (k->type) {

	case KEY_RSA:
		if (RSA_verify(NID_sha1, msg, mlen,
		    sig, slen, (RSA *)k->data) <= 0) {
			fprintf(stderr, "RSA verification failed\n");
			return (-1);
		}
		break;

	case KEY_DSA:
		if (DSA_verify(NID_sha1, msg, mlen,
		    sig, slen, (DSA *)k->data) <= 0) {
			fprintf(stderr, "DSA verification failed\n");
			return (-1);
		}
		break;

	default:
		fprintf(stderr, "Unknown key type: %d\n", k->type);
		return (-1);
	}
	return (slen);
}

void
key_free(struct key *k)
{
	if (k->type == KEY_RSA)
		RSA_free((RSA *)k->data);
	else if (k->type == KEY_DSA)
		DSA_free((DSA *)k->data);
	else if (k->data != NULL)
		free(k->data);
	
	free(k);
}
