/*	$OpenBSD: tal.c,v 1.18 2020/04/11 15:52:24 deraadt Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <netinet/in.h>
#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <libgen.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/x509.h>

#include "extern.h"

/*
 * Inner function for parsing RFC 7730 from a buffer.
 * Returns a valid pointer on success, NULL otherwise.
 * The pointer must be freed with tal_free().
 */
static struct tal *
tal_parse_buffer(const char *fn, char *buf)
{
	char		*nl, *line;
	unsigned char	*b64 = NULL;
	size_t		 sz;
	int		 rc = 0, b64sz;
	struct tal	*tal = NULL;
	enum rtype	 rp;
	EVP_PKEY	*pkey = NULL;

	if ((tal = calloc(1, sizeof(struct tal))) == NULL)
		err(1, NULL);

	/* Begin with the URI section, comment section already removed. */
	while ((nl = strchr(buf, '\n')) != NULL) {
		line = buf;
		*nl = '\0';

		/* advance buffer to next line */
		buf = nl + 1;

		/* Zero-length line is end of section. */
		if (*line == '\0')
			break;

		/* ignore https URI for now. */
		if (strncasecmp(line, "https://", 8) == 0) {
			warnx("%s: https schema ignored", line);
			continue;
		}

		/* Append to list of URIs. */
		tal->uri = reallocarray(tal->uri,
			tal->urisz + 1, sizeof(char *));
		if (tal->uri == NULL)
			err(1, NULL);

		tal->uri[tal->urisz] = strdup(line);
		if (tal->uri[tal->urisz] == NULL)
			err(1, NULL);
		tal->urisz++;

		/* Make sure we're a proper rsync URI. */
		if (!rsync_uri_parse(NULL, NULL,
		    NULL, NULL, NULL, NULL, &rp, line)) {
			warnx("%s: RFC 7730 section 2.1: "
			    "failed to parse URL: %s", fn, line);
			goto out;
		}
		if (rp != RTYPE_CER) {
			warnx("%s: RFC 7730 section 2.1: "
			    "not a certificate URL: %s", fn, line);
			goto out;
		}

	}

	if (tal->urisz == 0) {
		warnx("%s: no URIs in manifest part", fn);
		goto out;
	} else if (tal->urisz > 1)
		warnx("%s: multiple URIs: using the first", fn);
		/* XXX no support for TAL files with multiple TALs yet */

	sz = strlen(buf);
	if (sz == 0) {
		warnx("%s: RFC 7730 section 2.1: subjectPublicKeyInfo: "
		    "zero-length public key", fn);
		goto out;
	}

	/* Now the BASE64-encoded public key. */
	sz = ((sz + 3) / 4) * 3 + 1;
	if ((b64 = malloc(sz)) == NULL)
		err(1, NULL);
	if ((b64sz = b64_pton(buf, b64, sz)) < 0)
		errx(1, "b64_pton");

	tal->pkey = b64;
	tal->pkeysz = b64sz;

	/* Make sure it's a valid public key. */
	pkey = d2i_PUBKEY(NULL, (const unsigned char **)&b64, b64sz);
	if (pkey == NULL) {
		cryptowarnx("%s: RFC 7730 section 2.1: subjectPublicKeyInfo: "
		    "failed public key parse", fn);
		goto out;
	}
	rc = 1;
out:
	if (rc == 0) {
		tal_free(tal);
		tal = NULL;
	}
	EVP_PKEY_free(pkey);
	return tal;
}

/*
 * Parse a TAL from "buf" conformant to RFC 7730 originally from a file
 * named "fn".
 * Returns the encoded data or NULL on syntax failure.
 */
struct tal *
tal_parse(const char *fn, char *buf)
{
	struct tal	*p;
	const char	*d;
	size_t		 dlen;

	p = tal_parse_buffer(fn, buf);
	if (p == NULL)
		return NULL;

	/* extract the TAL basename (without .tal suffix) */
	d = strrchr(fn, '/');
	if (d == NULL)
		d = fn;
	else
		d++;
	dlen = strlen(d);
	if (strcasecmp(d + dlen - 4, ".tal") == 0)
		dlen -= 4;
	if ((p->descr = malloc(dlen + 1)) == NULL)
		err(1, NULL);
	memcpy(p->descr, d, dlen);
	p->descr[dlen] = '\0';

	return p;
}

