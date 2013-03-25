/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME     :  nwperlsys.h
 * DESCRIPTION  :  Derives from iperlsys.h and define the 
 *                 platform specific function
 * Author       :  SGP
 * Date	Created :  June 12th 2001.
 * Date Modified:  June 30th 2001.
 */

#ifndef ___NWPerlSys_H___
#define ___NWPerlSys_H___


#include "iperlsys.h"
#include "nwstdio.h"

#include "nw5iop.h"
#include <fcntl.h>

//Socket related calls
#include "nw5sck.h"

//Store the Watcom hash list
#include "nwtinfo.h"

//Watcom hash list
#include <wchash.h>

#include "win32ish.h"

START_EXTERN_C
extern int do_spawn2(char *cmd, int exectype);
extern int do_aspawn(void *vreally, void **vmark, void **vsp);
extern void Perl_init_os_extras(void);
BOOL fnGetHashListAddrs(void *addrs, BOOL *dontTouchHashList);
END_EXTERN_C

/* IPerlMem - Memory management functions - Begin ========================================*/

void*
PerlMemMalloc(struct IPerlMem* piPerl, size_t size)
{
	void *ptr = NULL;
	ptr = malloc(size);
	if (ptr) {
		void **listptr;
		BOOL m_dontTouchHashLists;
		if(fnGetHashListAddrs(&listptr,&m_dontTouchHashLists)) {
			if (listptr) {
				WCValHashTable<void*>* m_allocList= (WCValHashTable<void*>*)listptr;
				(WCValHashTable<void*>*)m_allocList->insert(ptr);
			}
		}
	}
	return(ptr);
}

void*
PerlMemRealloc(struct IPerlMem* piPerl, void* ptr, size_t size)
{
	void *newptr = NULL;
	WCValHashTable<void*>* m_allocList;

	newptr = realloc(ptr, size);

	if (ptr)
	{
		void **listptr;
		BOOL m_dontTouchHashLists;
		if(fnGetHashListAddrs(&listptr,&m_dontTouchHashLists)) {
			m_allocList= (WCValHashTable<void*>*)listptr;
			(WCValHashTable<void*>*)m_allocList->remove(ptr);
		}
	}
	if (newptr)
	{
		if (m_allocList)
			(WCValHashTable<void*>*)m_allocList->insert(newptr);
	}

	return(newptr);
}

void
PerlMemFree(struct IPerlMem* piPerl, void* ptr)
{
	BOOL m_dontTouchHashLists;
	WCValHashTable<void*>* m_allocList;

	void **listptr;
	if(fnGetHashListAddrs(&listptr,&m_dontTouchHashLists)) {
		m_allocList= (WCValHashTable<void*>*)listptr;
		// Final clean up, free all the nodes from the hash list
		if (m_dontTouchHashLists)
		{
			if(ptr)
			{
				free(ptr);
				ptr = NULL;
			}
		}
		else
		{
			if(ptr && m_allocList)
			{
				if ((WCValHashTable<void*>*)m_allocList->remove(ptr))
				{
					free(ptr);
					ptr = NULL;
				}
				else
				{
					// If it comes here, that means that the memory pointer is not contained in the hash list.
					// But no need to free now, since if is deleted here, it will result in an abend!!
					// If the memory is still there, it will be cleaned during final cleanup anyway.
				}
			}
		}
	}
	return;
}

void*
PerlMemCalloc(struct IPerlMem* piPerl, size_t num, size_t size)
{
	void *ptr = NULL;

	ptr = calloc(num, size);
	if (ptr) {
		void **listptr;
		BOOL m_dontTouchHashLists;
		if(fnGetHashListAddrs(&listptr,&m_dontTouchHashLists)) {
			if (listptr) {
				WCValHashTable<void*>* m_allocList= (WCValHashTable<void*>*)listptr;
				(WCValHashTable<void*>*)m_allocList->insert(ptr);
			}
		}
	}
	return(ptr);
}

struct IPerlMem perlMem =
{
    PerlMemMalloc,
    PerlMemRealloc,
    PerlMemFree,
    PerlMemCalloc,
};

/* IPerlMem - Memory management functions - End   ========================================*/

/* IPerlDir	- Directory Manipulation functions - Begin ===================================*/

int
PerlDirMakedir(struct IPerlDir* piPerl, const char *dirname, int mode)
{
	return mkdir(dirname);
}

int
PerlDirChdir(struct IPerlDir* piPerl, const char *dirname)
{
	return nw_chdir(dirname);
}

