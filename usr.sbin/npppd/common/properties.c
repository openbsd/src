/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * $Id: properties.c,v 1.3 2010/07/02 21:20:57 yasuoka Exp $
 */
/* LINTLIBRARY */
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "hash.h"
#include "properties.h"
#include "debugmacro.h"

struct properties {
	hash_table *hash_tbl;
};

#ifdef _WIN32
#define	snprintf	_snprintf
#define u_int32_t	unsigned long
#endif

#ifndef MAX
#define MAX(m,n)	(((m) > (n))? (m) : (n))
#endif
#ifndef MIN
#define MIN(m,n)	(((m) < (n))? (m) : (n))
#endif

static int           continue_line __P((const char *));
static char          *convert_in_save __P((const char *));
static void          convert_in_load __P((char *));
static uint32_t      str_hash __P((const void *, int));
static const char *  properties_put0(struct properties *, const char *, const char *, int);

static void          chomp __P((char *));
static char *        trim __P((char *));
static const char *  skip_space __P((const char *));

/**
 * Create properties object
 *
 * @param   sz	size of hash (sould be prime number)
 * @return  pointer to created object
 */
struct properties *
properties_create(int sz)
{
	struct properties *_this;

	if ((_this = (struct properties *)malloc(sizeof(struct properties)))
	    == NULL)
		return NULL;
	memset(_this, 0, sizeof(struct properties));

	if ((_this->hash_tbl = hash_create(
	    (int (*)(const void *, const void *))strcmp,
	    str_hash, sz)) == NULL) {
		free(_this);
		return NULL;
	}

	return _this;
}

/**
 * get property value by key
 *
 * @param   _this   pointer to properties object
 * @param   key	    property key
 * @returns property value
 */
const char *
properties_get(struct properties *_this, const char *key)
{
	hash_link *hl;

	if ((hl = hash_lookup(_this->hash_tbl, key)) != NULL)
		return hl->item;
	return NULL;
}

/**
 * remove property entry.
 *
 * @param   _this   pointer to properties object
 * @param   key	    property key
 */
void
properties_remove(struct properties *_this, const char *key)
{
	char *key0;
	hash_link *hl;

	if ((hl = hash_lookup(_this->hash_tbl, key)) == NULL)
		return;

	key0 = /* NOSTRICT */(char *)hl->key;

	hash_delete(_this->hash_tbl, key, 1);
	free(key0);
}

/**
 * remove all items from the properties.
 *
 * @param   _this   pointer to properties object
 */
void
properties_remove_all(struct properties *_this)
{
	char *key0;
	hash_link *hl;

	for (hl = hash_first(_this->hash_tbl); hl != NULL;
	    hl = hash_next(_this->hash_tbl)) {
		key0 = /* NOSTRICT */(char *)hl->key;
		hash_delete(_this->hash_tbl, hl->key, 1);
		free(key0);
	}
}

/**
 * put all items of 'props' properties to the properties.
 *
 * @param   _this   pointer to properties object
 */
void
properties_put_all(struct properties *_this, struct properties *props)
{
	hash_link *hl;

	for (hl = hash_first(props->hash_tbl); hl != NULL;
	    hl = hash_next(props->hash_tbl)) {
		properties_put(_this, hl->key, hl->item);
	}
}

/**
 * put(add) property entry.
 *
 * @param   _this   pointer to properties object
 * @param   key	    property key
 * @param   value   property value
 * @return  pointer to property value that is stored in hash table.
 */
const char *
properties_put(struct properties *_this, const char *key, const char *value)
{
	return properties_put0(_this, key, value, 1);
}

