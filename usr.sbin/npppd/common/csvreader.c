/* $OpenBSD: csvreader.c,v 1.2 2010/07/01 03:38:17 yasuoka Exp $ */
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
/* The original version is CSVReader.java */
/* @file
 * Subroutines to read CSV(RFC4180)
 * <pre>
 *  csvreader *csv;
 *  const char **cols;
 *  char buf[1024];
 *
 *  csv = csvreader_create();
 *  while (fgets(buf, sizeof(buf), stdin) != NULL) {
 *	if (csvreader_parse(csv, buf) != CSVREADER_NO_ERROR) {
 *	    // error handling
 *	    break;
 *	}
 *	cols = csv_reader_get_column(csv)
 *	if (cols == NULL)
 *	    continue;
 *	// your code here.  col[0] is the first column.
 *   }
 *   if (csvreader_parse(csv, buf) == CSVREADER_NO_ERROR) {
 *	cols = csv_reader_get_column(csv)
 *	if (cols != NULL) {
 *	    // your code here.  col[0] is the first column.
 *	}
 *   } else {
 *	    // error handling
 *   }
 *   csvreader_destroy(csv);
 *</pre>
 */
/* $Id: csvreader.c,v 1.2 2010/07/01 03:38:17 yasuoka Exp $ */
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "csvreader.h"

struct _csvreader {
	char 	*buffer;
	const char **cols;
	int	cap;
	int	pos;
	int	start_pos;
	int	col_cap;
	int	col_pos;
	int	state;
	int	column_start_with_quote:1;
};

#define CSV_BUFSIZ		256
#define CSV_COLSIZ		32

#define CSV_INIT		0
#define CSV_IN_DATA		1
#define CSV_WAIT_DELIM		2
#define CSV_HAS_DQUOTE		3
#define CSV_FLUSH_WAIT		4

#define DQUOTE		'"'
#define COMMA		','
#define	CR		'\r'
#define	LF		'\n'

static int   csvreader_buffer_append (csvreader *, const char *, int);
static int   csvreader_flush_column (csvreader *);
static CSVREADER_STATUS  csvreader_parse_flush0 (csvreader *, int);

/**
 * Make a cvsreader context and returns it. Return null if malloc() failed.
 */
csvreader *
csvreader_create(void)
{
	csvreader *_this;

	if ((_this = malloc(sizeof(csvreader))) == NULL)
		return NULL;
	memset(_this, 0, sizeof(*_this));
	_this->state = CSV_INIT;

	return _this;
}


/**
 * Free a cvsreader context.
 */
void
csvreader_destroy(csvreader *_this)
{
	if (_this->buffer != NULL)
		free(_this->buffer);
	if (_this->cols != NULL)
		free(_this->cols);
	free(_this);
}

/**
 * get the number of parsed columns.
 */
int
csvreader_get_number_of_column(csvreader *_this)
{
	if (_this->state != CSV_FLUSH_WAIT)
		return 0;
	return _this->col_pos;
}

/**
 * get a parsed column.
 */
const char **
csvreader_get_column(csvreader *_this)
{
	if (_this->state != CSV_FLUSH_WAIT)
		return NULL;

	_this->col_pos = 0;
	_this->pos = 0;
	_this->start_pos = 0;
	_this->state = CSV_INIT;

	return _this->cols;
}

/**
 * Reset a cvsreader context.
 */
void
csvreader_reset(csvreader *_this)
{
	_this->cap = 0;
	_this->pos = 0;
	_this->start_pos = 0;
	_this->col_cap = 0;
	_this->col_pos = 0;
	_this->state = 0;
	_this->column_start_with_quote = 0;
}

/**
 * Finish parsing of a column on the way.
 * <p>
 * Call this function when it's sure that there is no next line or the end
 * of the CSV.
 * It will return error when the parsing of the field isn't finished. </p>
 */
CSVREADER_STATUS 
csvreader_parse_flush(csvreader *_this)
{
	return csvreader_parse_flush0(_this, 1);
}

/**
 * parse a line.
 *
 * @param	a csvreader context
 * @param	a line to parse
 */