int
PerlDirRmdir(struct IPerlDir* piPerl, const char *dirname)
{
	return nw_rmdir(dirname);
}

int
PerlDirClose(struct IPerlDir* piPerl, DIR *dirp)
{
	return nw_closedir(dirp);
}

DIR*
PerlDirOpen(struct IPerlDir* piPerl, const char *filename)
{
	return nw_opendir(filename);
}

struct direct *
PerlDirRead(struct IPerlDir* piPerl, DIR *dirp)
{
	return nw_readdir(dirp);
}

void
PerlDirRewind(struct IPerlDir* piPerl, DIR *dirp)
{
    nw_rewinddir(dirp);
}

void
PerlDirSeek(struct IPerlDir* piPerl, DIR *dirp, long loc)
{
    nw_seekdir(dirp, loc);
}

long
PerlDirTell(struct IPerlDir* piPerl, DIR *dirp)
{
    return nw_telldir(dirp);
}

struct IPerlDir perlDir =
{
	PerlDirMakedir,
    PerlDirChdir,
    PerlDirRmdir,
    PerlDirClose,
    PerlDirOpen,
    PerlDirRead,
    PerlDirRewind,
    PerlDirSeek,
    PerlDirTell,
};

/* IPerlDir	- Directory Manipulation functions - End   ===================================*/

/* IPerlEnv	- Environment related functions - Begin ======================================*/

char*
PerlEnvGetenv(struct IPerlEnv* piPerl, const char *varname)
{
	return(getenv(varname));
};

int
PerlEnvPutenv(struct IPerlEnv* piPerl, const char *envstring)
{
	return(putenv(envstring));
};

char*
PerlEnvGetenv_len(struct IPerlEnv* piPerl, const char* varname, unsigned long* len)
{
	*len = 0; 
	char *e = getenv(varname);
	if (e)
	    *len = strlen(e);
	return e;
}

int
PerlEnvUname(struct IPerlEnv* piPerl, struct utsname *name)
{
    return nw_uname(name);
}

void
PerlEnvClearenv(struct IPerlEnv* piPerl)
{
	
}

struct IPerlEnv perlEnv = 
{
	PerlEnvGetenv,
	PerlEnvPutenv,
    PerlEnvGetenv_len,
    PerlEnvUname,
    PerlEnvClearenv,
/*    PerlEnvGetChildenv,
    PerlEnvFreeChildenv,
    PerlEnvGetChilddir,
    PerlEnvFreeChilddir,*/
};

/* IPerlEnv	- Environment related functions - End   ======================================*/

/* IPerlStdio	- Stdio functions - Begin ================================================*/

FILE*
PerlStdIOStdin(struct IPerlStdIO* piPerl)
{
    return nw_stdin();
}

FILE*
PerlStdIOStdout(struct IPerlStdIO* piPerl)
{
    return nw_stdout();
}

FILE*
PerlStdIOStderr(struct IPerlStdIO* piPerl)
{
    return nw_stderr();
}

FILE*
PerlStdIOOpen(struct IPerlStdIO* piPerl, const char *path, const char *mode)
{
    return nw_fopen(path, mode);
}

int
PerlStdIOClose(struct IPerlStdIO* piPerl, FILE* pf)
{
    return nw_fclose(pf);
}

int
PerlStdIOEof(struct IPerlStdIO* piPerl, FILE* pf)
{
    return nw_feof(pf);
}

int
PerlStdIOError(struct IPerlStdIO* piPerl, FILE* pf)
{
    return nw_ferror(pf);
}

void
PerlStdIOClearerr(struct IPerlStdIO* piPerl, FILE* pf)
{
    nw_clearerr(pf);
}

int
PerlStdIOGetc(struct IPerlStdIO* piPerl, FILE* pf)
{
    return nw_getc(pf);
}

STDCHAR*
PerlStdIOGetBase(struct IPerlStdIO* piPerl, FILE* pf)
{
#ifdef FILE_base
    FILE *f = pf;
    return FILE_base(f);
#else
    return NULL;
#endif
}

int
PerlStdIOGetBufsiz(struct IPerlStdIO* piPerl, FILE* pf)
{
#ifdef FILE_bufsiz
    FILE *f = pf;
    return FILE_bufsiz(f);
#else
    return (-1);
#endif
}

int
PerlStdIOGetCnt(struct IPerlStdIO* piPerl, FILE* pf)
{
#ifdef USE_STDIO_PTR
    FILE *f = pf;
    return FILE_cnt(f);
#else
    return (-1);
#endif
}

