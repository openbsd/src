/* $OpenBSD: sign.c,v 1.8 2007/09/11 15:47:17 gilles Exp $ */

/*
 * sign.c
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
 * $Vendor: sign.c,v 1.2 2005/04/01 16:47:31 dugsong Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "extern.h"
#include "gzip.h"
#include "key.h"
#include "util.h"

static char *passphrase_file = NULL;

static int
embed_signature(struct key *key, FILE *fin, FILE *fout)
{
	struct gzip_header gh;
	struct gzip_xfield *gx;
	struct gzsig_data *gd;
	u_char *sig, digest[20], buf[8192];
	SHA_CTX ctx;
	int i, siglen;
	long offset;

	/* Read gzip header. */
	if (fread((u_char *)&gh, 1, sizeof(gh), fin) != sizeof(gh)) {
		fprintf(stderr, "Error reading gzip header: %s\n",
		    strerror(errno));
		return (-1);
	}
	/* Verify gzip header. */
	if (memcmp(gh.magic, GZIP_MAGIC, sizeof(gh.magic)) != 0) {
		fprintf(stderr, "Invalid gzip file\n");
		return (-1);
	}
	if (gh.flags & GZIP_FCONT) {
		fprintf(stderr, "Multi-part gzip files not supported\n");
		return (-1);
	}
	/* Skip over any existing signature. */
	if (gh.flags & GZIP_FEXTRA) {
		gx = (struct gzip_xfield *)buf;
		gd = (struct gzsig_data *)(gx + 1);

		if (fread((u_char *)gx, 1, sizeof(*gx), fin) != sizeof(*gx)) {
			fprintf(stderr, "Error reading extra field: %s\n",
			    strerror(errno));
			return (-1);
		}
		if (memcmp(gx->subfield.id, GZSIG_ID, 2) != 0) {
			fprintf(stderr, "Unknown extra field\n");
			return (-1);
		}
		gx->subfield.len = letoh16(gx->subfield.len);
		
		if (gx->subfield.len < sizeof(*gd) ||
		    gx->subfield.len > sizeof(buf) - sizeof(*gx)) {
			fprintf(stderr, "Invalid signature length\n");
			return (-1);
		}
		if (fread((u_char *)gd, 1, gx->subfield.len, fin) !=
		    gx->subfield.len) {
			fprintf(stderr, "Error reading signature: %s\n",
			    strerror(errno));
			return (-1);
		}
		fprintf(stderr, "Overwriting existing signature\n");
	}
	/* Skip over any options. */
	offset = ftell(fin);

	if (gh.flags & GZIP_FNAME) {
		while (getc(fin) != '\0')
			;
	}
	if (gh.flags & GZIP_FCOMMENT) {
		while (getc(fin) != '\0')
			;
	}
	if (gh.flags & GZIP_FENCRYPT) {
		if (fread(buf, 1, GZIP_FENCRYPT_LEN, fin) != GZIP_FENCRYPT_LEN)
			return (-1);
	}
	/* Compute checksum over compressed data and trailer. */
	SHA1_Init(&ctx);
	
	while ((i = fread(buf, 1, sizeof(buf), fin)) > 0) {
		SHA1_Update(&ctx, buf, i);
	}
	SHA1_Final(digest, &ctx);
	
	/* Generate signature. */
	gx = (struct gzip_xfield *)buf;
	gd = (struct gzsig_data *)(gx + 1);
	sig = (u_char *)(gd + 1);
	
	siglen = key_sign(key, digest, sizeof(digest), sig,
	    sizeof(buf) - (sig - buf));
	
	if (siglen < 0) {
		fprintf(stderr, "Error signing checksum\n");
		return (-1);
	}
	i = sizeof(*gd) + siglen;
	gx->subfield.len = htole16(i);
	gx->len = htole16(sizeof(gx->subfield) + i);
	memcpy(gx->subfield.id, GZSIG_ID, sizeof(gx->subfield.id));
	gd->version = GZSIG_VERSION;
	
	/* Write out gzip header. */
	gh.flags |= GZIP_FEXTRA;

	if (fwrite((u_char *)&gh, 1, sizeof(gh), fout) != sizeof(gh)) {
		fprintf(stderr, "Error writing output: %s\n", strerror(errno));
		return (-1);
	}
	/* Write out signature. */
	if (fwrite(buf, 1, sizeof(*gx) + i, fout) != sizeof(*gx) + i) {
		fprintf(stderr, "Error writing output: %s\n", strerror(errno));
		return (-1);
	}
	/* Write out options, compressed data, and trailer. */
	if (fseek(fin, offset, SEEK_SET) < 0) {
		fprintf(stderr, "Error writing output: %s\n", strerror(errno));
		return (-1);
	}
	while ((i = fread(buf, 1, sizeof(buf), fin)) > 0) {		
		if (fwrite(buf, 1, i, fout) != i) {
			fprintf(stderr, "Error writing output: %s\n",
			    strerror(errno));
			return (-1);
		}
	}
	if (ferror(fin)) {
		fprintf(stderr, "Error reading input: %s\n", strerror(errno));
		return (-1);
	}
	return (0);
}

