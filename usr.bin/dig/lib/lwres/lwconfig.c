/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*! \file */

/**
 * Module for parsing resolv.conf files.
 *
 *    lwres_conf_init() creates an empty lwres_conf_t structure for
 *    lightweight resolver context ctx.
 *
 *    lwres_conf_clear() frees up all the internal memory used by that
 *    lwres_conf_t structure in resolver context ctx.
 *
 *    lwres_conf_parse() opens the file filename and parses it to initialise
 *    the resolver context ctx's lwres_conf_t structure.
 *
 * \section lwconfig_return Return Values
 *
 *    lwres_conf_parse() returns #LWRES_R_SUCCESS if it successfully read and
 *    parsed filename. It returns #LWRES_R_FAILURE if filename could not be
 *    opened or contained incorrect resolver statements.
 *
 * \section lwconfig_see See Also
 *
 *    stdio(3), \link resolver resolver \endlink
 *
 * \section files Files
 *
 *    /etc/resolv.conf
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <lwres/lwres.h>
#include <lwres/result.h>

static lwres_result_t
lwres_conf_parsenameserver(lwres_conf_t *confdata,  FILE *fp);

static lwres_result_t
lwres_conf_parselwserver(lwres_conf_t *confdata,  FILE *fp);

static lwres_result_t
lwres_conf_parsedomain(lwres_conf_t *confdata, FILE *fp);

static lwres_result_t
lwres_conf_parsesearch(lwres_conf_t *confdata,  FILE *fp);

static lwres_result_t
lwres_conf_parsesortlist(lwres_conf_t *confdata,  FILE *fp);

static lwres_result_t
lwres_conf_parseoption(lwres_conf_t *confdata,  FILE *fp);

static void
lwres_resetaddr(lwres_addr_t *addr);

static lwres_result_t
lwres_create_addr(const char *buff, lwres_addr_t *addr, int convert_zero);

/*!
 * Eat characters from FP until EOL or EOF. Returns EOF or '\n'
 */
static int
eatline(FILE *fp) {
	int ch;

	ch = fgetc(fp);
	while (ch != '\n' && ch != EOF)
		ch = fgetc(fp);

	return (ch);
}

/*!
 * Eats white space up to next newline or non-whitespace character (of
 * EOF). Returns the last character read. Comments are considered white
 * space.
 */
static int
eatwhite(FILE *fp) {
	int ch;

	ch = fgetc(fp);
	while (ch != '\n' && ch != EOF && isspace((unsigned char)ch))
		ch = fgetc(fp);

	if (ch == ';' || ch == '#')
		ch = eatline(fp);

	return (ch);
}

/*!
 * Skip over any leading whitespace and then read in the next sequence of
 * non-whitespace characters. In this context newline is not considered
 * whitespace. Returns EOF on end-of-file, or the character
 * that caused the reading to stop.
 */
static int
getword(FILE *fp, char *buffer, size_t size) {
	int ch;
	char *p = buffer;

	assert(buffer != NULL);
	assert(size > 0U);

	*p = '\0';

	ch = eatwhite(fp);

	if (ch == EOF)
		return (EOF);

	do {
		*p = '\0';

		if (ch == EOF || isspace((unsigned char)ch))
			break;
		else if ((size_t) (p - buffer) == size - 1)
			return (EOF);	/* Not enough space. */

		*p++ = (char)ch;
		ch = fgetc(fp);
	} while (1);

	return (ch);
}

static void
lwres_resetaddr(lwres_addr_t *addr) {
	assert(addr != NULL);

	memset(addr, 0, sizeof(*addr));
}