STDCHAR*
PerlStdIOGetPtr(struct IPerlStdIO* piPerl, FILE* pf)
{
#ifdef USE_STDIO_PTR
    FILE *f = pf;
    return FILE_ptr(f);
#else
    return NULL;
#endif
}

char*
PerlStdIOGets(struct IPerlStdIO* piPerl, FILE* pf, char* s, int n)
{
    return nw_fgets(s, n, pf);
}

int
PerlStdIOPutc(struct IPerlStdIO* piPerl, FILE* pf, int c)
{
    return nw_fputc(c, pf);
}

int
PerlStdIOPuts(struct IPerlStdIO* piPerl, FILE* pf, const char *s)
{
    return nw_fputs(s, pf);
}

int
PerlStdIOFlush(struct IPerlStdIO* piPerl, FILE* pf)
{
    return nw_fflush(pf);
}

int
PerlStdIOUngetc(struct IPerlStdIO* piPerl, int c, FILE* pf)
{
    return nw_ungetc(c, pf);
}

int
PerlStdIOFileno(struct IPerlStdIO* piPerl, FILE* pf)
{
    return nw_fileno(pf);
}

FILE*
PerlStdIOFdopen(struct IPerlStdIO* piPerl, int fd, const char *mode)
{
    return nw_fdopen(fd, mode);
}

FILE*
PerlStdIOReopen(struct IPerlStdIO* piPerl, const char*path, const char*mode, FILE* pf)
{
    return nw_freopen(path, mode, pf);
}

SSize_t
PerlStdIORead(struct IPerlStdIO* piPerl, void *buffer, Size_t size, Size_t count, FILE* pf)
{
    return nw_fread(buffer, size, count, pf);
}

SSize_t
PerlStdIOWrite(struct IPerlStdIO* piPerl, const void *buffer, Size_t size, Size_t count, FILE* pf)
{
    return nw_fwrite(buffer, size, count, pf);
}

void
PerlStdIOSetBuf(struct IPerlStdIO* piPerl, FILE* pf, char* buffer)
{
    nw_setbuf(pf, buffer);
}

int
PerlStdIOSetVBuf(struct IPerlStdIO* piPerl, FILE* pf, char* buffer, int type, Size_t size)
{
    return nw_setvbuf(pf, buffer, type, size);
}

void
PerlStdIOSetCnt(struct IPerlStdIO* piPerl, FILE* pf, int n)
{
#ifdef STDIO_CNT_LVALUE
    FILE *f = pf;
    FILE_cnt(f) = n;
#endif
}

void
PerlStdIOSetPtr(struct IPerlStdIO* piPerl, FILE* pf, STDCHAR * ptr)
{
#ifdef STDIO_PTR_LVALUE
    FILE *f = pf;
    FILE_ptr(f) = ptr;
#endif
}

void
PerlStdIOSetlinebuf(struct IPerlStdIO* piPerl, FILE* pf)
{
    nw_setvbuf(pf, NULL, _IOLBF, 0);
}

int
PerlStdIOPrintf(struct IPerlStdIO* piPerl, FILE* pf, const char *format,...)
{
    va_list(arglist);
    va_start(arglist, format);
    return nw_vfprintf(pf, format, arglist);
}

int
PerlStdIOVprintf(struct IPerlStdIO* piPerl, FILE* pf, const char *format, va_list arglist)
{
    return nw_vfprintf(pf, format, arglist);
}

long
PerlStdIOTell(struct IPerlStdIO* piPerl, FILE* pf)
{
    return nw_ftell(pf);
}

int
PerlStdIOSeek(struct IPerlStdIO* piPerl, FILE* pf, off_t offset, int origin)
{
    return nw_fseek(pf, offset, origin);
}

void
PerlStdIORewind(struct IPerlStdIO* piPerl, FILE* pf)
{
    nw_rewind(pf);
}

FILE*
PerlStdIOTmpfile(struct IPerlStdIO* piPerl)
{
    return nw_tmpfile();
}

int
PerlStdIOGetpos(struct IPerlStdIO* piPerl, FILE* pf, Fpos_t *p)
{
    return nw_fgetpos(pf, p);
}

int
PerlStdIOSetpos(struct IPerlStdIO* piPerl, FILE* pf, const Fpos_t *p)
{
    return nw_fsetpos(pf, p);
}

