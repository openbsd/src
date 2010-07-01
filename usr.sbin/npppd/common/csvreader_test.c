/* $OpenBSD: csvreader_test.c,v 1.2 2010/07/01 03:38:17 yasuoka Exp $ */
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
 * cc -o csvreader_test csvreader.c csvreader_test.c
 */
/* $Id: csvreader_test.c,v 1.2 2010/07/01 03:38:17 yasuoka Exp $ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csvreader.h"

#define	ASSERT(x)	\
	if (!(x)) { \
	    fprintf(stderr, \
		"\nASSERT(%s) failed on %s() at %s:%d.\n" \
		, #x, __func__, __FILE__, __LINE__); \
	    abort(); \
	}
#define countof(x)	(sizeof((x)) / sizeof((x)[0]))
#define	test(x)					\
    {						\
	fprintf(stderr, "%-10s ... ", #x);	\
	fflush(stderr);				\
	x();					\
	fprintf(stderr, "ok\n");		\
    }


static void
test01(void)
{
	int i;
	CSVREADER_STATUS status;
	const char ** column;
	csvreader *csv;
	char *csv_data[] = {
	    "hogehoge,fugafuga\n",
	    "hogehoge,fugafuga\n",
	    "hogehoge,fugafuga\n"
	};

	csv = csvreader_create();
	ASSERT(csv != NULL);

	for (i = 0; i < countof(csv_data); i++) {
		status = csvreader_parse(csv, csv_data[i]);
		ASSERT(status == CSVREADER_NO_ERROR);
		column = csvreader_get_column(csv);
		ASSERT(column != NULL);
		ASSERT(strcmp(column[0], "hogehoge") == 0);
		ASSERT(strcmp(column[1], "fugafuga") == 0);
		ASSERT(column[2] == NULL);
	}
	csvreader_parse_flush(csv);
	column = csvreader_get_column(csv);
	ASSERT(column == NULL);

	csvreader_destroy(csv);
}

static void
test02(void)
{
	CSVREADER_STATUS status;
	const char ** column;
	csvreader *csv;
	char *csv_data[] = {
	    "\"hogehoge\",\"fugafuga\"\n",
	    "hogehoge,\"fugafuga\"\n",
	    "\"hogehoge\",fugafuga\n",
	    "\"hogehoge\",fuga\"fuga\n",
	    "\"hogehoge\",fuga\nfuga\n",
	    "\"hogehoge\",\"fuga\nfuga\"\n",
	    "\"hogehoge\",\"fuga\rfuga\"\n",
	    "\",fugafuga\n",
	    "hogehoge\",\n",
	    "\"hogehoge\n",
	    "hogehoge,fugafuga",
	};

	csv = csvreader_create();
	ASSERT(csv != NULL);

	status = csvreader_parse(csv, csv_data[0]);
	ASSERT(status == CSVREADER_NO_ERROR);
	column = csvreader_get_column(csv);
	ASSERT(column != NULL);
	ASSERT(strcmp(column[0], "hogehoge") == 0);
	ASSERT(strcmp(column[1], "fugafuga") == 0);
	ASSERT(column[2] == NULL);

	status = csvreader_parse(csv, csv_data[1]);
	ASSERT(status == CSVREADER_NO_ERROR);
	column = csvreader_get_column(csv);
	ASSERT(column != NULL);
	ASSERT(strcmp(column[0], "hogehoge") == 0);
	ASSERT(strcmp(column[1], "fugafuga") == 0);
	ASSERT(column[2] == NULL);

	status = csvreader_parse(csv, csv_data[2]);
	ASSERT(status == CSVREADER_NO_ERROR);
	column = csvreader_get_column(csv);
	ASSERT(column != NULL);
	ASSERT(strcmp(column[0], "hogehoge") == 0);
	ASSERT(strcmp(column[1], "fugafuga") == 0);
	ASSERT(column[2] == NULL);

	status = csvreader_parse(csv, csv_data[3]);
	ASSERT(status == CSVREADER_NO_ERROR);
	column = csvreader_get_column(csv);
	ASSERT(column != NULL);
	ASSERT(strcmp(column[0], "hogehoge") == 0);
	ASSERT(strcmp(column[1], "fuga\"fuga") == 0);
	ASSERT(column[2] == NULL);

	status = csvreader_parse(csv, csv_data[4]);
	ASSERT(status == CSVREADER_NO_ERROR);
	column = csvreader_get_column(csv);
	ASSERT(column != NULL);
	ASSERT(strcmp(column[0], "hogehoge") == 0);
	ASSERT(strcmp(column[1], "fuga") == 0);
	ASSERT(column[2] == NULL);

	status = csvreader_parse(csv, csv_data[5]);
	ASSERT(status == CSVREADER_NO_ERROR);
	column = csvreader_get_column(csv);
	ASSERT(column != NULL);
	ASSERT(strcmp(column[0], "hogehoge") == 0);
	/* printf("**%s**\n", column[1]); */
	ASSERT(strcmp(column[1], "fuga\nfuga") == 0);
	ASSERT(column[2] == NULL);

	status = csvreader_parse(csv, csv_data[6]);
	ASSERT(status == CSVREADER_NO_ERROR);
	column = csvreader_get_column(csv);
	ASSERT(column != NULL);
	ASSERT(strcmp(column[0], "hogehoge") == 0);
	/* printf("**%s**\n", column[1]); */
	ASSERT(strcmp(column[1], "fuga\rfuga") == 0);
	ASSERT(column[2] == NULL);

	status = csvreader_parse(csv, csv_data[7]);
	ASSERT(status == CSVREADER_NO_ERROR);
	column = csvreader_get_column(csv);
	ASSERT(column == NULL);
	status = csvreader_parse(csv, csv_data[8]);
	ASSERT(status == CSVREADER_NO_ERROR);
	column = csvreader_get_column(csv);
	ASSERT(column != NULL);
	ASSERT(strcmp(column[0], ",fugafuga\nhogehoge") == 0);

	csvreader_parse_flush(csv);
	column = csvreader_get_column(csv);
	ASSERT(column == NULL);

	status = csvreader_parse(csv, csv_data[9]);
	ASSERT(status == CSVREADER_NO_ERROR);
	column = csvreader_get_column(csv);
	ASSERT(column == NULL);

	status = csvreader_parse_flush(csv);
	ASSERT(status == CSVREADER_PARSE_ERROR);
	column = csvreader_get_column(csv);
	ASSERT(column == NULL);

	csvreader_reset(csv);

	status = csvreader_parse(csv, csv_data[10]);
	ASSERT(status == CSVREADER_NO_ERROR);
	column = csvreader_get_column(csv);
	ASSERT(column != NULL);
	ASSERT(strcmp(column[0], "hogehoge") == 0);
	ASSERT(strcmp(column[1], "fugafuga") == 0);

	csvreader_destroy(csv);
}

