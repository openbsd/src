/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
 */
#include "includes.h"
#include "xmalloc.h"

#include <resolv.h>

int
uuencode(unsigned char *src, unsigned int srclength,
    char *target, size_t targsize)
{
	return b64_ntop(src, srclength, target, targsize);
}

int
uudecode(const char *src, unsigned char *target, size_t targsize)
{
	return b64_pton(src, target, targsize);
}

void
dump_base64(FILE *fp, unsigned char *data, int len)
{
	unsigned char *buf = xmalloc(2*len);
	int i, n;
	n = uuencode(data, len, buf, 2*len);
	for (i = 0; i < n; i++) {
		fprintf(fp, "%c", buf[i]);
		if (i % 70 == 69)
			fprintf(fp, "\n");
	}
	if (i % 70 != 69)
		fprintf(fp, "\n");
	xfree(buf);
}
