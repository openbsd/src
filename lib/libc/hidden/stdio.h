/*	$OpenBSD: stdio.h,v 1.5 2015/09/19 04:02:21 guenther Exp $	*/
/*
 * Copyright (c) 2015 Philip Guenther <guenther@openbsd.org>
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

#ifndef	_LIBC_STDIO_H_
#define	_LIBC_STDIO_H_

/* we want the const-correct declarations inside libc */
#define __SYS_ERRLIST

#include_next <stdio.h>

__BEGIN_HIDDEN_DECLS
char	*_mktemp(char *);
__END_HIDDEN_DECLS

extern const int sys_nerr;
extern const char *const sys_errlist[];

#if 0
extern PROTO_NORMAL(sys_nerr);
extern PROTO_NORMAL(sys_errlist);
#endif

PROTO_NORMAL(__srget);
PROTO_NORMAL(__swbuf);
PROTO_NORMAL(asprintf);
PROTO_NORMAL(clearerr);
PROTO_NORMAL(ctermid);
PROTO_NORMAL(dprintf);
PROTO_NORMAL(fclose);
PROTO_NORMAL(fdopen);
PROTO_NORMAL(feof);
PROTO_NORMAL(ferror);
PROTO_NORMAL(fflush);
PROTO_NORMAL(fgetc);
PROTO_NORMAL(fgetln);
PROTO_NORMAL(fgetpos);
PROTO_NORMAL(fgets);
PROTO_NORMAL(fileno);
/*PROTO_NORMAL(flockfile);*/
PROTO_NORMAL(fmemopen);
PROTO_NORMAL(fopen);
PROTO_NORMAL(fprintf);
PROTO_NORMAL(fpurge);
PROTO_NORMAL(fputc);
PROTO_NORMAL(fputs);
PROTO_NORMAL(fread);
PROTO_NORMAL(freopen);
PROTO_NORMAL(fscanf);
PROTO_NORMAL(fseek);
PROTO_NORMAL(fseeko);
PROTO_NORMAL(fsetpos);
PROTO_NORMAL(ftell);
PROTO_NORMAL(ftello);
/*PROTO_NORMAL(ftrylockfile);*/
/*PROTO_NORMAL(funlockfile);*/
PROTO_NORMAL(funopen);
PROTO_NORMAL(fwrite);
PROTO_NORMAL(getc);
PROTO_NORMAL(getc_unlocked);
PROTO_NORMAL(getchar);
PROTO_NORMAL(getchar_unlocked);
PROTO_NORMAL(getdelim);
PROTO_NORMAL(getline);
PROTO_NORMAL(getw);
PROTO_NORMAL(open_memstream);
PROTO_NORMAL(pclose);
PROTO_NORMAL(perror);
PROTO_NORMAL(popen);
PROTO_NORMAL(printf);
PROTO_NORMAL(putc);
PROTO_NORMAL(putc_unlocked);
PROTO_NORMAL(putchar);
PROTO_NORMAL(putchar_unlocked);
PROTO_NORMAL(puts);
PROTO_NORMAL(putw);
PROTO_NORMAL(remove);
PROTO_NORMAL(rename);
PROTO_NORMAL(renameat);
PROTO_NORMAL(rewind);
PROTO_NORMAL(scanf);
PROTO_NORMAL(setbuf);
PROTO_NORMAL(setbuffer);
PROTO_NORMAL(setlinebuf);
PROTO_NORMAL(setvbuf);
PROTO_NORMAL(snprintf);
PROTO_STD_DEPRECATED(sprintf);
PROTO_NORMAL(sscanf);
PROTO_DEPRECATED(tempnam);
PROTO_NORMAL(tmpfile);
PROTO_STD_DEPRECATED(tmpnam);
PROTO_NORMAL(ungetc);
PROTO_NORMAL(vasprintf);
PROTO_NORMAL(vdprintf);
PROTO_NORMAL(vfprintf);
PROTO_NORMAL(vfscanf);
PROTO_NORMAL(vprintf);
PROTO_NORMAL(vscanf);
PROTO_NORMAL(vsnprintf);
PROTO_STD_DEPRECATED(vsprintf);
PROTO_NORMAL(vsscanf);

#endif /* _LIBC_STDIO_H_ */
