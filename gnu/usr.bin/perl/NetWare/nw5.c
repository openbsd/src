
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	nw5.c
 * DESCRIPTION	:	Definitions for the redefined functions for NetWare.
 * Author		:	SGP, HYAK
 * Date			:	January 2001.
 *
 */



#include <perl.h>	// For dTHX, etc.
#include "nwpipe.h"


// This was added since the compile failed saying "undefined P_WAIT"
// when USE_ITHREADS was commented in the makefile
#ifndef P_WAIT
#define	P_WAIT		0
#endif

#ifndef P_NOWAIT
#define	P_NOWAIT	1
#endif

#define EXECF_EXEC 1
#define EXECF_SPAWN 2
#define EXECF_SPAWN_NOWAIT 3

static BOOL has_shell_metachars(char *ptr);

// The array is used to store pointer to the memory allocated to the TempPipeFile structure everytime
// a call to the function, nw_Popen. If a simple variable is used, everytime the memory is allocated before
// the previously allocated memory is freed, the pointer will get overwritten and the previous memory allocations
// are lost! Only the most recent one will get freed when calls are made to nw_Pclose.
// By using the array and the iPopenCount to index the array, all memory are freed!

// The size of the array indicates the limit on the no of times the nw_Popen function can be called (and
// memory allocted) from within a script through backtick operators!
// This is arbitrarily set to MAX_PIPE_RECURSION=256 which indicates there can be 256 nested backtick operators possible!
PTEMPPIPEFILE ptpf1[MAX_PIPE_RECURSION] = {'\0'};
int iPopenCount = 0;
FILE* File1[MAX_PIPE_RECURSION] = {'\0'};

/**
General:

In this code, wherever there is a  FILE *, the error condition is checked; and only if the FILE * is TRUE,
then the corresponding operation is done. Otherwise the error value is returned.
This is done because the file operations like "open" in the Perl code returns the FILE *,
returning a valid value if the file is found or NULL when the particular file is not found.
Now, if the return value is NULL, then an operation say "fgets", "fopen" etc. using this this NULL value
for FILE * will abend the server. If the check is made then an operation on a non existing file
does not abend the server.
**/

void
nw_abort(void)
{
	abort();	// Terminate the NLM application abnormally.
	return;
}

int
nw_access(const char *path, int mode) 
{
    return access(path, mode);
}

int
nw_chmod(const char *path, int mode)
{
    return chmod(path, mode);
}

void
nw_clearerr(FILE *pf)
{
	if(pf)
	    clearerr(pf);
}

int
nw_close(int fd)
{
    return close(fd);
}

nw_closedir(DIR *dirp)
{
	return (closedir(dirp));
}

void
nw_setbuf(FILE *pf, char *buf)
{
	if(pf)
	    setbuf(pf, buf);
}

int
nw_setmode(FILE *fp, int mode)
{
/**
	// Commented since a few abends were happening in fnFpSetMode
	int *dummy = 0;
	return(fnFpSetMode(fp, mode, dummy));
**/

	int handle = -1;
	errno = 0;

	handle = fileno(fp);
	if (errno)
	{
		errno = 0;
		return -1;
	}
	return setmode(handle, mode);
}

int
nw_setvbuf(FILE *pf, char *buf, int type, size_t size)
{
	if(pf)
	    return setvbuf(pf, buf, type, size);
	else
	    return -1;
}


unsigned int
nw_sleep(unsigned int t)
{
	delay(t*1000);	// Put the thread to sleep for 't' seconds.  Initially 't' is passed in milliseconds.
    return 0;
}