static void
test03(void)
{
	int i;
	CSVREADER_STATUS status;
	const char ** column;
	csvreader *csv;
	char *csv_data[] = {
	    "yasuoka,hogehoge,\"10.0.0.1\n",
	    "\n"
	};

	csv = csvreader_create();
	ASSERT(csv != NULL);

	status = csvreader_parse(csv, csv_data[0]);
	ASSERT(status == CSVREADER_NO_ERROR);

	status = csvreader_parse_flush(csv);
	ASSERT(status == CSVREADER_PARSE_ERROR);
	column = csvreader_get_column(csv);
	ASSERT(column == NULL);

	csvreader_destroy(csv);
}
static void
test04(void)
{
	int rval;
	char line[8];
	const char *test[5];

	memset(line, 0x55, sizeof(line));
	test[0] = "xxxxx";
	test[1] = NULL;
	rval = csvreader_toline(test, 5, line, sizeof(line));
	ASSERT(rval == 0);
	ASSERT(strcmp(line, "\"xxxxx\"") == 0);

	memset(line, 0x55, sizeof(line));
	test[0] = "xxxxxx";
	test[1] = NULL;
	rval = csvreader_toline(test, 5, line, sizeof(line));
	ASSERT(rval != 0);

	memset(line, 0x55, sizeof(line));
	test[0] = "xx";	/* 5 */
	test[1] = "x";	/* 4 */
	test[2] = NULL;
	rval = csvreader_toline(test, 5, line, sizeof(line));
	ASSERT(rval != 0);

	memset(line, 0x55, sizeof(line));
	test[0] = "x";	/* 5 */
	test[1] = "x";	/* 4 */
	test[2] = NULL;
	rval = csvreader_toline(test, 5, line, sizeof(line));
	ASSERT(rval == 0);
	ASSERT(strcmp(line, "\"x\",\"x\"") == 0);
}

int
main(int argc, char *argv[])
{
	test(test01);
	test(test02);
	test(test03);
	test(test04);

	return 0;
}