/*
 * Read the file named "file" into a returned, NUL-terminated buffer.
 * This replaces CRLF terminators with plain LF, if found, and also
 * elides document-leading comment lines starting with "#".
 * Files may not exceeds 4096 bytes.
 * This function exits on failure, so it always returns a buffer with
 * TAL data.
 */
char *
tal_read_file(const char *file)
{
	char		*nbuf, *line = NULL, *buf = NULL;
	FILE		*in;
	ssize_t		 n, i;
	size_t		 sz = 0, bsz = 0;
	int		 optcomment = 1;

	if ((in = fopen(file, "r")) == NULL)
		err(1, "fopen: %s", file);

	while ((n = getline(&line, &sz, in)) != -1) {
		/* replace CRLF with just LF */
		if (n > 1 && line[n - 1] == '\n' && line[n - 2] == '\r') {
			line[n - 2] = '\n';
			line[n - 1] = '\0';
			n--;
		}
		if (optcomment) {
			/* if this is comment, just eat the line */
			if (line[0] == '#')
				continue;
			optcomment = 0;
			/*
			 * Empty line is end of section and needs
			 * to be eaten as well.
			 */
			if (line[0] == '\n')
				continue;
		}

		/* make sure every line is valid ascii */
		for (i = 0; i < n; i++)
			if (!isprint((unsigned char)line[i]) &&
			    !isspace((unsigned char)line[i]))
				errx(1, "getline: %s: "
				    "invalid content", file);

		/* concat line to buf */
		if ((nbuf = realloc(buf, bsz + n + 1)) == NULL)
			err(1, NULL);
		if (buf == NULL)
			nbuf[0] = '\0';	/* initialize buffer */
		buf = nbuf;
		bsz += n + 1;
		if (strlcat(buf, line, bsz) >= bsz)
			errx(1, "strlcat overflow");
		/* limit the buffer size */
		if (bsz > 4096)
			errx(1, "%s: file too big", file);
	}

	free(line);
	if (ferror(in))
		err(1, "getline: %s", file);
	fclose(in);
	if (buf == NULL)
		errx(1, "%s: no data", file);
	return buf;
}

/*
 * Free a TAL pointer.
 * Safe to call with NULL.
 */
void
tal_free(struct tal *p)
{
	size_t	 i;

	if (p == NULL)
		return;

	if (p->uri != NULL)
		for (i = 0; i < p->urisz; i++)
			free(p->uri[i]);

	free(p->pkey);
	free(p->uri);
	free(p->descr);
	free(p);
}

/*
 * Buffer TAL parsed contents for writing.
 * See tal_read() for the other side of the pipe.
 */
void
tal_buffer(char **b, size_t *bsz, size_t *bmax, const struct tal *p)
{
	size_t	 i;

	io_buf_buffer(b, bsz, bmax, p->pkey, p->pkeysz);
	io_str_buffer(b, bsz, bmax, p->descr);
	io_simple_buffer(b, bsz, bmax, &p->urisz, sizeof(size_t));

	for (i = 0; i < p->urisz; i++)
		io_str_buffer(b, bsz, bmax, p->uri[i]);
}

/*
 * Read parsed TAL contents from descriptor.
 * See tal_buffer() for the other side of the pipe.
 * A returned pointer must be freed with tal_free().
 */
struct tal *
tal_read(int fd)
{
	size_t		 i;
	struct tal	*p;

	if ((p = calloc(1, sizeof(struct tal))) == NULL)
		err(1, NULL);

	io_buf_read_alloc(fd, (void **)&p->pkey, &p->pkeysz);
	assert(p->pkeysz > 0);
	io_str_read(fd, &p->descr);
	io_simple_read(fd, &p->urisz, sizeof(size_t));
	assert(p->urisz > 0);

	if ((p->uri = calloc(p->urisz, sizeof(char *))) == NULL)
		err(1, NULL);

	for (i = 0; i < p->urisz; i++)
		io_str_read(fd, &p->uri[i]);

	return p;
}