int
nw_spawnvp(int mode, char *cmdname, char **argv)
{
	// There is no pass-around environment on NetWare so we throw that
	// argument away for now.

	//  The function "spawnvp" does not work in all situations. Loading
	// edit.nlm seems to work, for example, but the name of the file
	// to edit does not appear to get passed correctly. Another problem
	// is that on Netware, P_WAIT does not really work reliably. It only
	// works with NLMs built to use CLIB (according to Nile Thayne).
	// NLMs such as EDIT that are written directly to the system have no
	// way of running synchronously from another process. The whole
	// architecture on NetWare seems pretty busted, so we just support it
	// as best we can.
	//
	// The spawnvp function only launches NLMs, it will not execute a command;
	// the NetWare "system" function is used for that purpose. Unfortunately, "system"
	// always returns success whether the command is successful or not or even
	// if the command was not found! To avoid ambiguity--you can have both an
	// NLM named "perl" and a system command named "perl"--we need to
	// force perl scripts to carry the word "load" when loading an NLM. This
	// might be clearer anyway.

	int ret = 0;
	int argc = 0;


	if (stricmp(cmdname, LOAD_COMMAND) == 0)
	{
		if (argv[1] != NULL)
			ret = spawnvp(mode, argv[1], &argv[1]);
	}
	else
	{
		int i=0;
		while (argv[i] != '\0')
			i++;
		argc = i;

		fnSystemCommand(argv, argc);
	}

	return ret;
}

int
nw_execv(char *cmdname, char **argv)
{
	return spawnvp(P_WAIT, cmdname, (char **)argv);
}


int
nw_execvp(char *cmdname, char **argv)
{
	return nw_spawnvp(P_WAIT, cmdname, (char **)argv);
}

int
nw_stat(const char *path, struct stat *sbuf)
{
	return (stat(path, sbuf));
}

FILE *
nw_stderr(void)
{
	return (stderr);
}

FILE *
nw_stdin(void)
{
	return (stdin);
}

FILE *
nw_stdout()
{
	return (stdout);
}

long
nw_telldir(DIR *dirp)
{
	dTHX;
	Perl_croak(aTHX_ "The telldir() function is not implemented on NetWare\n");
	return 0l;
}

int
nw_times(struct tms *timebuf)
{
	clock_t now = clock();

	timebuf->tms_utime = now;
	timebuf->tms_stime = 0;
	timebuf->tms_cutime = 0;
	timebuf->tms_cstime = 0;

	return 0;
}

FILE*
nw_tmpfile(void)
{
    return tmpfile();
}

int
nw_uname(struct utsname *name)
{
	return(uname(name));
}

int
nw_ungetc(int c, FILE *pf)
{
	if(pf)
	    return ungetc(c, pf);
	else
	    return -1;
}

int
nw_unlink(const char *filename)
{
	return(unlink(filename));
}

int
nw_utime(const char *filename, struct utimbuf *times)
{
 	 return(utime(filename, times));
}

int
nw_vfprintf(FILE *fp, const char *format, va_list args)
{
	if(fp)
	    return (vfprintf(fp, format, args));
	else
	    return -1;
}

int
nw_wait(int *status)
{
    return 0;	
}

int
nw_waitpid(int pid, int *status, int flags)
{
    return 0;	
}

int
nw_write(int fd, const void *buf, unsigned int cnt)
{
    return write(fd, buf, cnt);
}

char *
nw_crypt(const char *txt, const char *salt)
{
	 dTHX;

#ifdef HAVE_DES_FCRYPT
    dTHR;
    return des_fcrypt(txt, salt, w32_crypt_buffer);
#else
    Perl_croak(aTHX_ "The crypt() function is not implemented on NetWare\n");
    return NULL;
#endif
}

int
nw_dup(int fd)
{
    return dup(fd);
}

int
nw_dup2(int fd1,int fd2)
{
	return dup2(fd1,fd2);
}

void*
nw_dynaload(const char* filename)
{
	return NULL;
}

int
nw_fclose(FILE *pf)
{
	if(pf)
	    return (fclose(pf));
	else
	    return -1;
}

FILE *
nw_fdopen(int handle, const char *mode)
{
	return(fdopen(handle, mode));
}

int
nw_feof(FILE *fp)
{
	if(fp)
	    return (feof(fp));
	else
	    return -1;
}

