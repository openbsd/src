
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	CLIBsdio.h
 * DESCRIPTION	:	Forces the use of clib stdio.h calls over static watcom calls
 *                  for C/C++ applications that statically link watcom libraries.
 *
 *                  This file must be included each time that stdio.h is included.
 *                  In the case of the Perl project, just include stdio.h and
 *                  the make should take care of the rest.
 * Author		:	HYAK
 * Date			:	January 2001.
 *
 */



#ifndef _CLIBSDIO_H_
#define _CLIBSDIO_H_


#ifdef DEFINE_GPF
#define _GPFINIT =0
#define _GPFEXT
#else
#define _GPFINIT
#define _GPFEXT extern
#endif

#ifdef __cplusplus
extern "C"
{
#endif

_GPFEXT void* gpf___get_stdin _GPFINIT;
_GPFEXT void* gpf___get_stdout _GPFINIT;
_GPFEXT void* gpf___get_stderr _GPFINIT;

_GPFEXT void* gpf_clearerr _GPFINIT;
_GPFEXT void* gpf_fclose _GPFINIT;
_GPFEXT void* gpf_feof _GPFINIT;
_GPFEXT void* gpf_ferror _GPFINIT;
_GPFEXT void* gpf_fflush _GPFINIT;
_GPFEXT void* gpf_fgetc _GPFINIT;
_GPFEXT void* gpf_fgetpos _GPFINIT;
_GPFEXT void* gpf_fgets _GPFINIT;
_GPFEXT void* gpf_fopen _GPFINIT;
_GPFEXT void* gpf_fprintf _GPFINIT;
_GPFEXT void* gpf_fputc _GPFINIT;
_GPFEXT void* gpf_fputs _GPFINIT;
_GPFEXT void* gpf_fread _GPFINIT;
_GPFEXT void* gpf_freopen _GPFINIT;
_GPFEXT void* gpf_fscanf _GPFINIT;
_GPFEXT void* gpf_fseek _GPFINIT;
_GPFEXT void* gpf_fsetpos _GPFINIT;
_GPFEXT void* gpf_ftell _GPFINIT;
_GPFEXT void* gpf_fwrite _GPFINIT;
_GPFEXT void* gpf_getc _GPFINIT;
_GPFEXT void* gpf_getchar _GPFINIT;
_GPFEXT void* gpf_gets _GPFINIT;
_GPFEXT void* gpf_perror _GPFINIT;
_GPFEXT void* gpf_printf _GPFINIT;
_GPFEXT void* gpf_putc _GPFINIT;
_GPFEXT void* gpf_putchar _GPFINIT;
_GPFEXT void* gpf_puts _GPFINIT;
_GPFEXT void* gpf_rename _GPFINIT;
_GPFEXT void* gpf_rewind _GPFINIT;
_GPFEXT void* gpf_scanf _GPFINIT;
_GPFEXT void* gpf_setbuf _GPFINIT;
_GPFEXT void* gpf_setvbuf _GPFINIT;
_GPFEXT void* gpf_sprintf _GPFINIT;
_GPFEXT void* gpf_sscanf _GPFINIT;
_GPFEXT void* gpf_tmpfile _GPFINIT;
_GPFEXT void* gpf_tmpnam _GPFINIT;
_GPFEXT void* gpf_ungetc _GPFINIT;
_GPFEXT void* gpf_vfprintf _GPFINIT;
_GPFEXT void* gpf_vfscanf _GPFINIT;
_GPFEXT void* gpf_vprintf _GPFINIT;
_GPFEXT void* gpf_vscanf _GPFINIT;
_GPFEXT void* gpf_vsprintf _GPFINIT;
_GPFEXT void* gpf_vsscanf _GPFINIT;

_GPFEXT void* gpf_fdopen _GPFINIT;
_GPFEXT void* gpf_fileno _GPFINIT;

_GPFEXT void* gpf_cgets _GPFINIT;
_GPFEXT void* gpf_cprintf _GPFINIT;
_GPFEXT void* gpf_cputs _GPFINIT;
_GPFEXT void* gpf_cscanf _GPFINIT;
_GPFEXT void* gpf_fcloseall _GPFINIT;
_GPFEXT void* gpf_fgetchar _GPFINIT;
_GPFEXT void* gpf_flushall _GPFINIT;
_GPFEXT void* gpf_fputchar _GPFINIT;
_GPFEXT void* gpf_getch _GPFINIT;
_GPFEXT void* gpf_getche _GPFINIT;
_GPFEXT void* gpf_putch _GPFINIT;
_GPFEXT void* gpf_ungetch _GPFINIT;
_GPFEXT void* gpf_vcprintf _GPFINIT;
_GPFEXT void* gpf_vcscanf _GPFINIT;

#ifdef __cplusplus
}
#endif

