
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	CLIBstuf.c
 * DESCRIPTION	:	The purpose of clibstuf is to make sure that Perl, cgi2perl and
 *                  all the perl extension nlm's (*.NLP) use the Novell Netware CLIB versions
 *                  of standard functions. This code loads up a whole bunch of function pointers
 *                  to point at the standard CLIB functions.
 * Author		:	HYAK
 * Date			:	January 2001.
 *
 */



#define DEFINE_GPF
#include "string.h"		// Our version of string.h will include clibstr.h
#include "stdio.h"		// Our version of stdio.h will include clibsdio.h
#include "clibstuf.h"

#include <nwthread.h>
#include <nwadv.h>
#include <nwconio.h>



void ImportFromCLIB (unsigned int nlmHandle, void** psymbol, char* symbolName)
{
	*psymbol = ImportSymbol(nlmHandle, symbolName);
	if (*psymbol == NULL)
	{
		ConsolePrintf("Symbol %s not found, unable to continue\n", symbolName);
		exit(1);
	}
}


void fnInitGpfGlobals(void)
{
	unsigned int nlmHandle = GetNLMHandle();

	ImportFromCLIB(nlmHandle, &gpf___get_stdin, "__get_stdin");
	ImportFromCLIB(nlmHandle, &gpf___get_stdout, "__get_stdout");
	ImportFromCLIB(nlmHandle, &gpf___get_stderr, "__get_stderr");
	ImportFromCLIB(nlmHandle, &gpf_clearerr, "clearerr");
	ImportFromCLIB(nlmHandle, &gpf_fclose, "fclose");
	ImportFromCLIB(nlmHandle, &gpf_feof, "feof");
	ImportFromCLIB(nlmHandle, &gpf_ferror, "ferror");
	ImportFromCLIB(nlmHandle, &gpf_fflush, "fflush");
	ImportFromCLIB(nlmHandle, &gpf_fgetc, "fgetc");
	ImportFromCLIB(nlmHandle, &gpf_fgetpos, "fgetpos");
	ImportFromCLIB(nlmHandle, &gpf_fgets, "fgets");
	ImportFromCLIB(nlmHandle, &gpf_fopen, "fopen");
	ImportFromCLIB(nlmHandle, &gpf_fputc, "fputc");
	ImportFromCLIB(nlmHandle, &gpf_fputs, "fputs");
	ImportFromCLIB(nlmHandle, &gpf_fread, "fread");
	ImportFromCLIB(nlmHandle, &gpf_freopen, "freopen");
	ImportFromCLIB(nlmHandle, &gpf_fscanf, "fscanf");
	ImportFromCLIB(nlmHandle, &gpf_fseek, "fseek");
	ImportFromCLIB(nlmHandle, &gpf_fsetpos, "fsetpos");
	ImportFromCLIB(nlmHandle, &gpf_ftell, "ftell");
	ImportFromCLIB(nlmHandle, &gpf_fwrite, "fwrite");
	ImportFromCLIB(nlmHandle, &gpf_getc, "getc");
	ImportFromCLIB(nlmHandle, &gpf_getchar, "getchar");
	ImportFromCLIB(nlmHandle, &gpf_gets, "gets");
	ImportFromCLIB(nlmHandle, &gpf_perror, "perror");
	ImportFromCLIB(nlmHandle, &gpf_putc, "putc");
	ImportFromCLIB(nlmHandle, &gpf_putchar, "putchar");
	ImportFromCLIB(nlmHandle, &gpf_puts, "puts");
	ImportFromCLIB(nlmHandle, &gpf_rename, "rename");
	ImportFromCLIB(nlmHandle, &gpf_rewind, "rewind");
	ImportFromCLIB(nlmHandle, &gpf_scanf, "scanf");
	ImportFromCLIB(nlmHandle, &gpf_setbuf, "setbuf");
	ImportFromCLIB(nlmHandle, &gpf_setvbuf, "setvbuf");
	ImportFromCLIB(nlmHandle, &gpf_sscanf, "sscanf");
	ImportFromCLIB(nlmHandle, &gpf_tmpfile, "tmpfile");
	ImportFromCLIB(nlmHandle, &gpf_tmpnam, "tmpnam");
	ImportFromCLIB(nlmHandle, &gpf_ungetc, "ungetc");
	ImportFromCLIB(nlmHandle, &gpf_vfscanf, "vfscanf");
	ImportFromCLIB(nlmHandle, &gpf_vscanf, "vscanf");
	ImportFromCLIB(nlmHandle, &gpf_vsscanf, "vsscanf");
	ImportFromCLIB(nlmHandle, &gpf_fdopen, "fdopen");
	ImportFromCLIB(nlmHandle, &gpf_fileno, "fileno");
	ImportFromCLIB(nlmHandle, &gpf_cgets, "cgets");
	ImportFromCLIB(nlmHandle, &gpf_cprintf, "cprintf");
	ImportFromCLIB(nlmHandle, &gpf_cputs, "cputs");
	ImportFromCLIB(nlmHandle, &gpf_cscanf, "cscanf");
	ImportFromCLIB(nlmHandle, &gpf_fcloseall, "fcloseall");
	ImportFromCLIB(nlmHandle, &gpf_fgetchar, "fgetchar");
	ImportFromCLIB(nlmHandle, &gpf_flushall, "flushall");
	ImportFromCLIB(nlmHandle, &gpf_fputchar, "fputchar");
	ImportFromCLIB(nlmHandle, &gpf_getch, "getch");
	ImportFromCLIB(nlmHandle, &gpf_getche, "getche");
	ImportFromCLIB(nlmHandle, &gpf_putch, "putch");
	ImportFromCLIB(nlmHandle, &gpf_ungetch, "ungetch");
	ImportFromCLIB(nlmHandle, &gpf_vcprintf, "vcprintf");
	ImportFromCLIB(nlmHandle, &gpf_vcscanf, "vcscanf");

	ImportFromCLIB(nlmHandle, &gpf_memchr, "memchr");
	ImportFromCLIB(nlmHandle, &gpf_memcmp, "memcmp");
	ImportFromCLIB(nlmHandle, &gpf_memcpy, "memcpy");
	ImportFromCLIB(nlmHandle, &gpf_memmove, "memmove");
	ImportFromCLIB(nlmHandle, &gpf_memset, "memset");
	ImportFromCLIB(nlmHandle, &gpf_memicmp, "memicmp");

	ImportFromCLIB(nlmHandle, &gpf_strerror, "strerror");
	ImportFromCLIB(nlmHandle, &gpf_strtok_r, "strtok_r");
	
	ImportFromCLIB(nlmHandle, &gpf_strcpy, "strcpy");
	ImportFromCLIB(nlmHandle, &gpf_strcat, "strcat");
	ImportFromCLIB(nlmHandle, &gpf_strchr, "strchr");
	ImportFromCLIB(nlmHandle, &gpf_strstr, "strstr");
	ImportFromCLIB(nlmHandle, &gpf_strcoll, "strcoll");
	ImportFromCLIB(nlmHandle, &gpf_strcspn, "strcspn");
	ImportFromCLIB(nlmHandle, &gpf_strpbrk, "strpbrk");
	ImportFromCLIB(nlmHandle, &gpf_strrchr, "strrchr");
	ImportFromCLIB(nlmHandle, &gpf_strrev, "strrev");
	ImportFromCLIB(nlmHandle, &gpf_strspn, "strspn");
	ImportFromCLIB(nlmHandle, &gpf_strupr, "strupr");
	ImportFromCLIB(nlmHandle, &gpf_strxfrm, "strxfrm");
	ImportFromCLIB(nlmHandle, &gpf_strcmp, "strcmp");
	ImportFromCLIB(nlmHandle, &gpf_stricmp, "stricmp");
	ImportFromCLIB(nlmHandle, &gpf_strtok, "strtok");
	ImportFromCLIB(nlmHandle, &gpf_strlen, "strlen");
	ImportFromCLIB(nlmHandle, &gpf_strncpy, "strncpy");
	ImportFromCLIB(nlmHandle, &gpf_strncat, "strncat");
	ImportFromCLIB(nlmHandle, &gpf_strncmp, "strncmp");
	ImportFromCLIB(nlmHandle, &gpf_strcmpi, "strcmpi");
	ImportFromCLIB(nlmHandle, &gpf_strnicmp, "strnicmp");
	ImportFromCLIB(nlmHandle, &gpf_strdup, "strdup");
	ImportFromCLIB(nlmHandle, &gpf_strlist, "strlist");
	ImportFromCLIB(nlmHandle, &gpf_strlwr, "strlwr");
	ImportFromCLIB(nlmHandle, &gpf_strnset, "strnset");
	ImportFromCLIB(nlmHandle, &gpf_strset, "strset");
	ImportFromCLIB(nlmHandle, &gpf_strtok_r, "strtok_r");
	ImportFromCLIB(nlmHandle, &gpf_printf, "printf");
	ImportFromCLIB(nlmHandle, &gpf_fprintf, "fprintf");
	ImportFromCLIB(nlmHandle, &gpf_sprintf, "sprintf");
	ImportFromCLIB(nlmHandle, &gpf_vprintf, "vprintf");
	ImportFromCLIB(nlmHandle, &gpf_vfprintf, "vfprintf");
	ImportFromCLIB(nlmHandle, &gpf_vsprintf, "vsprintf");

}