void
PerlStdIOInit(struct IPerlStdIO* piPerl)
{
}

void
PerlStdIOInitOSExtras(struct IPerlStdIO* piPerl)
{
    Perl_init_os_extras();
}


int
PerlStdIOOpenOSfhandle(struct IPerlStdIO* piPerl, long osfhandle, int flags)
{
    return nw_open_osfhandle(osfhandle, flags);
}

int
PerlStdIOGetOSfhandle(struct IPerlStdIO* piPerl, int filenum)
{
    return nw_get_osfhandle(filenum);
}

FILE*
PerlStdIOFdupopen(struct IPerlStdIO* piPerl, FILE* pf)
{
    FILE* pfdup=NULL;
    fpos_t pos=0;
    char mode[3]={'\0'};
    int fileno = nw_dup(nw_fileno(pf));

    /* open the file in the same mode */
    if(((FILE*)pf)->_flag & _IOREAD) {
	mode[0] = 'r';
	mode[1] = 0;
    }
    else if(((FILE*)pf)->_flag & _IOWRT) {
	mode[0] = 'a';
	mode[1] = 0;
    }
    else if(((FILE*)pf)->_flag & _IORW) {
	mode[0] = 'r';
	mode[1] = '+';
	mode[2] = 0;
    }

    /* it appears that the binmode is attached to the 
     * file descriptor so binmode files will be handled
     * correctly
     */
    pfdup = nw_fdopen(fileno, mode);

    /* move the file pointer to the same position */
    if (!fgetpos(pf, &pos)) {
	fsetpos(pfdup, &pos);
    }
    return pfdup;
}

struct IPerlStdIO perlStdIO =
{
	PerlStdIOStdin,
    PerlStdIOStdout,
    PerlStdIOStderr,
    PerlStdIOOpen,
    PerlStdIOClose,
    PerlStdIOEof,
    PerlStdIOError,
    PerlStdIOClearerr,
    PerlStdIOGetc,
    PerlStdIOGetBase,
    PerlStdIOGetBufsiz,
    PerlStdIOGetCnt,
    PerlStdIOGetPtr,
    PerlStdIOGets,
    PerlStdIOPutc,
    PerlStdIOPuts,
    PerlStdIOFlush,
    PerlStdIOUngetc,
    PerlStdIOFileno,
    PerlStdIOFdopen,
    PerlStdIOReopen,
    PerlStdIORead,
    PerlStdIOWrite,
    PerlStdIOSetBuf,
    PerlStdIOSetVBuf,
    PerlStdIOSetCnt,
    PerlStdIOSetPtr,
    PerlStdIOSetlinebuf,
    PerlStdIOPrintf,
    PerlStdIOVprintf,
    PerlStdIOTell,
    PerlStdIOSeek,
    PerlStdIORewind,
    PerlStdIOTmpfile,
    PerlStdIOGetpos,
    PerlStdIOSetpos,
    PerlStdIOInit,
    PerlStdIOInitOSExtras,
    PerlStdIOFdupopen,
};

/* IPerlStdio	- Stdio functions - End   ================================================*/

/* IPerlLIO	- Low-level IO functions - Begin =============================================*/

int
PerlLIOAccess(struct IPerlLIO* piPerl, const char *path, int mode)
{
    return nw_access(path, mode);
}

int
PerlLIOChmod(struct IPerlLIO* piPerl, const char *filename, int pmode)
{
    return nw_chmod(filename, pmode);
}

int
PerlLIOChown(struct IPerlLIO* piPerl, const char *filename, uid_t owner, gid_t group)
{
	dTHX;
    Perl_croak(aTHX_ "chown not implemented!\n");
	return 0;
}

int
PerlLIOChsize(struct IPerlLIO* piPerl, int handle, long size)
{
	return (nw_chsize(handle,size));
}

int
PerlLIOClose(struct IPerlLIO* piPerl, int handle)
{
    return nw_close(handle);
}

int
PerlLIODup(struct IPerlLIO* piPerl, int handle)
{
    return nw_dup(handle);
}

int
PerlLIODup2(struct IPerlLIO* piPerl, int handle1, int handle2)
{
    return nw_dup2(handle1, handle2);
}

int
PerlLIOFlock(struct IPerlLIO* piPerl, int fd, int oper)
{
	//On NetWare simulate flock by locking a range on the file
    return nw_flock(fd, oper);
}

int
PerlLIOFileStat(struct IPerlLIO* piPerl, int handle, struct stat *buffer)
{
    return fstat(handle, buffer);
}