int
nw_ferror(FILE *fp)
{
	if(fp)
	    return (ferror(fp));
	else
	    return -1;
}


int
nw_fflush(FILE *pf)
{
	if(pf)
	    return fflush(pf);
	else
	    return -1;
}

int
nw_fgetpos(FILE *pf, fpos_t *p)
{
	if(pf)
	    return fgetpos(pf, p);
	else
	    return -1;
}

char*
nw_fgets(char *s, int n, FILE *pf)
{
	if(pf)
	    return(fgets(s, n, pf));
	else
	    return NULL;
}

int
nw_fileno(FILE *pf)
{
	if(pf)
	    return fileno(pf);
	else
	    return -1;
}

int
nw_flock(int fd, int oper)
{
	dTHX;
	Perl_croak(aTHX_ "The flock() function is not implemented on NetWare\n");
	return 0;
}


FILE *
nw_fopen(const char *filename, const char *mode)
{
	return (fopen(filename, mode));
}

int
nw_fputc(int c, FILE *pf)
{
	if(pf)
	    return fputc(c,pf);
	else
	    return -1;
}

int
nw_fputs(const char *s, FILE *pf)
{
	if(pf)
	    return fputs(s, pf);
	else
	    return -1;
}

size_t
nw_fread(void *buf, size_t size, size_t count, FILE *fp)
{
	if(fp)
	    return fread(buf, size, count, fp);
	else
	    return -1;
}

FILE *
nw_freopen(const char *path, const char *mode, FILE *stream)
{
	if(stream)
	    return freopen(path, mode, stream);
	else
	    return NULL;
}

int
nw_fseek(FILE *pf, long offset, int origin)
{
	if(pf)
	    return (fseek(pf, offset, origin));
	else
	    return -1;
}

int
nw_fsetpos(FILE *pf, const fpos_t *p)
{
	if(pf)
	    return fsetpos(pf, p);
	else
	    return -1;
}

long
nw_ftell(FILE *pf)
{
	if(pf)
	    return ftell(pf);
	else
	    return -1;
}

size_t
nw_fwrite(const void *buf, size_t size, size_t count, FILE *fp)
{
	if(fp)
	    return fwrite(buf, size, count, fp);
	else
	    return -1;
}

long
nw_get_osfhandle(int fd)
{
	return 0l;
}

int
nw_getc(FILE *pf)
{
	if(pf)
	    return getc(pf);
	else
	    return -1;
}

int
nw_putc(int c, FILE *pf)
{
	if(pf)
	    return putc(c,pf);
	else
	    return -1;
}

int
nw_fgetc(FILE *pf)
{
	if(pf)
	    return fgetc(pf);
	else
	    return -1;
}

int
nw_getpid(void)
{
	return GetThreadGroupID();
}

int
nw_kill(int pid, int sig)
{
	return 0;
}

int
nw_link(const char *oldname, const char *newname)
{
	return 0;
}

long
nw_lseek(int fd, long offset, int origin)
{
    return lseek(fd, offset, origin);
}

int
nw_chdir(const char *dir)
{
    return chdir(dir);
}

int
nw_rmdir(const char *dir)
{
    return rmdir(dir);
}

DIR *
nw_opendir(const char *filename)
{
	char	*buff = NULL;
	int		len = 0;
	DIR		*ret = NULL;
	
	len = strlen(filename);
	buff = malloc(len + 5);
	if (buff) {
		strcpy(buff, filename);
		if (buff[len-1]=='/' || buff[len-1]=='\\') {
			buff[--len] = 0;
		}
		strcpy(buff+len, "/*.*");
		ret = opendir(buff);
		free (buff);
		buff = NULL;
		return ret;
	} else {
		return NULL;
	}
}

int
nw_open(const char *path, int flag, ...)
{
	va_list ap;
	int pmode = -1;

	va_start(ap, flag);
    pmode = va_arg(ap, int);
    va_end(ap);

	if (stricmp(path, "/dev/null")==0)
	path = "NWNUL";

	return open(path, flag, pmode);
}