/*% initializes data structure for subsequent config parsing. */
void
lwres_conf_init(lwres_conf_t *confdata, int lwresflags) {
	int i;

	confdata->nsnext = 0;
	confdata->lwnext = 0;
	confdata->domainname = NULL;
	confdata->searchnxt = 0;
	confdata->sortlistnxt = 0;
	confdata->resdebug = 0;
	confdata->ndots = 1;
	confdata->no_tld_query = 0;
	confdata->flags = lwresflags;

	for (i = 0; i < LWRES_CONFMAXNAMESERVERS; i++)
		lwres_resetaddr(&confdata->nameservers[i]);

	for (i = 0; i < LWRES_CONFMAXSEARCH; i++)
		confdata->search[i] = NULL;

	for (i = 0; i < LWRES_CONFMAXSORTLIST; i++) {
		lwres_resetaddr(&confdata->sortlist[i].addr);
		lwres_resetaddr(&confdata->sortlist[i].mask);
	}
}

/*% Frees up all the internal memory used by the config data structure, returning it to the lwres_context_t. */
void
lwres_conf_clear(lwres_conf_t *confdata) {
	int i;

	for (i = 0; i < confdata->nsnext; i++)
		lwres_resetaddr(&confdata->nameservers[i]);

	free(confdata->domainname);
	confdata->domainname = NULL;

	for (i = 0; i < confdata->searchnxt; i++) {
		free(confdata->search[i]);
		confdata->search[i] = NULL;
	}

	for (i = 0; i < LWRES_CONFMAXSORTLIST; i++) {
		lwres_resetaddr(&confdata->sortlist[i].addr);
		lwres_resetaddr(&confdata->sortlist[i].mask);
	}

	confdata->nsnext = 0;
	confdata->lwnext = 0;
	confdata->domainname = NULL;
	confdata->searchnxt = 0;
	confdata->sortlistnxt = 0;
	confdata->resdebug = 0;
	confdata->ndots = 1;
	confdata->no_tld_query = 0;
}

static lwres_result_t
lwres_conf_parsenameserver(lwres_conf_t *confdata,  FILE *fp) {
	char word[LWRES_CONFMAXLINELEN];
	int res, use_ipv4, use_ipv6;
	lwres_addr_t address;

	if (confdata->nsnext == LWRES_CONFMAXNAMESERVERS)
		return (LWRES_R_SUCCESS);

	res = getword(fp, word, sizeof(word));
	if (strlen(word) == 0U)
		return (LWRES_R_FAILURE); /* Nothing on line. */
	else if (res == ' ' || res == '\t')
		res = eatwhite(fp);

	if (res != EOF && res != '\n')
		return (LWRES_R_FAILURE); /* Extra junk on line. */

	res = lwres_create_addr(word, &address, 1);
	use_ipv4 = confdata->flags & LWRES_USEIPV4;
	use_ipv6 = confdata->flags & LWRES_USEIPV6;
	if (res == LWRES_R_SUCCESS &&
	    ((address.family == LWRES_ADDRTYPE_V4 && use_ipv4) ||
	    (address.family == LWRES_ADDRTYPE_V6 && use_ipv6))) {
		confdata->nameservers[confdata->nsnext++] = address;
	}

	return (LWRES_R_SUCCESS);
}

static lwres_result_t
lwres_conf_parselwserver(lwres_conf_t *confdata,  FILE *fp) {
	char word[LWRES_CONFMAXLINELEN];
	int res;

	if (confdata->lwnext == LWRES_CONFMAXLWSERVERS)
		return (LWRES_R_SUCCESS);

	res = getword(fp, word, sizeof(word));
	if (strlen(word) == 0U)
		return (LWRES_R_FAILURE); /* Nothing on line. */
	else if (res == ' ' || res == '\t')
		res = eatwhite(fp);

	if (res != EOF && res != '\n')
		return (LWRES_R_FAILURE); /* Extra junk on line. */

	res = lwres_create_addr(word,
				&confdata->lwservers[confdata->lwnext++], 1);
	if (res != LWRES_R_SUCCESS)
		return (res);

	return (LWRES_R_SUCCESS);
}