int
PerlLIOIOCtl(struct IPerlLIO* piPerl, int i, unsigned int u, char *data)
{
	return 0;
}

int
PerlLIOIsatty(struct IPerlLIO* piPerl, int fd)
{
    return nw_isatty(fd);
}

int
PerlLIOLink(struct IPerlLIO* piPerl, const char*oldname, const char *newname)
{
    return nw_link(oldname, newname);
}

long
PerlLIOLseek(struct IPerlLIO* piPerl, int handle, long offset, int origin)
{
    return nw_lseek(handle, offset, origin);
}

int
PerlLIOLstat(struct IPerlLIO* piPerl, const char *path, struct stat *buffer)
{
    return nw_stat(path, buffer);
}

char*
PerlLIOMktemp(struct IPerlLIO* piPerl, char *Template)
{
	return(nw_mktemp(Template));
}

int
PerlLIOOpen(struct IPerlLIO* piPerl, const char *filename, int oflag)
{
    return nw_open(filename, oflag);
}

int
PerlLIOOpen3(struct IPerlLIO* piPerl, const char *filename, int oflag, int pmode)
{
    return nw_open(filename, oflag, pmode);
}

int
PerlLIORead(struct IPerlLIO* piPerl, int handle, void *buffer, unsigned int count)
{
    return nw_read(handle, buffer, count);
}

int
PerlLIORename(struct IPerlLIO* piPerl, const char *OldFileName, const char *newname)
{
    return nw_rename(OldFileName, newname);
}

int
PerlLIOSetmode(struct IPerlLIO* piPerl, FILE *fp, int mode)
{
    return nw_setmode(fp, mode);
}

int
PerlLIONameStat(struct IPerlLIO* piPerl, const char *path, struct stat *buffer)
{
    return nw_stat(path, buffer);
}

char*
PerlLIOTmpnam(struct IPerlLIO* piPerl, char *string)
{
    return tmpnam(string);
}

int
PerlLIOUmask(struct IPerlLIO* piPerl, int pmode)
{
    return umask(pmode);
}

int
PerlLIOUnlink(struct IPerlLIO* piPerl, const char *filename)
{
    return nw_unlink(filename);
}

int
PerlLIOUtime(struct IPerlLIO* piPerl, const char *filename, struct utimbuf *times)
{
    return nw_utime(filename, times);
}

int
PerlLIOWrite(struct IPerlLIO* piPerl, int handle, const void *buffer, unsigned int count)
{
    return nw_write(handle, buffer, count);
}

struct IPerlLIO perlLIO =
{
	PerlLIOAccess,
    PerlLIOChmod,
    PerlLIOChown,
    PerlLIOChsize,
    PerlLIOClose,
    PerlLIODup,
    PerlLIODup2,
    PerlLIOFlock,
    PerlLIOFileStat,
    PerlLIOIOCtl,
    PerlLIOIsatty,
    PerlLIOLink,
    PerlLIOLseek,
    PerlLIOLstat,
    PerlLIOMktemp,
    PerlLIOOpen,
    PerlLIOOpen3,
    PerlLIORead,
    PerlLIORename,
    PerlLIOSetmode,
    PerlLIONameStat,
    PerlLIOTmpnam,
    PerlLIOUmask,
    PerlLIOUnlink,
    PerlLIOUtime,
    PerlLIOWrite,    
};

/* IPerlLIO	- Low-level IO functions - End   =============================================*/

/* IPerlProc - Process control functions - Begin =========================================*/

#define EXECF_EXEC 1
#define EXECF_SPAWN 2

void
PerlProcAbort(struct IPerlProc* piPerl)
{
    nw_abort();
}

char *
PerlProcCrypt(struct IPerlProc* piPerl, const char* clear, const char* salt)
{
    return nw_crypt(clear, salt);
}

void
PerlProcExit(struct IPerlProc* piPerl, int status)
{
//    exit(status);
	dTHX;
	dJMPENV;
	JMPENV_JUMP(2);
}

void
PerlProc_Exit(struct IPerlProc* piPerl, int status)
{
//    _exit(status);
	dTHX;
	dJMPENV;
	JMPENV_JUMP(2);
}

int
PerlProcExecl(struct IPerlProc* piPerl, const char *cmdname, const char *arg0, const char *arg1, const char *arg2, const char *arg3)
{
	dTHX;
    Perl_croak(aTHX_ "execl not implemented!\n");
	return 0;
}

