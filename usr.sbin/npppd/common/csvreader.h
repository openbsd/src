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
/* $Id: csvreader.h,v 1.1 2010/01/11 04:20:57 yasuoka Exp $ */

/** cvsreader のステータスを表す */
typedef enum _CSVREADER_STATUS {
	/** 正常に処理が完了した */
	CSVREADER_NO_ERROR		= 0,
	/**
	 * 処理中の列が存在する
	 * <p>
	 * csvreader_get_columns で取り出す前に、次の行をパースすると
	 * このエラーが発生します。</p>
	 */
	CSVREADER_HAS_PENDING_COLUMN	= 10001,
	/** メモリー割り当てに失敗した */
	CSVREADER_OUT_OF_MEMORY		= 10002,
	/** パースエラー */
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
