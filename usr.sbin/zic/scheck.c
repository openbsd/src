/*	$OpenBSD: scheck.c,v 1.3 2015/02/09 14:42:44 tedu Exp $ */
/*
** This file is in the public domain, so clarified as of
** 2006-07-17 by Arthur David Olson.
*/

#include <ctype.h>

#include "private.h"

const char *
scheck(string, format)
const char * const	string;
const char * const	format;
{
	char *		fbuf;
	const char *	fp;
	char *		tp;
	int		c;
	const char *	result;
	char			dummy;

	result = "";
	if (string == NULL || format == NULL)
		return result;
	fbuf = malloc(2 * strlen(format) + 4);
	if (fbuf == NULL)
		return result;
	fp = format;
	tp = fbuf;
	while ((*tp++ = c = *fp++) != '\0') {
		if (c != '%')
			continue;
		if (*fp == '%') {
			*tp++ = *fp++;
			continue;
		}
		*tp++ = '*';
		if (*fp == '*')
			++fp;
		while (isdigit((unsigned char)*fp))
			*tp++ = *fp++;
		if (*fp == 'l' || *fp == 'h')
			*tp++ = *fp++;
		else if (*fp == '[')
			do *tp++ = *fp++;
				while (*fp != '\0' && *fp != ']');
		if ((*tp++ = *fp++) == '\0')
			break;
	}
	*(tp - 1) = '%';
	*tp++ = 'c';
	*tp = '\0';
	if (sscanf(string, fbuf, &dummy) != 1)
		result = format;
	free(fbuf);
	return result;
}