int
nw_open_osfhandle(long handle, int flags)
{
	return 0;
}

unsigned long
nw_os_id(void)
{
	return 0l;
}

int nw_Pipe(int* a, int* e)
{
	int ret = 0;

	errno = 0;
	ret = pipe(a);
	if(errno)
		e = &errno;

	return ret;
}

FILE* nw_Popen(char* command, char* mode, int* e)
{
	int i = -1;

	FILE* ret = NULL;
	PTEMPPIPEFILE ptpf = NULL;

	// this callback is supposed to call _popen, which spawns an
	// asynchronous command and opens a pipe to it. The returned
	// file handle can be read or written to; if read, it represents
	// stdout of the called process and will return EOF when the
	// called process finishes. If written to, it represents stdin
	// of the called process. Naturally _popen is not available on
	// NetWare so we must do some fancy stuff to simulate it. We will
	// redirect to and from temp files; this has the side effect
	// of having to run the process synchronously rather than
	// asynchronously. This means that you will only be able to do
	// this with CLIB NLMs built to run on the calling thread.

	errno = 0;

	ptpf1[iPopenCount] = (PTEMPPIPEFILE) malloc(sizeof(TEMPPIPEFILE));
	if (!ptpf1[iPopenCount])
		return NULL;

	ptpf = ptpf1[iPopenCount];
	iPopenCount ++;
	if(iPopenCount > MAX_PIPE_RECURSION)
		iPopenCount = MAX_PIPE_RECURSION;	// Limit to the max no of pipes to be open recursively.

	fnTempPipeFile(ptpf);
	ret = fnPipeFileOpen((PTEMPPIPEFILE) ptpf, (char *) command, (char *) mode);
	if (ret)
		File1[iPopenCount-1] = ret;	// Store the obtained Pipe file handle.
	else
	{	// Pipe file not obtained. So free the allocated memory.
		if(ptpf1[iPopenCount-1])
		{
			free(ptpf1[iPopenCount-1]);
			ptpf1[iPopenCount-1] = NULL;
			ptpf = NULL;
			iPopenCount --;
		}
	}

	if (errno)
		e = &errno;

	return ret;
}

int nw_Pclose(FILE* file, int* e)
{
	int i=0, j=0;

	errno = 0;

	if(file)
	{
		if(iPopenCount > 0)
		{
			for (i=0; i<iPopenCount; i++)
			{
				if(File1[i] == file)
				{
					// Delete the memory allocated corresponding to the file handle passed-in and
					// also close the file corresponding to the file handle passed-in!
					if(ptpf1[i])
					{
						fnPipeFileClose(ptpf1[i]);

						free(ptpf1[i]);
						ptpf1[i] = NULL;
					}

					fclose(File1[i]);
					File1[i] = NULL;

					break;
				}
			}

			// Rearrange the file pointer array
			for(j=i; j<(iPopenCount-1); j++)
			{
				File1[j] = File1[j+1];
				ptpf1[j] = ptpf1[j+1];
			}
			iPopenCount--;
		}
	}
	else
	    return -1;

	if (errno)
		e = &errno;

	return 0;
}


int
nw_vprintf(const char *format, va_list args)
{
    return (vprintf(format, args));
}

int
nw_printf(const char *format, ...)
{
	
	va_list marker;
    va_start(marker, format);     /* Initialize variable arguments. */

    return (vprintf(format, marker));
}

int
nw_read(int fd, void *buf, unsigned int cnt)
{
	return read(fd, buf, cnt);
}

struct direct *
nw_readdir(DIR *dirp)
{
	DIR* ret=NULL;

	ret = readdir(dirp);
	if(ret)
		return((struct direct *)ret);
	return NULL;
}

int
nw_rename(const char *oname, const char *newname)
{
	return(rename(oname,newname));
}

void
nw_rewinddir(DIR *dirp)
{
	dTHX;
	Perl_croak(aTHX_ "The rewinddir() function is not implemented on NetWare\n");
}