int
PerlProcExecv(struct IPerlProc* piPerl, const char *cmdname, const char *const *argv)
{
    return nw_execvp((char *)cmdname, (char **)argv);
}

int
PerlProcExecvp(struct IPerlProc* piPerl, const char *cmdname, const char *const *argv)
{
    return nw_execvp((char *)cmdname, (char **)argv);
}

uid_t
PerlProcGetuid(struct IPerlProc* piPerl)
{
	return 0;
}

uid_t
PerlProcGeteuid(struct IPerlProc* piPerl)
{
	return 0;
}

gid_t
PerlProcGetgid(struct IPerlProc* piPerl)
{
	return 0;
}

gid_t
PerlProcGetegid(struct IPerlProc* piPerl)
{
	return 0;
}

char *
PerlProcGetlogin(struct IPerlProc* piPerl)
{
	return NULL;
}

int
PerlProcKill(struct IPerlProc* piPerl, int pid, int sig)
{
    return nw_kill(pid, sig);
}

int
PerlProcKillpg(struct IPerlProc* piPerl, int pid, int sig)
{
    dTHX;
    Perl_croak(aTHX_ "killpg not implemented!\n");
    return 0;
}

int
PerlProcPauseProc(struct IPerlProc* piPerl)
{
    return nw_sleep((32767L << 16) + 32767);
}

PerlIO*
PerlProcPopen(struct IPerlProc* piPerl, const char *command, const char *mode)
{
    dTHX;
    PERL_FLUSHALL_FOR_CHILD;

	return (PerlIO*)nw_Popen((char *)command, (char *)mode, (int *)errno);
}

int
PerlProcPclose(struct IPerlProc* piPerl, PerlIO *stream)
{
    return nw_Pclose((FILE*)stream, (int *)errno);
}

int
PerlProcPipe(struct IPerlProc* piPerl, int *phandles)
{
    return nw_Pipe((int *)phandles, (int *)errno);
}

int
PerlProcSetuid(struct IPerlProc* piPerl, uid_t u)
{
	return 0;
}

int
PerlProcSetgid(struct IPerlProc* piPerl, gid_t g)
{
	return 0;
}

int
PerlProcSleep(struct IPerlProc* piPerl, unsigned int s)
{
    return nw_sleep(s);
}

int
PerlProcTimes(struct IPerlProc* piPerl, struct tms *timebuf)
{
    return nw_times(timebuf);
}

int
PerlProcWait(struct IPerlProc* piPerl, int *status)
{
    return nw_wait(status);
}

int
PerlProcWaitpid(struct IPerlProc* piPerl, int pid, int *status, int flags)
{
    return nw_waitpid(pid, status, flags);
}

Sighandler_t
PerlProcSignal(struct IPerlProc* piPerl, int sig, Sighandler_t subcode)
{
    return 0;
}

int
PerlProcFork(struct IPerlProc* piPerl)
{
	return 0;
}

int
PerlProcGetpid(struct IPerlProc* piPerl)
{
    return nw_getpid();
}

/*BOOL
PerlProcDoCmd(struct IPerlProc* piPerl, char *cmd)
{
    do_spawn2(cmd, EXECF_EXEC);
    return FALSE;
}*/

int
PerlProcSpawn(struct IPerlProc* piPerl, char* cmds)
{
    return do_spawn2(cmds, EXECF_SPAWN);
}

int
PerlProcSpawnvp(struct IPerlProc* piPerl, int mode, const char *cmdname, const char *const *argv)
{
    return nw_spawnvp(mode, (char *)cmdname, (char **)argv);
}

int
PerlProcASpawn(struct IPerlProc* piPerl, void *vreally, void **vmark, void **vsp)
{
    return do_aspawn(vreally, vmark, vsp);
}

struct IPerlProc perlProc =
{
    PerlProcAbort,
    PerlProcCrypt,
    PerlProcExit,
    PerlProc_Exit,
    PerlProcExecl,
    PerlProcExecv,
    PerlProcExecvp,
    PerlProcGetuid,
    PerlProcGeteuid,
    PerlProcGetgid,
    PerlProcGetegid,
    PerlProcGetlogin,
    PerlProcKill,
    PerlProcKillpg,
    PerlProcPauseProc,
    PerlProcPopen,
    PerlProcPclose,
    PerlProcPipe,
    PerlProcSetuid,
    PerlProcSetgid,
    PerlProcSleep,
    PerlProcTimes,
    PerlProcWait,
    PerlProcWaitpid,
    PerlProcSignal,
    PerlProcFork,
    PerlProcGetpid,
    //PerlProcLastHost;
    //PerlProcPopenList;
};