#pragma aux __get_stdin = "call gpf___get_stdin";
#pragma aux __get_stdout = "call gpf___get_stdout";
#pragma aux __get_stderr = "call gpf___get_stderr";

#pragma aux clearerr = "call gpf_clearerr";
#pragma aux fclose = "call gpf_fclose";
#pragma aux feof = "call gpf_feof";
#pragma aux ferror = "call gpf_ferror";
#pragma aux fflush = "call gpf_fflush";
#pragma aux fgetc = "call gpf_fgetc";
#pragma aux fgetpos = "call gpf_fgetpos";
#pragma aux fgets = "call gpf_fgets";
#pragma aux fopen = "call gpf_fopen";
#pragma aux fprintf = "call gpf_fprintf";
#pragma aux fputc = "call gpf_fputc";
#pragma aux fputs = "call gpf_fputs";
#pragma aux fread = "call gpf_fread";
#pragma aux freopen = "call gpf_freopen";
#pragma aux fscanf = "call gpf_fscanf";
#pragma aux fseek = "call gpf_fseek";
#pragma aux fsetpos = "call gpf_fsetpos";
#pragma aux ftell = "call gpf_ftell";
#pragma aux fwrite = "call gpf_fwrite";
#pragma aux getc = "call gpf_getc";
#pragma aux getchar = "call gpf_getchar";
#pragma aux gets = "call gpf_gets";
#pragma aux perror = "call gpf_perror";
#pragma aux printf = "call gpf_printf";
#pragma aux putc = "call gpf_putc";
#pragma aux putchar = "call gpf_putchar";
#pragma aux puts = "call gpf_puts";
#pragma aux rename = "call gpf_rename";
#pragma aux rewind = "call gpf_rewind";
#pragma aux scanf = "call gpf_scanf";
#pragma aux setbuf = "call gpf_setbuf";
#pragma aux setvbuf = "call gpf_setvbuf";
#pragma aux sprintf = "call gpf_sprintf";
#pragma aux sscanf = "call gpf_sscanf";
#pragma aux tmpfile = "call gpf_tmpfile";
#pragma aux tmpnam = "call gpf_tmpnam";
#pragma aux ungetc = "call gpf_ungetc";
#pragma aux vfprintf = "call gpf_vfprintf";
#pragma aux vfscanf = "call gpf_vfscanf";
#pragma aux vprintf = "call gpf_vprintf";
#pragma aux vscanf = "call gpf_vscanf";
#pragma aux vsprintf = "call gpf_vsprintf";
#pragma aux vsscanf = "call gpf_vsscanf";

#pragma aux fdopen = "call gpf_fdopen";
#pragma aux fileno = "call gpf_fileno";

#pragma aux cgets = "call gpf_cgets";
#pragma aux cprintf = "call gpf_cprintf";
#pragma aux cputs = "call gpf_cputs";
#pragma aux cscanf = "call gpf_cscanf";
#pragma aux fcloseall = "call gpf_fcloseall";
#pragma aux fgetchar = "call gpf_fgetchar";
#pragma aux flushall = "call gpf_flushall";
#pragma aux fputchar = "call gpf_fputchar";
#pragma aux getch = "call gpf_getch";
#pragma aux getche = "call gpf_getche";
#pragma aux putch = "call gpf_putch";
#pragma aux ungetch = "call gpf_ungetch";
#pragma aux vcprintf = "call gpf_vcprintf";
#pragma aux vcscanf = "call gpf_vcscanf";


#endif	// _CLIBSDIO_H_

