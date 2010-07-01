/* $OpenBSD: csvreader.h,v 1.2 2010/07/01 03:38:17 yasuoka Exp $ */
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
#ifndef CSVREADER_H
#define CSVREADER_H 1
/* $Id: csvreader.h,v 1.2 2010/07/01 03:38:17 yasuoka Exp $ */

/** status of cvsreader */
typedef enum _CSVREADER_STATUS {
	/** complete without any error */
	CSVREADER_NO_ERROR		= 0,
	/**
	 * A processing column exists.
	 * <p>
	 * This error occurs when parsing next line before getting a
	 * new coloumn by csvreader_get_columns().
	 * </p>
	 */
	CSVREADER_HAS_PENDING_COLUMN	= 10001,
	/** Failed in memory allocation */
	CSVREADER_OUT_OF_MEMORY		= 10002,
	/** Parse Error */
	CSVREADER_PARSE_ERROR		= 10003
} CSVREADER_STATUS;

typedef struct _csvreader csvreader;

#ifdef __cplusplus
extern "C" {
#endif

int               csvreader_toline (const char **, int, char *, int);
csvreader *       csvreader_create (void);
void              csvreader_destroy (csvreader *);
void              csvreader_reset (csvreader *);
const char **     csvreader_get_column (csvreader *);
int               csvreader_get_number_of_column (csvreader *);
CSVREADER_STATUS  csvreader_parse_flush (csvreader *);
CSVREADER_STATUS  csvreader_parse (csvreader *, const char *);

#ifdef __cplusplus
}
#endif

#endif