static lwres_result_t
lwres_conf_parsedomain(lwres_conf_t *confdata,  FILE *fp) {
	char word[LWRES_CONFMAXLINELEN];
	int res, i;

	res = getword(fp, word, sizeof(word));
	if (strlen(word) == 0U)
		return (LWRES_R_FAILURE); /* Nothing else on line. */
	else if (res == ' ' || res == '\t')
		res = eatwhite(fp);

	if (res != EOF && res != '\n')
		return (LWRES_R_FAILURE); /* Extra junk on line. */

	free(confdata->domainname);

	/*
	 * Search and domain are mutually exclusive.
	 */
	for (i = 0; i < LWRES_CONFMAXSEARCH; i++) {
		free(confdata->search[i]);
		confdata->search[i] = NULL;
	}
	confdata->searchnxt = 0;

	confdata->domainname = strdup(word);

	if (confdata->domainname == NULL)
		return (LWRES_R_FAILURE);

	return (LWRES_R_SUCCESS);
}

static lwres_result_t
lwres_conf_parsesearch(lwres_conf_t *confdata,  FILE *fp) {
	int idx, delim;
	char word[LWRES_CONFMAXLINELEN];

	free(confdata->domainname);
	confdata->domainname = NULL;

	/*
	 * Remove any previous search definitions.
	 */
	for (idx = 0; idx < LWRES_CONFMAXSEARCH; idx++) {
		free(confdata->search[idx]);
		confdata->search[idx] = NULL;
	}
	confdata->searchnxt = 0;

	delim = getword(fp, word, sizeof(word));
	if (strlen(word) == 0U)
		return (LWRES_R_FAILURE); /* Nothing else on line. */

	idx = 0;
	while (strlen(word) > 0U) {
		if (confdata->searchnxt == LWRES_CONFMAXSEARCH)
			goto ignore; /* Too many domains. */

		confdata->search[idx] = strdup(word);
		if (confdata->search[idx] == NULL)
			return (LWRES_R_FAILURE);
		idx++;
		confdata->searchnxt++;

	ignore:
		if (delim == EOF || delim == '\n')
			break;
		else
			delim = getword(fp, word, sizeof(word));
	}

	return (LWRES_R_SUCCESS);
}

static lwres_result_t
lwres_create_addr(const char *buffer, lwres_addr_t *addr, int convert_zero) {
	struct in_addr v4;
	struct in6_addr v6;
	char buf[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255") +
		 sizeof("%4294967295")];
	char *percent;
	size_t n;

	n = strlcpy(buf, buffer, sizeof(buf));
	if (n >= sizeof(buf))
		return (LWRES_R_FAILURE);

	percent = strchr(buf, '%');
	if (percent != NULL)
		*percent = 0;

	if (inet_aton(buffer, &v4) == 1) {
		if (convert_zero) {
			unsigned char zeroaddress[] = {0, 0, 0, 0};
			unsigned char loopaddress[] = {127, 0, 0, 1};
			if (memcmp(&v4, zeroaddress, 4) == 0)
				memmove(&v4, loopaddress, 4);
		}
		addr->family = LWRES_ADDRTYPE_V4;
		addr->length = sizeof(v4);
		addr->zone = 0;
		memcpy(addr->address, &v4, sizeof(v4));

	} else if (inet_pton(AF_INET6, buf, &v6) == 1) {
		addr->family = LWRES_ADDRTYPE_V6;
		addr->length = sizeof(v6);
		memcpy(addr->address, &v6, sizeof(v6));
		if (percent != NULL) {
			unsigned long zone;
			char *ep;

			percent++;

			zone = if_nametoindex(percent);
			if (zone != 0U) {
				addr->zone = zone;
				return (LWRES_R_SUCCESS);
			}
			zone = strtoul(percent, &ep, 10);
			if (ep != percent && *ep == 0)
				addr->zone = zone;
			else
				return (LWRES_R_FAILURE);
		} else
			addr->zone = 0;
	} else
		return (LWRES_R_FAILURE); /* Unrecognised format. */

	return (LWRES_R_SUCCESS);
}