void
nw_rewind(FILE *pf)
{
	if(pf)
	    rewind(pf);
}

void
nw_seekdir(DIR *dirp, long loc)
{
	dTHX;
	Perl_croak(aTHX_ "The seekdir() function is not implemented on NetWare\n");
}

int *
nw_errno(void)
{
    return (&errno);
}

char ***
nw_environ(void)
{
	return ((char ***)nw_getenviron());
}

char *
nw_strerror(int e)
{
	return (strerror(e));
}

int
nw_isatty(int fd)
{
	return(isatty(fd));
}

char *
nw_mktemp(char *Template)
{
	return (fnMy_MkTemp(Template));
}

int
nw_chsize(int handle, long size)
{
	return(chsize(handle,size));
}

#ifdef HAVE_INTERP_INTERN
void
sys_intern_init(pTHX)
{

}

void
sys_intern_clear(pTHX)
{

}

void
sys_intern_dup(pTHX_ struct interp_intern *src, struct interp_intern *dst)
{
    PERL_ARGS_ASSERT_SYS_INTERN_DUP;
}
#endif	/* HAVE_INTERP_INTERN */

void
Perl_init_os_extras(void)
{
    
}

void
Perl_nw5_init(int *argcp, char ***argvp)
{
    MALLOC_INIT;
}

#ifdef USE_ITHREADS
PerlInterpreter *
perl_clone_host(PerlInterpreter* proto_perl, UV flags)
{
	// Perl Clone is not implemented on NetWare.
    return NULL;
}
#endif

// Some more functions:

int
execv(char *cmdname, char **argv)
{
	// This feature needs to be implemented.
	// _asm is commented out since it goes into the internal debugger.
//	_asm {int 3};
	return(0);
}

int
execvp(char *cmdname, char **argv)
{
	// This feature needs to be implemented.
	// _asm is commented out since it goes into the internal debugger.
//	_asm {int 3};
	return(0);
}

int
do_aspawn(void *vreally, void **vmark, void **vsp)
{
	// This feature needs to be implemented.
	// _asm is commented out since it goes into the internal debugger.
//	_asm {int 3};
////	return(0);


	// This below code is required for system() call.
	// Otherwise system() does not work on NetWare.
	// Ananth, 3 Sept 2001

    dTHX;
    SV *really = (SV*)vreally;
    SV **mark = (SV**)vmark;
    SV **sp = (SV**)vsp;
    char **argv;
    char *str;
    int status;
    int flag = P_WAIT;
    int index = 0;


    if (sp <= mark)
	return -1;

	nw_perlshell_items = 0;	// No Shell
//    Newx(argv, (sp - mark) + nw_perlshell_items + 3, char*);	// In the old code of 5.6.1
    Newx(argv, (sp - mark) + nw_perlshell_items + 2, char*);

    if (SvNIOKp(*(mark+1)) && !SvPOKp(*(mark+1))) {
	++mark;
	flag = SvIVx(*mark);
    }

    while (++mark <= sp) {
	if (*mark && (str = (char *)SvPV_nolen(*mark)))
	{
	    argv[index] = str;
		index++;
	}
	else
	{
		argv[index] = "";
//		argv[index] = '\0';
		index++;
    }
	}
    argv[index] = '\0';
	index++;

    status = nw_spawnvp(flag,
			   (char*)(really ? SvPV_nolen(really) : argv[0]),
			   (char**)argv);

    if (flag != P_NOWAIT) {
	if (status < 0) {
//   	    dTHR;	// Only in old code of 5.6.1
	    if (ckWARN(WARN_EXEC))
		Perl_warner(aTHX_ packWARN(WARN_EXEC), "Can't spawn \"%s\": %s", argv[0], strerror(errno));
	    status = 255 * 256;
	}
	else
	    status *= 256;
	PL_statusvalue = status;
    }

    Safefree(argv);
    return (status);
}

