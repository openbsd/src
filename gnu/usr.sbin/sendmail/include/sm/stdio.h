/*
 * Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
 *      All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Sendmail: stdio.h,v 1.16 2001/03/08 03:23:08 ca Exp $
 */

#ifndef SM_STDIO_H
#define SM_STDIO_H

#include <sm/gen.h>
#include <sm/io.h>

/*
**  We include <stdio.h> here for several reasons:
**  - To force the <stdio.h> idempotency macro to be defined so that
**    any future includes of <stdio.h> will have no effect;
**  - To declare functions like rename() which we do not and can not override.
**  Note that all of the following redefinitions of standard stdio
**  apis are macros.
*/

#include <stdio.h>
#undef FILE
#undef _IOFBF
#undef _IOLBF
#undef EOF
#undef BUFSIZ
#undef getc
#undef putc

/*
** Temporary for transition from stdio to sm_io.
*/

#define FILE SM_FILE_T
#define _IOFBF SM_IO_FBF
#define _IOLBF SM_IO_LBF
#define _SMIONBF SM_IO_NBF
#define EOF SM_IO_EOF
#define BUFSIZ SM_IO_BUFSIZ
#define fpos_t off_t

#undef stdin
#undef stdout
#undef stderr
#undef clearerr
#undef feof
#undef ferror
#undef getc_unlocked
#undef getchar
#undef putc_unlocked
#undef putchar
#undef fileno

#define stdin	smioin
#define stdout	smioout
#define stderr	smioerr

#define clearerr(f)	sm_io_clearerr(f)
#define fclose(f)	sm_io_close(f)
#define feof(f)	sm_io_eof(f)
#define ferror(f)	sm_io_error(f)
#define fflush(f)	sm_io_flush(f)
#define fgetc(f)	sm_io_fgetc(f)
#define fgetln(f, x)	sm_io_getln(f, x)
#define fgetpos(f, p)	sm_io_getpos(f, p)
#define fgets(b, n, f)	sm_io_fgets(f, b, n)
#define fpurge(f)	sm_io_purge(f)
#define fputc(c, f)	sm_io_fputc(f, c)
#define fread(b, s, c, f)	sm_io_read(f, b, s, c)
#define fseek(f, o, w)	sm_io_seek(f, o, w)
#define fsetpos(f, p)	sm_io_setpos(f, p)
#define ftell(f)	sm_io_tell(f)
#define fwrite(b, s, c, f)	sm_io_write(f, b, s, c)
#define getc(f)	sm_io_getc(f)
#define getc_unlocked(f)	sm_io_getc_unlocked(f)
#define getchar()	sm_io_getc(smioout)
#define putc(c, f)	sm_io_putc(f, c)
#define putc_unlocked(c, f)	sm_io_putc_unlocked(f, c)
#define putchar(c)	sm_io_putc(smioout, c)
#define rewind(f)	sm_io_rewind(f)
#define setbuf(f, b)	(void)sm_io_setvbuf(f, b, b ? SM_IO_FBF : \
				SM_IO_NBF, SM_IO_BUFSIZ);
#define setbuffer(f, b, size)	(void)sm_io_setvbuf(f, b, b ? SM_IO_FBF : \
					SM_IO_NBF, size);
#define setlinebuf(f)	sm_io_setvbuf(f, (char *)NULL, SM_IO_LBF, (size_t)0);
#define setvbuf(f, b, m, size)	sm_io_setvbuf(f, b, m, size)
#define ungetc(c, f)	sm_io_ungetc(f, c)

#define fileno(fp)	sm_io_getinfo(fp, SM_IO_WHAT_FD, NULL)
#define fopen(path, mode)	((FILE *)sm_io_open(SmFtStdio, (void *)(fd), (strcmp((mode), "r")==0?SM_IO_RDONLY:strcmp((mode), "w")==0?SM_IO_WRONLY:SM_IO_RDWR), NULL))
#define freopen(path, mode, fp)	((FILE *)sm_io_reopen(SmFtStdio, (path), (strcmp((mode), "w")==0?SM_IO_WRONLY:SM_IO_RDONLY), NULL, (fp))
#define fdopen(fd, mode)	((FILE *)sm_io_open(SmFtStdiofd, (void *)(fd), (strcmp((mode), "r")==0?SM_IO_RDONLY:strcmp((mode), "w")==0?SM_IO_WRONLY:SM_IO_RDWR), NULL))

/* the following have variable arg counts */
#define fscanf	sm_io_fscanf
#define fprintf	sm_io_fprintf
#define printf(	sm_io_printf(smioout,
#define snprintf	sm_io_snprintf
#define vsnprintf	sm_io_vsnprintf
#define vfprintf	sm_io_vfprintf

#endif /* SM_STDIO_H */
