/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME     :  nwstdio.h
 * DESCRIPTION  :  Making stdio calls go thro' the 
 *                 NetWare specific implementation.
 *                 This gets included if PERLIO_IS_STDIO. Instead
 *                 of directly calling stdio functions this goes 
 *                 thro' IPerlStdIO, this ensures that cgi2perl
 *                 can call CGI functions and send the o/p to
 *                 browser or console.
 * Author       :  SGP
 * Date	Created :  June 29th 2001.
 * Date Modified:  June 30th 2001.
 */

#ifndef ___NWStdio_H___
#define ___NWStdio_H___

#define PerlIO						FILE

#define PerlIO_putc(f,c)			(*PL_StdIO->pPutc)(PL_StdIO, (f),(c))
#define PerlIO_fileno(f)			(*PL_StdIO->pFileno)(PL_StdIO, (f))
#define PerlIO_close(f)				(*PL_StdIO->pClose)(PL_StdIO, (f))
#define PerlIO_stderr()				(*PL_StdIO->pStderr)(PL_StdIO)
#define PerlIO_printf				Perl_fprintf_nocontext
#define PerlIO_vprintf(f,fmt,a)		(*PL_StdIO->pVprintf)(PL_StdIO, (f),(fmt),a)
#define PerlIO_flush(f)				(*PL_StdIO->pFlush)(PL_StdIO, (f))
#define PerlIO_stdout()				(*PL_StdIO->pStdout)(PL_StdIO) 
#define PerlIO_stdin()				(*PL_StdIO->pStdin)(PL_StdIO)
#define PerlIO_clearerr(f)			(*PL_StdIO->pClearerr)(PL_StdIO, (f))
#define PerlIO_fdopen(f,s)			(*PL_StdIO->pFdopen)(PL_StdIO, (f),(s))
#define PerlIO_getc(f)				(*PL_StdIO->pGetc)(PL_StdIO, (f)) 
#define PerlIO_ungetc(f,c)			(*PL_StdIO->pUngetc)(PL_StdIO, (c),(f)) 
#define PerlIO_tell(f)				(*PL_StdIO->pTell)(PL_StdIO, (f)) 
#define PerlIO_seek(f,o,w)			(*PL_StdIO->pSeek)(PL_StdIO, (f),(o),(w))
#define PerlIO_error(f)				(*PL_StdIO->pError)(PL_StdIO, (f)) 
#define PerlIO_write(f,buf,size)	(*PL_StdIO->pWrite)(PL_StdIO, (buf), (size),1, (f))
#define PerlIO_puts(f,s)			(*PL_StdIO->pPuts)(PL_StdIO, (f),(s)) 
#define PerlIO_read(f,buf,size)		(*PL_StdIO->pRead)(PL_StdIO, (buf), (size), 1, (f))
#define PerlIO_eof(f)				(*PL_StdIO->pEof)(PL_StdIO, (f)) 
//#define PerlIO_fdupopen(f)			(*PL_StdIO->pFdupopen)(PL_StdIO, (f))
#define PerlIO_reopen(p,m,f)		(*PL_StdIO->pReopen)(PL_StdIO, (p), (m), (f))
#define PerlIO_open(x,y)			(*PL_StdIO->pOpen)(PL_StdIO, (x),(y))

#ifdef HAS_SETLINEBUF
#define PerlIO_setlinebuf(f)		(*PL_StdIO->pSetlinebuf)(PL_StdIO, (f))
#else
#define PerlIO_setlinebuf(f)		setvbuf(f, NULL, _IOLBF, 0)
#endif

#define PerlIO_isutf8(f)		0

#ifdef USE_STDIO_PTR
#define PerlIO_has_cntptr(f)		1
#define PerlIO_get_ptr(f)		FILE_ptr(f)
#define PerlIO_get_cnt(f)		FILE_cnt(f)

#ifdef STDIO_CNT_LVALUE
#define PerlIO_canset_cnt(f)		1
#define PerlIO_set_cnt(f,c)		(FILE_cnt(f) = (c))
#ifdef STDIO_PTR_LVALUE
#ifdef STDIO_PTR_LVAL_NOCHANGE_CNT
#define PerlIO_fast_gets(f)		1
#endif
#endif /* STDIO_PTR_LVALUE */
#else /* STDIO_CNT_LVALUE */
#define PerlIO_canset_cnt(f)		0
#define PerlIO_set_cnt(f,c)		abort()
#endif

#ifdef STDIO_PTR_LVALUE
#ifdef STDIO_PTR_LVAL_NOCHANGE_CNT
#define PerlIO_set_ptrcnt(f,p,c)      STMT_START {FILE_ptr(f) = (p), PerlIO_set_cnt(f,c);} STMT_END
#else
#ifdef STDIO_PTR_LVAL_SETS_CNT
/* assert() may pre-process to ""; potential syntax error (FILE_ptr(), ) */
#define PerlIO_set_ptrcnt(f,p,c)      STMT_START {FILE_ptr(f) = (p); assert(FILE_cnt(f) == (c));} STMT_END
#define PerlIO_fast_gets(f)		1
#else
#define PerlIO_set_ptrcnt(f,p,c)	abort()
#endif
#endif
#endif

#else  /* USE_STDIO_PTR */

#define PerlIO_has_cntptr(f)		0
#define PerlIO_canset_cnt(f)		0
#define PerlIO_get_cnt(f)		(abort(),0)
#define PerlIO_get_ptr(f)		(abort(),(void *)0)
#define PerlIO_set_cnt(f,c)		abort()
#define PerlIO_set_ptrcnt(f,p,c)	abort()

#endif /* USE_STDIO_PTR */

#ifndef PerlIO_fast_gets
#define PerlIO_fast_gets(f)		0
#endif

#ifdef FILE_base
#define PerlIO_has_base(f)		1
#define PerlIO_get_bufsiz(f)		(*PL_StdIO->pGetBufsiz)(PL_StdIO, (f))
#define PerlIO_get_base(f)			(*PL_StdIO->pGetBase)(PL_StdIO, (f)) 
#else
#define PerlIO_has_base(f)		0
#define PerlIO_get_base(f)		(abort(),(void *)0)
#define PerlIO_get_bufsiz(f)		(abort(),0)
#endif

#define PerlIO_importFILE(f,fl)		(f)
#define PerlIO_exportFILE(f,fl)		(f)
#define PerlIO_findFILE(f)		(f)
#define PerlIO_releaseFILE(p,f)		((void) 0)

#endif /* ___NWStdio_H___ */