int
do_spawn2(char *cmd, int exectype)
{
	// This feature needs to be implemented.
	// _asm is commented out since it goes into the internal debugger.
//	_asm {int 3};
////	return(0);

	// Below added to make system() work for NetWare

    dTHX;
    char **a;
    char *s;
    char **argv;
    int status = -1;
    BOOL needToTry = TRUE;
    char *cmd2;

    /* Save an extra exec if possible. See if there are shell
     * metacharacters in it */
    if (!has_shell_metachars(cmd)) {
	Newx(argv, strlen(cmd) / 2 + 2, char*);
	Newx(cmd2, strlen(cmd) + 1, char);
	strcpy(cmd2, cmd);
	a = argv;
	for (s = cmd2; *s;) {
	    while (*s && isSPACE(*s))
		s++;
	    if (*s)
		*(a++) = s;
	    while (*s && !isSPACE(*s))
		s++;
	    if (*s)
		*s++ = '\0';
	}
	*a = NULL;
	if (argv[0]) {
	    switch (exectype) {
			case EXECF_SPAWN:
				status = nw_spawnvp(P_WAIT, argv[0], (char **)argv);
				break;

			case EXECF_SPAWN_NOWAIT:
				status = nw_spawnvp(P_NOWAIT, argv[0], (char **)argv);
				break;

			case EXECF_EXEC:
				status = nw_execvp(argv[0], (char **)argv);
				break;
	    }
	    if (status != -1 || errno == 0)
		needToTry = FALSE;
	}
	Safefree(argv);
	Safefree(cmd2);
    }

    if (needToTry) {
	char **argv = NULL;
	int i = -1;

	Newx(argv, nw_perlshell_items + 2, char*);
	while (++i < nw_perlshell_items)
	    argv[i] = nw_perlshell_vec[i];
	argv[i++] = cmd;
	argv[i] = NULL;
	switch (exectype) {
		case EXECF_SPAWN:
			status = nw_spawnvp(P_WAIT, argv[0], (char **)argv);
			break;

		case EXECF_SPAWN_NOWAIT:
			status = nw_spawnvp(P_NOWAIT, argv[0], (char **)argv);
			break;

		case EXECF_EXEC:
			status = nw_execvp(argv[0], (char **)argv);
			break;
	}
	cmd = argv[0];
	Safefree(argv);
    }

    if (exectype != EXECF_SPAWN_NOWAIT) {
	if (status < 0) {
    	    dTHR;
	    if (ckWARN(WARN_EXEC))
		Perl_warner(aTHX_ WARN_EXEC, "Can't %s \"%s\": %s",
		     (exectype == EXECF_EXEC ? "exec" : "spawn"),
		     cmd, strerror(errno));
	    status = 255 * 256;
	}
	else
	    status *= 256;
	PL_statusvalue = status;
    }
    return (status);
}

int
do_spawn(char *cmd)
{
    return do_spawn2(cmd, EXECF_SPAWN);
}

// Added to make system() work for NetWare
static BOOL
has_shell_metachars(char *ptr)
{
    int inquote = 0;
    char quote = '\0';

    /*
     * Scan string looking for redirection (< or >) or pipe
     * characters (|) that are not in a quoted string.
     * Shell variable interpolation (%VAR%) can also happen inside strings.
     */
    while (*ptr) {
	switch(*ptr) {
	case '%':
	    return TRUE;
	case '\'':
	case '\"':
	    if (inquote) {
		if (quote == *ptr) {
		    inquote = 0;
		    quote = '\0';
		}
	    }
	    else {
		quote = *ptr;
		inquote++;
	    }
	    break;
	case '>':
	case '<':
	case '|':
	    if (!inquote)
		return TRUE;
	default:
	    break;
	}
	++ptr;
    }
    return FALSE;
}

int
fork(void)
{
	return 0;
}


// added to remove undefied symbol error in CodeWarrior compilation
int
Perl_Ireentrant_buffer_ptr(aTHX)
{
	return 0;
}