static lwres_result_t
lwres_conf_parsesortlist(lwres_conf_t *confdata,  FILE *fp) {
	int delim, res, idx;
	char word[LWRES_CONFMAXLINELEN];
	char *p;

	delim = getword(fp, word, sizeof(word));
	if (strlen(word) == 0U)
		return (LWRES_R_FAILURE); /* Empty line after keyword. */

	while (strlen(word) > 0U) {
		if (confdata->sortlistnxt == LWRES_CONFMAXSORTLIST)
			return (LWRES_R_FAILURE); /* Too many values. */

		p = strchr(word, '/');
		if (p != NULL)
			*p++ = '\0';

		idx = confdata->sortlistnxt;
		res = lwres_create_addr(word, &confdata->sortlist[idx].addr, 1);
		if (res != LWRES_R_SUCCESS)
			return (res);

		if (p != NULL) {
			res = lwres_create_addr(p,
						&confdata->sortlist[idx].mask,
						0);
			if (res != LWRES_R_SUCCESS)
				return (res);
		} else {
			/*
			 * Make up a mask.
			 */
			confdata->sortlist[idx].mask =
				confdata->sortlist[idx].addr;

			memset(&confdata->sortlist[idx].mask.address, 0xff,
			       confdata->sortlist[idx].addr.length);
		}

		confdata->sortlistnxt++;

		if (delim == EOF || delim == '\n')
			break;
		else
			delim = getword(fp, word, sizeof(word));
	}

	return (LWRES_R_SUCCESS);
}

static lwres_result_t
lwres_conf_parseoption(lwres_conf_t *confdata,  FILE *fp) {
	int delim;
	long ndots;
	char *p;
	char word[LWRES_CONFMAXLINELEN];

	delim = getword(fp, word, sizeof(word));
	if (strlen(word) == 0U)
		return (LWRES_R_FAILURE); /* Empty line after keyword. */

	while (strlen(word) > 0U) {
		if (strcmp("debug", word) == 0) {
			confdata->resdebug = 1;
		} else if (strcmp("no_tld_query", word) == 0) {
			confdata->no_tld_query = 1;
		} else if (strncmp("ndots:", word, 6) == 0) {
			ndots = strtol(word + 6, &p, 10);
			if (*p != '\0') /* Bad string. */
				return (LWRES_R_FAILURE);
			if (ndots < 0 || ndots > 0xff) /* Out of range. */
				return (LWRES_R_FAILURE);
			confdata->ndots = (uint8_t)ndots;
		}

		if (delim == EOF || delim == '\n')
			break;
		else
			delim = getword(fp, word, sizeof(word));
	}

	return (LWRES_R_SUCCESS);
}

/*% parses a file and fills in the data structure. */
lwres_result_t
lwres_conf_parse(lwres_conf_t *confdata, const char *filename) {
	FILE *fp = NULL;
	char word[256];
	lwres_result_t rval, ret;
	int stopchar;

	assert(filename != NULL);
	assert(strlen(filename) > 0U);
	assert(confdata != NULL);

	errno = 0;
	if ((fp = fopen(filename, "r")) == NULL)
		return (LWRES_R_NOTFOUND);

	ret = LWRES_R_SUCCESS;
	do {
		stopchar = getword(fp, word, sizeof(word));
		if (stopchar == EOF)
			break;

		if (strlen(word) == 0U)
			rval = LWRES_R_SUCCESS;
		else if (strcmp(word, "nameserver") == 0)
			rval = lwres_conf_parsenameserver(confdata, fp);
		else if (strcmp(word, "lwserver") == 0)
			rval = lwres_conf_parselwserver(confdata, fp);
		else if (strcmp(word, "domain") == 0)
			rval = lwres_conf_parsedomain(confdata, fp);
		else if (strcmp(word, "search") == 0)
			rval = lwres_conf_parsesearch(confdata, fp);
		else if (strcmp(word, "sortlist") == 0)
			rval = lwres_conf_parsesortlist(confdata, fp);
		else if (strcmp(word, "options") == 0)
			rval = lwres_conf_parseoption(confdata, fp);
		else {
			/* unrecognised word. Ignore entire line */
			rval = LWRES_R_SUCCESS;
			stopchar = eatline(fp);
			if (stopchar == EOF) {
				break;
			}
		}
		if (ret == LWRES_R_SUCCESS && rval != LWRES_R_SUCCESS)
			ret = rval;
	} while (1);

	fclose(fp);

	return (ret);
}