CSVREADER_STATUS
csvreader_parse(csvreader *_this, const char *line)
{
	int off, lline, append;

	lline = strlen(line);

	if (_this->state == CSV_FLUSH_WAIT)
		return CSVREADER_HAS_PENDING_COLUMN;

	if (csvreader_buffer_append(_this, line, lline) != 0)
		return CSVREADER_OUT_OF_MEMORY;

	for (off = 0; off < lline; off++) {
		append = 1;
		switch (_this->state) {
		case CSV_INIT:
			_this->state = CSV_IN_DATA;
			if (line[off] == DQUOTE) {
				_this->column_start_with_quote = 1; 
				break;
			}
			/* FALLTHROUGH */
		case CSV_IN_DATA:
			if (_this->column_start_with_quote != 0) {
				if (line[off] == DQUOTE)
					_this->state = CSV_HAS_DQUOTE;
				break;
			}
			if (line[off] == COMMA) {
				append = 0;
				csvreader_flush_column(_this);
			}
			if (_this->column_start_with_quote == 0 &&
			    (line[off] == CR || line[off] == LF))
				goto eol;
			break;
		case CSV_HAS_DQUOTE:
			if (line[off] == DQUOTE) {
				_this->state = CSV_IN_DATA;
				append = 0;
				break;
			}
                	_this->state = CSV_WAIT_DELIM;
			/* FALLTHROUGH */
		case CSV_WAIT_DELIM:
			if (line[off] == CR || line[off] == LF)
				goto eol;
			append = 0;
			if (line[off] != COMMA)
				return CSVREADER_PARSE_ERROR;
			csvreader_flush_column(_this);
			break;
		}
		if (append)
			_this->buffer[_this->pos++] = line[off];
	}
eol:

	return csvreader_parse_flush0(_this, 0);
}

static CSVREADER_STATUS 
csvreader_parse_flush0(csvreader *_this, int is_final)
{
	if (_this->state == CSV_FLUSH_WAIT)
		return CSVREADER_NO_ERROR;
	switch (_this->state) {
	case CSV_IN_DATA:
		if (_this->column_start_with_quote != 0) {
			if (is_final)
				return CSVREADER_PARSE_ERROR;
			/* wait next line */
			return CSVREADER_NO_ERROR;
		}
		/* FALLTHROUGH */
	case CSV_INIT:
		if (is_final && _this->col_pos == 0)
			return CSVREADER_NO_ERROR;
		/* FALLTHROUGH */
        case CSV_HAS_DQUOTE:
        case CSV_WAIT_DELIM:
		csvreader_flush_column(_this);
		_this->state = CSV_FLUSH_WAIT;
		return CSVREADER_NO_ERROR;
	}
	return CSVREADER_PARSE_ERROR;
}

/**
 * Convert columns stored in char *[] to a CVS line string.
 *
 * @param	cols	columns to be converted. NULL means the end of the
 * 			column.
 * @param	ncols	number of columns
 * @param	buffer	the output buffer to write a converted line.
 * @param	lbuffer	the size of the output buffer.
 */
int
csvreader_toline(const char **cols, int ncols, char *buffer, int lbuffer)
{
	int i, j, off;

	off = 0;
#define	checksize()	if (off + 1 > lbuffer) { goto enobufs; }
	for (i = 0; i < ncols && cols[i] != NULL; i++) {
		if (i != 0) {
			checksize();
			buffer[off++] = ',';
		}
		for (j = 0; cols[i][j] != '\0'; j++) {
			if (j == 0) {
				checksize();
				buffer[off++] = '"';
			}
			if (cols[i][j] == '"') {
				checksize();
				buffer[off++] = '"';
			}
			checksize();
			buffer[off++] = cols[i][j];
		}
		checksize();
		buffer[off++] = '"';
	}
	checksize();
	buffer[off++] = '\0';

	return 0;
enobufs:
	return 1;
}

static int
csvreader_buffer_append(csvreader *_this, const char *buf, int lbuf)
{
	int ncap;
	char *nbuffer;

	if (_this->pos + lbuf > _this->cap) {
		ncap = _this->cap + lbuf;
		if ((ncap % CSV_BUFSIZ) != 0)
			ncap += CSV_BUFSIZ - (ncap % CSV_BUFSIZ);
		if ((nbuffer = realloc(_this->buffer, ncap)) == NULL)
			return 1;
		_this->cap = ncap;
		_this->buffer = nbuffer;
	}

	return 0;
}

static int
csvreader_flush_column(csvreader *_this)
{
	int ncap;
	const char **ncols;

	if (_this->col_pos + 1 >= _this->col_cap) {
		ncap = _this->col_cap + CSV_COLSIZ;
		if ((ncols = realloc(_this->cols, ncap * sizeof(char *)))
		    == NULL)
			return CSVREADER_OUT_OF_MEMORY;
		_this->col_cap = ncap;
		_this->cols = ncols;
	}

	if (_this->column_start_with_quote != 0) {
		 _this->start_pos++;
		_this->buffer[_this->pos - 1] = '\0';
	} else {
		_this->buffer[_this->pos++] = '\0';
	}

	_this->cols[_this->col_pos++] = _this->buffer + _this->start_pos;
	_this->cols[_this->col_pos] = NULL;
	_this->start_pos = _this->pos;
	_this->column_start_with_quote = 0; 
	_this->state = CSV_INIT;

	return CSVREADER_NO_ERROR;
}
