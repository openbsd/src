/* $OpenBSD: verify.c,v 1.7 2007/10/12 19:52:06 jasper Exp $ */

/*
 * verify.c
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
 * $Vendor: verify.c,v 1.3 2005/04/07 23:19:35 dugsong Exp $
 */

#include <sys/types.h>

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"
#include "gzip.h"
#include "key.h"
#include "util.h"

static int
verify_signature(struct key *key, FILE *fin)
{
	struct gzip_header gh;
	struct gzip_xfield *gx;
	struct gzsig_data *gd;
	u_char *sig, digest[20], buf[8192], sbuf[4096];
	SHA_CTX ctx;
	int i, siglen;

	/* Read gzip header. */
	if ((i = fread((u_char *)&gh, 1, sizeof(gh), fin)) != sizeof(gh)) {
		fprintf(stderr, "Error reading gzip header: %s\n",
		    strerror(errno));
		return (-1);
	}
	/* Verify gzip header. */
	if (memcmp(gh.magic, GZIP_MAGIC, sizeof(gh.magic)) != 0) {
		fprintf(stderr, "Invalid gzip file\n");
		return (-1);
	} else if (gh.flags & GZIP_FCONT){
		fprintf(stderr, "Multi-part gzip files not supported\n");
		return (-1);
	} else if ((gh.flags & GZIP_FEXTRA) == 0) {
		fprintf(stderr, "No gzip signature found\n");
		return (-1);
	}
	/* Read signature. */
	gx = (struct gzip_xfield *)buf;
	
	if ((i = fread((u_char *)gx, 1, sizeof(*gx), fin)) != sizeof(*gx)) {
		fprintf(stderr, "Error reading extra field: %s\n",
		    strerror(errno));
		return (-1);
	}
	if (memcmp(gx->subfield.id, GZSIG_ID, sizeof(gx->subfield.id)) != 0) {
		fprintf(stderr, "Unknown extra field\n");
		return (-1);
	}
	gx->subfield.len = letoh16(gx->subfield.len);

	if (gx->subfield.len <= 0 || gx->subfield.len > sizeof(sbuf)) {
		fprintf(stderr, "Invalid signature length\n");
		return (-1);
	}
	gd = (struct gzsig_data *)sbuf;
	
	if ((i = fread((u_char *)gd, 1, gx->subfield.len, fin)) !=
	    gx->subfield.len) {
		fprintf(stderr, "Error reading signature: %s\n",
		    strerror(errno));
		return (-1);
	}
	/* Skip over any options. */
	if (gh.flags & GZIP_FNAME) {
		while (getc(fin) != '\0')
			;
	}
	if (gh.flags & GZIP_FCOMMENT) {
		while (getc(fin) != '\0')
			;
	}
	if (gh.flags & GZIP_FENCRYPT &&
	    fread(buf, 1, GZIP_FENCRYPT_LEN, fin) != GZIP_FENCRYPT_LEN)
		return (-1);
	
	/* Check signature version. */
	if (gd->version != GZSIG_VERSION) {
		fprintf(stderr, "Unknown signature version: %d\n",
		    gd->version);
		return (-1);
	}
	/* Compute SHA1 checksum over compressed data and trailer. */
	sig = (u_char *)(gd + 1);
	siglen = gx->subfield.len - sizeof(*gd);

	SHA1_Init(&ctx);
	
	while ((i = fread(buf, 1, sizeof(buf), fin)) > 0) {
		SHA1_Update(&ctx, buf, i);
	}
	SHA1_Final(digest, &ctx);
	
	/* Verify signature. */
	if (key_verify(key, digest, sizeof(digest), sig, siglen) < 0) {
		fprintf(stderr, "Error verifying signature\n");
		return (-1);
	}
	return (0);
}

void
verify_usage(void)
{
	fprintf(stderr, "Usage: %s verify [-q] [-f secret_file] pubkey [file ...]\n",
	    __progname);
}

void
verify(int argc, char *argv[])
{
	struct key *key;
	char *gzipfile;
	FILE *fin;
	int i, error, qflag;

	qflag = 0;
	
	while ((i = getopt(argc, argv, "qv")) != -1) {
		switch (i) {
		case 'q':
			qflag = 1;
			break;
		case 'v':
			qflag = 0;
			break;
		default:
			verify_usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		verify_usage();
		exit(1);
	}
	OpenSSL_add_all_algorithms();
	
	if ((key = key_new()) == NULL)
		fatal(1, "Can't initialize public key");
	
	if (key_load_public(key, argv[0]) < 0)
		fatal(1, "Can't load public key");

	if (argc == 1 || *argv[1] == '-') {
		argc = 0;
		
		if (verify_signature(key, stdin) == 0) {
			if (!qflag)
				fprintf(stderr, "Verified input\n");
		} else
			fatal(1, "Couldn't verify input");
	}
	for (i = 1; i < argc; i++) {
		gzipfile = argv[i];

		if ((fin = fopen(gzipfile, "r")) == NULL) {
			fprintf(stderr,  "Couldn't open %s: %s\n",
			    gzipfile, strerror(errno));
			continue;
		}
		error = verify_signature(key, fin);
		fclose(fin);

		if (!error) {
			if (!qflag)
				fprintf(stderr, "Verified %s\n", gzipfile);
		} else
			fatal(1, "Couldn't verify %s", gzipfile);
	}
	key_free(key);
}