static const char *
properties_put0(struct properties *_this, const char *key, const char *value,
    int do_trim)
{
	char *key0 = NULL, *value0 = NULL;
	char *key1 = NULL, *value1 = NULL;

	if (key[0] == '\0')
		return NULL;

	if ((key0 = strdup(key)) == NULL)
		goto exception;

	key1 = key0;
	if (do_trim)
		key1 = trim(key1);
	if (key1[0] == '\0')
		goto exception;

	if ((value0 = strdup(value)) == NULL)
		goto exception;
	value1 = value0;
	if (do_trim) {
		value1 = trim(value1);
		if (value1 != value0) {
			/* value1 must point the beginning of the buffer */
			if ((value1 = strdup(value1)) == NULL)
				goto exception;
			free(value0);
			value0 = value1;
		}
	}
	properties_remove(_this, key1);
	if (hash_insert(_this->hash_tbl, key1, value1) != 0)
		goto exception;

	return value;
exception:
	if (key0 != NULL)
		free(key0);
	if (value0 != NULL)
		free(value0);
	return NULL;
}

/**
 * Destroy properties object.
 * @param   _this   pointer to properties object
 */
void
properties_destroy(struct properties *_this)
{
	if (_this->hash_tbl != NULL) {
		properties_remove_all(_this);
		hash_free(_this->hash_tbl);
		_this->hash_tbl = NULL;
	}
	free(_this);
}

/**
 * get first key value of properties.
 *
 * @param   _this   pointer to properties object
 * @return  the first key value of properties.
 * @see     #properties_next_key
 */
const char *
properties_first_key(struct properties *_this)
{
	hash_link *hl;

	hl = hash_first(_this->hash_tbl);
	if (hl != NULL)
		return hl->key;
	return NULL;
}

/**
 * get next key value of properties.
 *
 * @param   _this   pointer to properties object
 * @return  the next key value of properties.
 * @see	    properties_first_key
 */
const char *
properties_next_key(struct properties *_this)
{
	hash_link *hl;

	hl = hash_next(_this->hash_tbl);
	if (hl != NULL)
		return hl->key;
	return NULL;
}

/**
 * Store this object to a file
 *
 * @param   _this  pointer to properties object
 * @param   fp	   FILE stream to store.
 * @return  returns 0 in succeed.
 */
int
properties_save(struct properties *_this, FILE *fp)
{
	char *value;
	const char *key;

	for (key = properties_first_key(_this); key != NULL;
	    key = properties_next_key(_this)) {
		if ((value = convert_in_save(properties_get(_this, key)))
		    == NULL)
			return -1;

		fprintf(fp, "%s: %s\n", key, value);
		free(value);
	}
	return 0;
}

static int
continue_line(const char *line)
{
	int eol;
	int slash_cnt = 0;

	eol = strlen(line);

	while (--eol > 0) {
		if (*(line + eol) == '\\')
			slash_cnt++;
		else
			break;
	}
	if (slash_cnt % 2 == 1)
		return 1;

	return 0;
}

static char *
convert_in_save(const char *value)
{
	size_t outsz = 128;
	int i, j;
	char *out, *out0;

	if ((out = (char *)malloc(outsz)) == NULL)
		return NULL;

	for (i = 0, j = 0; value[i] != '\0'; i++) {
		if (j + 2 > outsz) {
			if ((out0 = (char *)realloc(out, outsz * 2)) == NULL) {
				free(out);
				return NULL;
			}
			out = out0;
			outsz *= 2;
		}
		switch (value[i]) {
		case '\n':  out[j++] = '\\'; out[j++] = 'n'; break;
		case '\r':  out[j++] = '\\'; out[j++] = 'r'; break;
		case '\t':  out[j++] = '\\'; out[j++] = 't'; break;
		case '\\':  out[j++] = '\\'; out[j++] = '\\'; break;
		default:    out[j++] = value[i]; break;
		}
	}
	out[j] = '\0';

	return out;
}

static void
convert_in_load(char *value)
{
	int i, j;

	for (i = 0, j = 0; value[i] != '\0'; i++, j++) {
		if (value[i] == '\\') {
			switch (value[++i]) {
			case '\\':  value[j] = '\\'; continue;
			case 'r':   value[j] = '\r'; continue;
			case 'n':   value[j] = '\n'; continue;
			case 't':   value[j] = '\t'; continue;
			default:    break;
			}
		}
		value[j] = value[i];
	}
	value[j] = '\0';
}

/*
 * Load properties from the given file pointer(FILE) using the given charset
 * decoder.  We use EUC-JP encoding internally.
 *
 * @param   _this   pointer to the properties object.
 * @param   fp	    FILE stream to load.
 * @return  return 0 in succeed.
 */