/* IPerlProc - Process control functions - End   =========================================*/

/* IPerlSock - Socket functions - Begin ==================================================*/

u_long
PerlSockHtonl(struct IPerlSock* piPerl, u_long hostlong)
{
	return(nw_htonl(hostlong));
}

u_short
PerlSockHtons(struct IPerlSock* piPerl, u_short hostshort)
{
	return(nw_htons(hostshort));
}

u_long
PerlSockNtohl(struct IPerlSock* piPerl, u_long netlong)
{
	return nw_ntohl(netlong);
}

u_short
PerlSockNtohs(struct IPerlSock* piPerl, u_short netshort)
{
	return nw_ntohs(netshort);
}

SOCKET PerlSockAccept(struct IPerlSock* piPerl, SOCKET s, struct sockaddr* addr, int* addrlen)
{
	return nw_accept(s, addr, addrlen);
}

int
PerlSockBind(struct IPerlSock* piPerl, SOCKET s, const struct sockaddr* name, int namelen)
{
	return nw_bind(s, name, namelen);
}

int
PerlSockConnect(struct IPerlSock* piPerl, SOCKET s, const struct sockaddr* name, int namelen)
{
	return nw_connect(s, name, namelen);
}

void
PerlSockEndhostent(struct IPerlSock* piPerl)
{
    nw_endhostent();
}

void
PerlSockEndnetent(struct IPerlSock* piPerl)
{
    nw_endnetent();
}

void
PerlSockEndprotoent(struct IPerlSock* piPerl)
{
    nw_endprotoent();
}

void
PerlSockEndservent(struct IPerlSock* piPerl)
{
    nw_endservent();
}

struct hostent*
PerlSockGethostbyaddr(struct IPerlSock* piPerl, const char* addr, int len, int type)
{
	return(nw_gethostbyaddr(addr,len,type));
}

struct hostent*
PerlSockGethostbyname(struct IPerlSock* piPerl, const char* name)
{
    return nw_gethostbyname(name);
}

struct hostent*
PerlSockGethostent(struct IPerlSock* piPerl)
{
	return(nw_gethostent());
}

int
PerlSockGethostname(struct IPerlSock* piPerl, char* name, int namelen)
{
	return nw_gethostname(name,namelen);
}

struct netent *
PerlSockGetnetbyaddr(struct IPerlSock* piPerl, long net, int type)
{
    return nw_getnetbyaddr(net, type);
}

struct netent *
PerlSockGetnetbyname(struct IPerlSock* piPerl, const char *name)
{
    return nw_getnetbyname((char*)name);
}

struct netent *
PerlSockGetnetent(struct IPerlSock* piPerl)
{
    return nw_getnetent();
}

int PerlSockGetpeername(struct IPerlSock* piPerl, SOCKET s, struct sockaddr* name, int* namelen)
{
    return nw_getpeername(s, name, namelen);
}

struct protoent*
PerlSockGetprotobyname(struct IPerlSock* piPerl, const char* name)
{
    return nw_getprotobyname(name);
}

struct protoent*
PerlSockGetprotobynumber(struct IPerlSock* piPerl, int number)
{
    return nw_getprotobynumber(number);
}

struct protoent*
PerlSockGetprotoent(struct IPerlSock* piPerl)
{
    return nw_getprotoent();
}

struct servent*
PerlSockGetservbyname(struct IPerlSock* piPerl, const char* name, const char* proto)
{
    return nw_getservbyname((char*)name, (char*)proto);
}

struct servent*
PerlSockGetservbyport(struct IPerlSock* piPerl, int port, const char* proto)
{
	return nw_getservbyport(port, proto);
}

struct servent*
PerlSockGetservent(struct IPerlSock* piPerl)
{
	return nw_getservent();
}

int
PerlSockGetsockname(struct IPerlSock* piPerl, SOCKET s, struct sockaddr* name, int* namelen)
{
	return nw_getsockname(s, name, namelen);
}

int
PerlSockGetsockopt(struct IPerlSock* piPerl, SOCKET s, int level, int optname, char* optval, int* optlen)
{
	return nw_getsockopt(s, level, optname, optval, optlen);
}

unsigned long
PerlSockInetAddr(struct IPerlSock* piPerl, const char* cp)
{
	return(nw_inet_addr(cp));
}