void
sign_usage(void)
{
	fprintf(stderr, "Usage: gzsig sign [-q] [-f secret_file] privkey [file ...]\n");
}

int
sign_passwd_cb(char *buf, int size, int rwflag, void *u)
{
	char *p;
	FILE *f;

	if (passphrase_file != NULL) {
		if ((f = fopen(passphrase_file, "r")) == NULL)
			err(1, "fopen(%.64s)", passphrase_file);
		if (fgets(buf, size, f) == NULL)
			err(1, "fgets(%.64s)", passphrase_file);
		fclose(f);
		buf[strcspn(buf, "\n")] = '\0';
	} else {
		p = getpass("Enter passphrase: ");
		if (strlcpy(buf, p, size) >= size)
			errx(1, "Passphrase too long");
		memset(p, 0, strlen(p));
	}

	return (strlen(buf));
}

void
sign(int argc, char *argv[])
{
	struct key *key;
	char *gzipfile, tmppath[MAXPATHLEN];
	FILE *fin, *fout;
	int i, fd, error, qflag;

	qflag = 0;
	
	while ((i = getopt(argc, argv, "qvf:")) != -1) {
		switch (i) {
		case 'q':
			qflag = 1;
			break;
		case 'v':
			qflag = 0;
			break;
		case 'f':
			passphrase_file = optarg;
			break;
		default:
			sign_usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		sign_usage();
		exit(1);
	}
	OpenSSL_add_all_algorithms();
	
	if ((key = key_new()) == NULL)
		fatal(1, "Couldn't initialize private key");
	
	if (key_load_private(key, argv[0]) < 0)
		fatal(1, "Couldn't load private key");
	
	if (argc == 1 || *argv[1] == '-') {
		argc = 0;
		
		if (embed_signature(key, stdin, stdout) == 0) {
			if (!qflag)
				fprintf(stderr, "Signed input\n");
		} else
			fatal(1, "Couldn't sign input");
	}
	for (i = 1; i < argc; i++) {
		gzipfile = argv[i];

		if ((fin = fopen(gzipfile, "r+")) == NULL) {
			fprintf(stderr,  "Error opening %s: %s\n",
			    gzipfile, strerror(errno));
			continue;
		}
		snprintf(tmppath, sizeof(tmppath), "%s.XXXXXX", gzipfile);
		
		if ((fd = mkstemp(tmppath)) < 0) {
			fprintf(stderr, "Error creating %s: %s\n",
			    tmppath, strerror(errno));
			fclose(fin);
			continue;
		}
		if ((fout = fdopen(fd, "w")) == NULL) {
			fprintf(stderr, "Error opening %s: %s\n",
			    tmppath, strerror(errno));
			fclose(fin);
			close(fd);
			continue;
		}
		if (copy_permissions(gzipfile, tmppath) < 0) {
			fprintf(stderr, "Error initializing %s: %s\n",
			    tmppath, strerror(errno));
			fclose(fin);
			fclose(fout);
			continue;
		}
		error = embed_signature(key, fin, fout);
		
		fclose(fin);
		fclose(fout);

		if (!error) {
			if (rename(tmppath, gzipfile) < 0) {
				unlink(tmppath);
				fatal(1, "Couldn't sign %s", gzipfile);
			}
			if (!qflag)
				fprintf(stderr, "Signed %s\n", gzipfile);
		} else {
			unlink(tmppath);
			fatal(1, "Couldn't sign %s", gzipfile);
		}
	}
	key_free(key);
}