int
properties_load(struct properties *_this, FILE *fp)
{
	char *key, *value, *delim;
	char buf0[BUFSIZ];
	size_t linesz = 128, linelen;

	int lineoff = 0, hasnl, linecont;
	char *line = NULL, *line0, *line1;

	if ((line = (char *)malloc(linesz)) == NULL)
		goto fail;

	linecont = 0;
	while (fgets(buf0, sizeof(buf0), fp) != NULL) {
	    	hasnl = 0;
		linelen = strlen(buf0);
		if (linelen > 0 && buf0[linelen - 1] == '\n') {
			hasnl = 1;
			chomp(buf0);
		}
		if (linecont || (lineoff == 0))
			line0 = (char *)skip_space(buf0);
		else
			line0 = buf0;
		linelen = strlen(line0);
		while (lineoff + linelen + 128 > linesz) {
			if ((line1 = realloc(line, linesz * 2))
			    == NULL)
				goto fail;
			line = line1;
			linesz *= 2;
		}
		memcpy(line + lineoff, line0, linelen);
		line[lineoff + linelen] = '\0';
		lineoff += linelen;

		linecont = 0;
		if (!hasnl)
			continue;

		if (continue_line(line0)) {
			lineoff--;	/* delete \(backslash) */
			linecont = 1;
			continue;
		}
		lineoff = 0;
		if (*line == '#')
			continue;
		key = line;

		for (delim = key; *delim != '\0'; delim++) {
			if (*delim == '=' || *delim == ':')
				break;
		}
		if (*delim == '\0')
			continue;

		*delim = '\0';
		value = trim(delim + 1);
		key = trim(key);

		convert_in_load(value);

		properties_put0(_this, key, value, 0);

		lineoff = 0;
	}
	if (line != NULL)
		free(line);
	return 0;
fail:
	if (line != NULL)
		free(line);

	return -1;
}

static uint32_t
str_hash(const void *ptr, int sz)
{
	u_int32_t hash = 0;
	int i, len;
	const char *str;

	str = ptr;
	len = strlen(str);
	for (i = 0; i < len; i++)
		hash = hash*0x1F + str[i];
	hash = (hash << 16) ^ (hash & 0xffff);

	return hash % sz;
}

#define	ISCRLF(x)	((x) == '\r' || (x) == '\n')
#define	ISSPACE(x)	((x) == ' ' || (x) == '\t' || (x) == '\r' || \
			(x) == '\n')
static const char *
skip_space(s)
	const char *s;
{
	const char *r;

	for (r = s; *r != '\0' && ISSPACE(*r); r++)
		;; /* skip */
	return r;
}

static char *
trim(s)
	char *s;
{
	char *r;
	char *t;

	r = /* NOSTRICT */(char *)skip_space(s);
	for (t = r + strlen(r) - 1; r <= t; t--) {
		if (ISSPACE(*t))
			*t = '\0';
		else
			break;
	}
	return r;
}

static void
chomp(s)
	char *s;
{
	char *t;

	for (t = s + strlen(s) - 1; s <= t; t--) {
		if (ISCRLF(*t))
			*t = '\0';
		else
			break;
	}
}

#if PROPGET_CMD
#include <stdio.h>

static void usage __P((void));

static void usage(void)
{
	fprintf(stderr, "usage: propgetcmd prop_file prop_key [prop_key ..]\n");
}

int
main(int argc, char *argv[])
{
	const char *k;
	struct properties *prop;
	FILE *fp;

	argc--;
	argv++;

	if (argc < 2) {
		usage();
		return 1;
	}

	if ((fp = fopen(*argv, "r")) == NULL) {
		perror(*argv);
		return 1;
	}
	argc--;
	argv++;

	if ((prop = properties_create(127)) == NULL) {
		perror("properties_create() failed");
		return 1;
	}
	properties_load(prop, fp);

	while (argc--) {
		if (properties_get(prop, *argv) != NULL)
			printf("%s\n", properties_get(prop, *argv));

	}
	fclose(fp);
	properties_destroy(prop);
}
#endif
