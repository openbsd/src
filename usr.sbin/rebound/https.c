/* $OpenBSD: https.c,v 1.2 2018/12/06 17:43:30 tedu Exp $ */
/*
 * Copyright (c) 2018 Ted Unangst <tedu@openbsd.org>
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

/*
 * this file attempts to implement a client for RFC 8484:
 * DNS Queries over HTTPS (DoH)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>

#include <tls.h>

void logmsg(int prio, const char *msg, ...);
void __dead logerr(const char *msg, ...);

static struct tls_config *config;
static struct tls *ctx;

static int connected;
static const char *servername;

int
https_init(void)
{
	int rv;

	config = tls_config_new();
	if (!config) {
		logmsg(LOG_NOTICE, "failed to create tls config");
		return -1;
	}
	tls_config_set_ca_file(config, tls_default_ca_cert_file());
	ctx = tls_client();
	if (!ctx) {
		logmsg(LOG_NOTICE, "failed to create tls client");
		goto fail;
	}
	rv = tls_configure(ctx, config);
	if (rv != 0) {
		logmsg(LOG_NOTICE, "failed to config tls client");
		goto fail;
	}
	return 0;

fail:
	tls_free(ctx);
	ctx = NULL;
	tls_config_free(config);
	config = NULL;
	return -1;
}

int
https_connect(const char *ip, const char *name)
{
	const char *port = "443";
	int rv;

	if (!ctx)
		return -1;

	if (connected)
		tls_close(ctx);
	connected = 0;
	tls_reset(ctx);
	rv = tls_configure(ctx, config);
	if (rv != 0) {
		logmsg(LOG_NOTICE, "failed to reconfig tls client");
		return -1;
	}
	rv = tls_connect_servername(ctx, ip, port, name);
	if (rv != 0) {
		logmsg(LOG_NOTICE, "failed to make tls connection: %s",
		    tls_error(ctx));
		goto fail;
	}
	servername = name;
	connected = 1;
	return 0;

fail:
	return -1;
}

static const char headerfmt[] =
	"POST /dns-query HTTP/1.1\r\n"
	"Host: %s\r\n"
	"User-Agent: rebound 6.4\r\n"
	"Accept: application/dns-message\r\n"
	"Content-Type: application/dns-message\r\n"
	"Content-Length: %zu\r\n"
	"\r\n";

int
https_query(uint8_t *query, size_t qlen, uint8_t *resp, size_t *resplen)
{
	char header[1024];
	unsigned char buf[65536 + 1024];
	unsigned char *two, *clptr, *endptr, *dataptr;
	ssize_t amt, headerlen, have, needlen, contlen;
	const char *errstr;

	if (!connected)
		return -1;

	headerlen = snprintf(header, sizeof(header), headerfmt, servername, qlen);
	amt = tls_write(ctx, header, headerlen);
	if (amt != headerlen)
		goto badwrite;
	amt = tls_write(ctx, query, qlen);
	if (amt != qlen)
		goto badwrite;
	amt = tls_read(ctx, buf, sizeof(buf) - 1);
	/* what in the world is going on here? */
	if (amt < 10)
		goto bad1;
	buf[amt] = 0;
	two = buf;
	while (*two && two < buf + 10 && *two != '2')
		two++;
	if (*two != '2')
		goto bad1;
	if (memcmp(two, "200", 3) != 0)
		goto notok;
	clptr = strcasestr(buf, "content-length:");
	if (!clptr)
		goto fail;
	while (*clptr && !isdigit(*clptr))
		clptr++;
	if (!isdigit(*clptr))
		goto fail;
	endptr = clptr;
	while (*endptr && *endptr != '\r')
		endptr++;
	if (*endptr != '\r')
		goto fail;
	*endptr = 0;
	contlen = strtonum(clptr, 0, 65536, &errstr);
	if (errstr)
		goto fail;
	dataptr = memmem(endptr + 1, amt - (endptr + 1 - buf), "\r\n\r\n", 4);
	if (!dataptr)
		goto fail;
	dataptr += 4;
	have = amt - (dataptr - buf);
	if (have > contlen)
		have = contlen;
	memcpy(resp, dataptr, have);
	needlen = contlen;
	needlen -= have;
	while (needlen > 0) {
		amt = tls_read(ctx, buf, needlen);
		if (amt <= 0)
			break;
		memcpy(resp + have, buf, amt);
		have += amt;
		needlen -= amt;
	}
	*resplen = have;

	return 0;

badwrite:
	logmsg(LOG_NOTICE, "unable to write request");
	return -1;
bad1:
	logmsg(LOG_NOTICE, "didn't receive enough data for anything");
	return -1;
notok:
	logmsg(LOG_NOTICE, "http response was not 200");
	return -1;
fail:
	logmsg(LOG_NOTICE, "some other problem with response");
	return -1;
}