char*
PerlSockInetNtoa(struct IPerlSock* piPerl, struct in_addr in)
{
	return NULL;
}

int
PerlSockListen(struct IPerlSock* piPerl, SOCKET s, int backlog)
{
	return (nw_listen(s, backlog));
}

int
PerlSockRecv(struct IPerlSock* piPerl, SOCKET s, char* buffer, int len, int flags)
{
	return (nw_recv(s, buffer, len, flags));
}

int
PerlSockRecvfrom(struct IPerlSock* piPerl, SOCKET s, char* buffer, int len, int flags, struct sockaddr* from, int* fromlen)
{
	return nw_recvfrom(s, buffer, len, flags, from, fromlen);
}

int
PerlSockSelect(struct IPerlSock* piPerl, int nfds, char* readfds, char* writefds, char* exceptfds, const struct timeval* timeout)
{
	return nw_select(nfds, (fd_set*) readfds, (fd_set*) writefds, (fd_set*) exceptfds, timeout);
}

int
PerlSockSend(struct IPerlSock* piPerl, SOCKET s, const char* buffer, int len, int flags)
{
	return (nw_send(s, buffer, len, flags));
}

int
PerlSockSendto(struct IPerlSock* piPerl, SOCKET s, const char* buffer, int len, int flags, const struct sockaddr* to, int tolen)
{
	return(nw_sendto(s, buffer, len, flags, to, tolen));
}

void
PerlSockSethostent(struct IPerlSock* piPerl, int stayopen)
{
	nw_sethostent(stayopen);
}

void
PerlSockSetnetent(struct IPerlSock* piPerl, int stayopen)
{
	nw_setnetent(stayopen);
}

void
PerlSockSetprotoent(struct IPerlSock* piPerl, int stayopen)
{
	nw_setprotoent(stayopen);
}

void
PerlSockSetservent(struct IPerlSock* piPerl, int stayopen)
{
	nw_setservent(stayopen);
}

int
PerlSockSetsockopt(struct IPerlSock* piPerl, SOCKET s, int level, int optname, const char* optval, int optlen)
{
	return nw_setsockopt(s, level, optname, optval, optlen);
}

int
PerlSockShutdown(struct IPerlSock* piPerl, SOCKET s, int how)
{
	return nw_shutdown(s, how);
}

SOCKET
PerlSockSocket(struct IPerlSock* piPerl, int af, int type, int protocol)
{
	return nw_socket(af, type, protocol);
}

int
PerlSockSocketpair(struct IPerlSock* piPerl, int domain, int type, int protocol, int* fds)
{
    dTHX;
    Perl_croak(aTHX_ "socketpair not implemented!\n");
    return 0;
}

int
PerlSockIoctlsocket(struct IPerlSock* piPerl, SOCKET s, long cmd, u_long *argp)
{
	dTHX;
    Perl_croak(aTHX_ "ioctlsocket not implemented!\n");
	return 0;
}

struct IPerlSock  perlSock =
{
	PerlSockHtonl,
    PerlSockHtons,
    PerlSockNtohl,
    PerlSockNtohs,
    PerlSockAccept,
    PerlSockBind,
    PerlSockConnect,
    PerlSockEndhostent,
    PerlSockEndnetent,
    PerlSockEndprotoent,
    PerlSockEndservent,
    PerlSockGethostname,
    PerlSockGetpeername,
    PerlSockGethostbyaddr,
    PerlSockGethostbyname,
    PerlSockGethostent,
    PerlSockGetnetbyaddr,
    PerlSockGetnetbyname,
    PerlSockGetnetent,
    PerlSockGetprotobyname,
    PerlSockGetprotobynumber,
    PerlSockGetprotoent,
    PerlSockGetservbyname,
    PerlSockGetservbyport,
    PerlSockGetservent,
    PerlSockGetsockname,
    PerlSockGetsockopt,
    PerlSockInetAddr,
	PerlSockInetNtoa,
	PerlSockListen,
    PerlSockRecv,
    PerlSockRecvfrom,
    PerlSockSelect,
    PerlSockSend,
    PerlSockSendto,
    PerlSockSethostent,
    PerlSockSetnetent,
    PerlSockSetprotoent,
    PerlSockSetservent,
    PerlSockSetsockopt,
    PerlSockShutdown,
	PerlSockSocket,
    PerlSockSocketpair,
};

/* IPerlSock - Socket functions - End ==================================================*/

#endif /* ___NWPerlSys_H___ */
