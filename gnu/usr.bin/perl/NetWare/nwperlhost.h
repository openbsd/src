
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	nwperlhost.h
 * DESCRIPTION	:	This is modelled on the perlhost.h module of Win32 port.
 *                  This is the host that include all the functions for running Perl within a class.
 * Author		:	SGP, HYAK
 * Date			:	January 2001.
 *
 */



#ifndef ___NWPerlHost_H___
#define ___NWPerlHost_H___


#include "iperlsys.h"
#include "nwvmem.h"

#include "nw5sck.h"
#include "netware.h"

#define	LPBYTE	unsigned char *

#if !defined(PERL_OBJECT)
START_EXTERN_C
#endif	/* PERL_OBJECT */

extern int do_spawn2(char *cmd, int exectype);
extern int do_aspawn(void *vreally, void **vmark, void **vsp);
extern void Perl_init_os_extras(void);

#if !defined(PERL_OBJECT)
END_EXTERN_C
#endif	/* PERL_OBJECT */

#ifdef PERL_OBJECT
extern int g_do_aspawn(void *vreally, void **vmark, void **vsp);
#define do_aspawn g_do_aspawn
#endif	/* PERL_OBJECT */

class CPerlHost
{
public:
    CPerlHost(void);
    CPerlHost(struct IPerlMem** ppMem, struct IPerlMem** ppMemShared,
		 struct IPerlMem** ppMemParse, struct IPerlEnv** ppEnv,
		 struct IPerlStdIO** ppStdIO, struct IPerlLIO** ppLIO,
		 struct IPerlDir** ppDir, struct IPerlSock** ppSock,
		 struct IPerlProc** ppProc);
    CPerlHost(const CPerlHost& host);
    virtual ~CPerlHost(void);

    static CPerlHost* IPerlMem2Host(struct IPerlMem* piPerl);
    static CPerlHost* IPerlMemShared2Host(struct IPerlMem* piPerl);
    static CPerlHost* IPerlMemParse2Host(struct IPerlMem* piPerl);
    static CPerlHost* IPerlEnv2Host(struct IPerlEnv* piPerl);
    static CPerlHost* IPerlStdIO2Host(struct IPerlStdIO* piPerl);
    static CPerlHost* IPerlLIO2Host(struct IPerlLIO* piPerl);
    static CPerlHost* IPerlDir2Host(struct IPerlDir* piPerl);
    static CPerlHost* IPerlSock2Host(struct IPerlSock* piPerl);
    static CPerlHost* IPerlProc2Host(struct IPerlProc* piPerl);

/* IPerlMem */
    inline void* Malloc(size_t size) { return m_pVMem->Malloc(size); };
    inline void* Realloc(void* ptr, size_t size) { return m_pVMem->Realloc(ptr, size); };
    inline void Free(void* ptr) { m_pVMem->Free(ptr); };
	inline void* Calloc(size_t num, size_t size){ return m_pVMem->Calloc(num, size); };

/* IPerlMemShared */
    inline void* MallocShared(size_t size)
    {
	return m_pVMemShared->Malloc(size);
    };
    inline void* ReallocShared(void* ptr, size_t size) { return m_pVMemShared->Realloc(ptr, size); };
    inline void FreeShared(void* ptr) { m_pVMemShared->Free(ptr); };
    inline void* CallocShared(size_t num, size_t size)
    {
	size_t count = num*size;
	void* lpVoid = MallocShared(count);

	return lpVoid;
    };

/* IPerlMemParse */
    inline void* MallocParse(size_t size) { return m_pVMemParse->Malloc(size); };
    inline void* ReallocParse(void* ptr, size_t size) { return m_pVMemParse->Realloc(ptr, size); };
    inline void FreeParse(void* ptr) { m_pVMemParse->Free(ptr); };
    inline void* CallocParse(size_t num, size_t size)
    {
	size_t count = num*size;
	void* lpVoid = MallocParse(count);

	return lpVoid;
    };

/* IPerlEnv */
    char *Getenv(const char *varname);
    int Putenv(const char *envstring);
    inline char *Getenv(const char *varname, unsigned long *len)
    {
	*len = 0;
	char *e = Getenv(varname);
	if (e)
	    *len = strlen(e);
	return e;
    }


public:

/* IPerlDIR */


/* IPerllProc */
    void Abort(void);
    void Exit(int status);
    void _Exit(int status);
    int Execl(const char *cmdname, const char *arg0, const char *arg1, const char *arg2, const char *arg3);
    int Execv(const char *cmdname, const char *const *argv);
    int Execvp(const char *cmdname, const char *const *argv);

public:

    struct IPerlMem	    m_hostperlMem;
    struct IPerlMem	    m_hostperlMemShared;
    struct IPerlMem	    m_hostperlMemParse;
    struct IPerlEnv	    m_hostperlEnv;
    struct IPerlStdIO	    m_hostperlStdIO;
    struct IPerlLIO	    m_hostperlLIO;
    struct IPerlDir	    m_hostperlDir;
    struct IPerlSock	    m_hostperlSock;
    struct IPerlProc	    m_hostperlProc;

    struct IPerlMem*	    m_pHostperlMem;
    struct IPerlMem*	    m_pHostperlMemShared;
    struct IPerlMem*	    m_pHostperlMemParse;
    struct IPerlEnv*	    m_pHostperlEnv;
    struct IPerlStdIO*	    m_pHostperlStdIO;
    struct IPerlLIO*	    m_pHostperlLIO;
    struct IPerlDir*	    m_pHostperlDir;
    struct IPerlSock*	    m_pHostperlSock;
    struct IPerlProc*	    m_pHostperlProc;

protected:

    VMem*   m_pVMem;
    VMem*   m_pVMemShared;
    VMem*   m_pVMemParse;
};


#define STRUCT2PTR(x, y) (CPerlHost*)(((LPBYTE)x)-offsetof(CPerlHost, y))

inline CPerlHost* IPerlMem2Host(struct IPerlMem* piPerl)
{
    return STRUCT2PTR(piPerl, m_hostperlMem);
}

inline CPerlHost* IPerlMemShared2Host(struct IPerlMem* piPerl)
{
    return STRUCT2PTR(piPerl, m_hostperlMemShared);
}

inline CPerlHost* IPerlMemParse2Host(struct IPerlMem* piPerl)
{
    return STRUCT2PTR(piPerl, m_hostperlMemParse);
}

inline CPerlHost* IPerlEnv2Host(struct IPerlEnv* piPerl)
{
    return STRUCT2PTR(piPerl, m_hostperlEnv);
}

inline CPerlHost* IPerlStdIO2Host(struct IPerlStdIO* piPerl)
{
    return STRUCT2PTR(piPerl, m_hostperlStdIO);
}

inline CPerlHost* IPerlLIO2Host(struct IPerlLIO* piPerl)
{
    return STRUCT2PTR(piPerl, m_hostperlLIO);
}

inline CPerlHost* IPerlDir2Host(struct IPerlDir* piPerl)
{
    return STRUCT2PTR(piPerl, m_hostperlDir);
}

inline CPerlHost* IPerlSock2Host(struct IPerlSock* piPerl)
{
    return STRUCT2PTR(piPerl, m_hostperlSock);
}

inline CPerlHost* IPerlProc2Host(struct IPerlProc* piPerl)
{
    return STRUCT2PTR(piPerl, m_hostperlProc);
}


#undef IPERL2HOST
#define IPERL2HOST(x) IPerlMem2Host(x)


/* IPerlMem */
void*
PerlMemMalloc(struct IPerlMem* piPerl, size_t size)
{
    return IPERL2HOST(piPerl)->Malloc(size);
}
void*
PerlMemRealloc(struct IPerlMem* piPerl, void* ptr, size_t size)
{
    return IPERL2HOST(piPerl)->Realloc(ptr, size);
}
void
PerlMemFree(struct IPerlMem* piPerl, void* ptr)
{
    IPERL2HOST(piPerl)->Free(ptr);
}
void*
PerlMemCalloc(struct IPerlMem* piPerl, size_t num, size_t size)
{
    return IPERL2HOST(piPerl)->Calloc(num, size);
}


struct IPerlMem perlMem =
{
    PerlMemMalloc,
    PerlMemRealloc,
    PerlMemFree,
    PerlMemCalloc,
};

#undef IPERL2HOST
#define IPERL2HOST(x) IPerlMemShared2Host(x)

/* IPerlMemShared */
void*
PerlMemSharedMalloc(struct IPerlMem* piPerl, size_t size)
{
    return IPERL2HOST(piPerl)->MallocShared(size);
}
void*
PerlMemSharedRealloc(struct IPerlMem* piPerl, void* ptr, size_t size)
{
    return IPERL2HOST(piPerl)->ReallocShared(ptr, size);
}
void
PerlMemSharedFree(struct IPerlMem* piPerl, void* ptr)
{
    IPERL2HOST(piPerl)->FreeShared(ptr);
}
void*
PerlMemSharedCalloc(struct IPerlMem* piPerl, size_t num, size_t size)
{
    return IPERL2HOST(piPerl)->CallocShared(num, size);
}


struct IPerlMem perlMemShared =
{
    PerlMemSharedMalloc,
    PerlMemSharedRealloc,
    PerlMemSharedFree,
    PerlMemSharedCalloc,
};

#undef IPERL2HOST
#define IPERL2HOST(x) IPerlMemParse2Host(x)

/* IPerlMemParse */
void*
PerlMemParseMalloc(struct IPerlMem* piPerl, size_t size)
{
    return IPERL2HOST(piPerl)->MallocParse(size);
}
void*
PerlMemParseRealloc(struct IPerlMem* piPerl, void* ptr, size_t size)
{
    return IPERL2HOST(piPerl)->ReallocParse(ptr, size);
}
void
PerlMemParseFree(struct IPerlMem* piPerl, void* ptr)
{
    IPERL2HOST(piPerl)->FreeParse(ptr);
}
void*
PerlMemParseCalloc(struct IPerlMem* piPerl, size_t num, size_t size)
{
    return IPERL2HOST(piPerl)->CallocParse(num, size);
}


struct IPerlMem perlMemParse =
{
    PerlMemParseMalloc,
    PerlMemParseRealloc,
    PerlMemParseFree,
    PerlMemParseCalloc,
};


#undef IPERL2HOST
#define IPERL2HOST(x) IPerlEnv2Host(x)

/* IPerlEnv */
char*
PerlEnvGetenv(struct IPerlEnv* piPerl, const char *varname)
{
    return IPERL2HOST(piPerl)->Getenv(varname);
};

int
PerlEnvPutenv(struct IPerlEnv* piPerl, const char *envstring)
{
    return IPERL2HOST(piPerl)->Putenv(envstring);
};

char*
PerlEnvGetenv_len(struct IPerlEnv* piPerl, const char* varname, unsigned long* len)
{
    return IPERL2HOST(piPerl)->Getenv(varname, len);
}

int
PerlEnvUname(struct IPerlEnv* piPerl, struct utsname *name)
{
    return nw_uname(name);
}

void
PerlEnvClearenv(struct IPerlEnv* piPerl)
{
	// If removed, compilation fails while compiling CGI2Perl.
}

void*
PerlEnvGetChildenv(struct IPerlEnv* piPerl)
{
	// If removed, compilation fails while compiling CGI2Perl.
	return NULL;
}

void
PerlEnvFreeChildenv(struct IPerlEnv* piPerl, void* childEnv)
{
	// If removed, compilation fails while compiling CGI2Perl.
}

char*
PerlEnvGetChilddir(struct IPerlEnv* piPerl)
{
	// If removed, compilation fails while compiling CGI2Perl.
	return NULL;
}

void
PerlEnvFreeChilddir(struct IPerlEnv* piPerl, char* childDir)
{
	// If removed, compilation fails while compiling CGI2Perl.
}

struct IPerlEnv perlEnv = 
{
    PerlEnvGetenv,
    PerlEnvPutenv,
    PerlEnvGetenv_len,
    PerlEnvUname,
    PerlEnvClearenv,
    PerlEnvGetChildenv,
    PerlEnvFreeChildenv,
    PerlEnvGetChilddir,
    PerlEnvFreeChilddir,
};

#undef IPERL2HOST
#define IPERL2HOST(x) IPerlStdIO2Host(x)

/* PerlStdIO */
PerlIO*
PerlStdIOStdin(struct IPerlStdIO* piPerl)
{
    return (PerlIO*)nw_stdin();
}

PerlIO*
PerlStdIOStdout(struct IPerlStdIO* piPerl)
{
    return (PerlIO*)nw_stdout();
}

PerlIO*
PerlStdIOStderr(struct IPerlStdIO* piPerl)
{
    return (PerlIO*)nw_stderr();
}

PerlIO*
PerlStdIOOpen(struct IPerlStdIO* piPerl, const char *path, const char *mode)
{
    return (PerlIO*)nw_fopen(path, mode);
}

int
PerlStdIOClose(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    return nw_fclose(((FILE*)pf));
}

int
PerlStdIOEof(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    return nw_feof((FILE*)pf);
}

int
PerlStdIOError(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    return nw_ferror((FILE*)pf);
}

void
PerlStdIOClearerr(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    nw_clearerr((FILE*)pf);
}

int
PerlStdIOGetc(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    return nw_getc((FILE*)pf);
}

STDCHAR*
PerlStdIOGetBase(struct IPerlStdIO* piPerl, PerlIO* pf)
{
#ifdef FILE_base
    FILE *f = (FILE*)pf;
    return FILE_base(f);
#else
    return NULL;
#endif
}

int
PerlStdIOGetBufsiz(struct IPerlStdIO* piPerl, PerlIO* pf)
{
#ifdef FILE_bufsiz
    FILE *f = (FILE*)pf;
    return FILE_bufsiz(f);
#else
    return (-1);
#endif
}

int
PerlStdIOGetCnt(struct IPerlStdIO* piPerl, PerlIO* pf)
{
#ifdef USE_STDIO_PTR
    FILE *f = (FILE*)pf;
    return FILE_cnt(f);
#else
    return (-1);
#endif
}

STDCHAR*
PerlStdIOGetPtr(struct IPerlStdIO* piPerl, PerlIO* pf)
{
#ifdef USE_STDIO_PTR
    FILE *f = (FILE*)pf;
    return FILE_ptr(f);
#else
    return NULL;
#endif
}

char*
PerlStdIOGets(struct IPerlStdIO* piPerl, PerlIO* pf, char* s, int n)
{
    return nw_fgets(s, n, (FILE*)pf);
}

int
PerlStdIOPutc(struct IPerlStdIO* piPerl, PerlIO* pf, int c)
{
    return nw_fputc(c, (FILE*)pf);
}

int
PerlStdIOPuts(struct IPerlStdIO* piPerl, PerlIO* pf, const char *s)
{
    return nw_fputs(s, (FILE*)pf);
}

int
PerlStdIOFlush(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    return nw_fflush((FILE*)pf);
}

int
PerlStdIOUngetc(struct IPerlStdIO* piPerl, int c, PerlIO* pf)	//(J)
{
    return nw_ungetc(c, (FILE*)pf);
}

int
PerlStdIOFileno(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    return nw_fileno((FILE*)pf);
}

PerlIO*
PerlStdIOFdopen(struct IPerlStdIO* piPerl, int fd, const char *mode)
{
    return (PerlIO*)nw_fdopen(fd, mode);
}

PerlIO*
PerlStdIOReopen(struct IPerlStdIO* piPerl, const char*path, const char*mode, PerlIO* pf)
{
    return (PerlIO*)nw_freopen(path, mode, (FILE*)pf);
}

SSize_t
PerlStdIORead(struct IPerlStdIO* piPerl, void *buffer, Size_t size, Size_t dummy, PerlIO* pf)
{
    return nw_fread(buffer, 1, size, (FILE*)pf);
}

SSize_t
PerlStdIOWrite(struct IPerlStdIO* piPerl, const void *buffer, Size_t size, Size_t dummy, PerlIO* pf)
//PerlStdIOWrite(struct IPerlStdIO* piPerl, PerlIO* pf, const void *buffer, Size_t size) 
{
    return nw_fwrite(buffer, 1, size, (FILE*)pf);
}

void
PerlStdIOSetBuf(struct IPerlStdIO* piPerl, PerlIO* pf, char* buffer)
{
    nw_setbuf((FILE*)pf, buffer);
}

int
PerlStdIOSetVBuf(struct IPerlStdIO* piPerl, PerlIO* pf, char* buffer, int type, Size_t size)
{
    return nw_setvbuf((FILE*)pf, buffer, type, size);
}

void
PerlStdIOSetCnt(struct IPerlStdIO* piPerl, PerlIO* pf, int n)
{
#ifdef STDIO_CNT_LVALUE
    FILE *f = (FILE*)pf;
    FILE_cnt(f) = n;
#endif
}

void
PerlStdIOSetPtrCnt(struct IPerlStdIO* piPerl, PerlIO* pf, STDCHAR * ptr, int n)
{
#ifdef STDIO_PTR_LVALUE
    FILE *f = (FILE*)pf;
    FILE_ptr(f) = ptr;
    FILE_cnt(f) = n;
#endif
}

void
PerlStdIOSetlinebuf(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    nw_setvbuf((FILE*)pf, NULL, _IOLBF, 0);
}

int
PerlStdIOPrintf(struct IPerlStdIO* piPerl, PerlIO* pf, const char *format,...)
{
    va_list(arglist);
    va_start(arglist, format);
    return nw_vfprintf((FILE*)pf, format, arglist);
}

int
PerlStdIOVprintf(struct IPerlStdIO* piPerl, PerlIO* pf, const char *format, va_list arglist)
{
    return nw_vfprintf((FILE*)pf, format, arglist);
}

long
PerlStdIOTell(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    return nw_ftell((FILE*)pf);
}

int
PerlStdIOSeek(struct IPerlStdIO* piPerl, PerlIO* pf, off_t offset, int origin)
{
    return nw_fseek((FILE*)pf, offset, origin);
}

void
PerlStdIORewind(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    nw_rewind((FILE*)pf);
}

PerlIO*
PerlStdIOTmpfile(struct IPerlStdIO* piPerl)
{
    return (PerlIO*)nw_tmpfile();
}

int
PerlStdIOGetpos(struct IPerlStdIO* piPerl, PerlIO* pf, Fpos_t *p)
{
    return nw_fgetpos((FILE*)pf, p);
}

int
PerlStdIOSetpos(struct IPerlStdIO* piPerl, PerlIO* pf, const Fpos_t *p)
{
    return nw_fsetpos((FILE*)pf, p);
}

void
PerlStdIOInit(struct IPerlStdIO* piPerl)
{
	// If removed, compilation error occurs.
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

PerlIO*
PerlStdIOFdupopen(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    PerlIO* pfdup=NULL;
    fpos_t pos=0;
    char mode[3]={'\0'};
    int fileno = nw_dup(nw_fileno((FILE*)pf));

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
    pfdup = (PerlIO*)nw_fdopen(fileno, mode);

    /* move the file pointer to the same position */
    if (!fgetpos((FILE*)pf, &pos)) {
	fsetpos((FILE*)pfdup, &pos);
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
    PerlStdIOSetPtrCnt,
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


#undef IPERL2HOST
#define IPERL2HOST(x) IPerlLIO2Host(x)

/* IPerlLIO */
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
	// If removed, compilation error occurs.
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
	// If removed, compilation error occurs.
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


#undef IPERL2HOST
#define IPERL2HOST(x) IPerlDir2Host(x)

/* IPerlDIR */
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


/* IPerlSock */
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
    return nw_inet_ntoa(in);
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
    dTHX;	// (J) dTHXo
    Perl_croak(aTHX_ "socketpair not implemented!\n");
    return 0;
}

int
PerlSockIoctlsocket(struct IPerlSock* piPerl, SOCKET s, long cmd, u_long *argp)
{
	dTHX;	// (J) dTHXo
    Perl_croak(aTHX_ "ioctlsocket not implemented!\n");
	return 0;
}

struct IPerlSock perlSock =
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
	//Following commented by sgp bcos of comiplation error too many initializers (E279)
//    PerlSockClosesocket,
};


/* IPerlProc */

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
	//dJMPENV;
	JMPENV_JUMP(2);
}

void
PerlProc_Exit(struct IPerlProc* piPerl, int status)
{
//    _exit(status);
	dTHX;
	//dJMPENV;
	JMPENV_JUMP(2);
}

int
PerlProcExecl(struct IPerlProc* piPerl, const char *cmdname, const char *arg0, const char *arg1, const char *arg2, const char *arg3)
{
	// If removed, compilation error occurs.
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
	// If removed, compilation error occurs.
	return 0;
}

uid_t
PerlProcGeteuid(struct IPerlProc* piPerl)
{
	// If removed, compilation error occurs.
	return 0;
}

gid_t
PerlProcGetgid(struct IPerlProc* piPerl)
{
	// If removed, compilation error occurs.
	return 0;
}

gid_t
PerlProcGetegid(struct IPerlProc* piPerl)
{
	// If removed, compilation error occurs.
	return 0;
}

char *
PerlProcGetlogin(struct IPerlProc* piPerl)
{
	// If removed, compilation error occurs.
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
    dTHX;	// (J) dTHXo 
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
    dTHX;	// (J) dTHXo 
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
	// If removed, compilation error occurs.
	return 0;
}

int
PerlProcSetgid(struct IPerlProc* piPerl, gid_t g)
{
	// If removed, compilation error occurs.
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
	// If removed, compilation error occurs.
    return 0;
}

int
PerlProcFork(struct IPerlProc* piPerl)
{
	// If removed, compilation error occurs.
	return 0;
}

int
PerlProcGetpid(struct IPerlProc* piPerl)
{
    return nw_getpid();
}

BOOL
PerlProcDoCmd(struct IPerlProc* piPerl, char *cmd)
{
    do_spawn2(cmd, EXECF_EXEC);
    return FALSE;
}

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
/*    PerlProcDynaLoader,
    PerlProcGetOSError,
    PerlProcDoCmd,
    PerlProcSpawn,
    PerlProcSpawnvp,
    PerlProcASpawn,*/
};


/*
 * CPerlHost
 */

CPerlHost::CPerlHost(void)
{
    m_pVMem = new VMem();
    m_pVMemShared = new VMem();
    m_pVMemParse =  new VMem();

	memcpy(&m_hostperlMem, &perlMem, sizeof(perlMem));
	memcpy(&m_hostperlMemShared, &perlMemShared, sizeof(perlMemShared));
    memcpy(&m_hostperlMemParse, &perlMemParse, sizeof(perlMemParse));
    memcpy(&m_hostperlEnv, &perlEnv, sizeof(perlEnv));
    memcpy(&m_hostperlStdIO, &perlStdIO, sizeof(perlStdIO));
    memcpy(&m_hostperlLIO, &perlLIO, sizeof(perlLIO));
    memcpy(&m_hostperlDir, &perlDir, sizeof(perlDir));
    memcpy(&m_hostperlSock, &perlSock, sizeof(perlSock));
    memcpy(&m_hostperlProc, &perlProc, sizeof(perlProc));

    m_pHostperlMem	    = &m_hostperlMem;
    m_pHostperlMemShared    = &m_hostperlMemShared;
    m_pHostperlMemParse	    = &m_hostperlMemParse;
    m_pHostperlEnv	    = &m_hostperlEnv;
    m_pHostperlStdIO	    = &m_hostperlStdIO;
    m_pHostperlLIO	    = &m_hostperlLIO;
    m_pHostperlDir	    = &m_hostperlDir;
    m_pHostperlSock	    = &m_hostperlSock;
    m_pHostperlProc	    = &m_hostperlProc;
}

#define SETUPEXCHANGE(xptr, iptr, table) \
    STMT_START {				\
	if (xptr) {				\
	    iptr = *xptr;			\
	    *xptr = &table;			\
	}					\
	else {					\
	    iptr = &table;			\
	}					\
    } STMT_END

CPerlHost::CPerlHost(struct IPerlMem** ppMem, struct IPerlMem** ppMemShared,
		 struct IPerlMem** ppMemParse, struct IPerlEnv** ppEnv,
		 struct IPerlStdIO** ppStdIO, struct IPerlLIO** ppLIO,
		 struct IPerlDir** ppDir, struct IPerlSock** ppSock,
		 struct IPerlProc** ppProc)
{
    m_pVMem = new VMem();
    m_pVMemShared = new VMem();
    m_pVMemParse =  new VMem();

	memcpy(&m_hostperlMem, &perlMem, sizeof(perlMem));
    memcpy(&m_hostperlMemShared, &perlMemShared, sizeof(perlMemShared));
    memcpy(&m_hostperlMemParse, &perlMemParse, sizeof(perlMemParse));
    memcpy(&m_hostperlEnv, &perlEnv, sizeof(perlEnv));
    memcpy(&m_hostperlStdIO, &perlStdIO, sizeof(perlStdIO));
    memcpy(&m_hostperlLIO, &perlLIO, sizeof(perlLIO));
    memcpy(&m_hostperlDir, &perlDir, sizeof(perlDir));
    memcpy(&m_hostperlSock, &perlSock, sizeof(perlSock));
    memcpy(&m_hostperlProc, &perlProc, sizeof(perlProc));

    SETUPEXCHANGE(ppMem,	m_pHostperlMem,		m_hostperlMem);
    SETUPEXCHANGE(ppMemShared,	m_pHostperlMemShared,	m_hostperlMemShared);
    SETUPEXCHANGE(ppMemParse,	m_pHostperlMemParse,	m_hostperlMemParse);
    SETUPEXCHANGE(ppEnv,	m_pHostperlEnv,		m_hostperlEnv);
    SETUPEXCHANGE(ppStdIO,	m_pHostperlStdIO,	m_hostperlStdIO);
    SETUPEXCHANGE(ppLIO,	m_pHostperlLIO,		m_hostperlLIO);
    SETUPEXCHANGE(ppDir,	m_pHostperlDir,		m_hostperlDir);
    SETUPEXCHANGE(ppSock,	m_pHostperlSock,	m_hostperlSock);
    SETUPEXCHANGE(ppProc,	m_pHostperlProc,	m_hostperlProc);
}
#undef SETUPEXCHANGE

CPerlHost::CPerlHost(const CPerlHost& host)
{
	memcpy(&m_hostperlMem, &perlMem, sizeof(perlMem));
    memcpy(&m_hostperlMemShared, &perlMemShared, sizeof(perlMemShared));
    memcpy(&m_hostperlMemParse, &perlMemParse, sizeof(perlMemParse));
    memcpy(&m_hostperlEnv, &perlEnv, sizeof(perlEnv));
    memcpy(&m_hostperlStdIO, &perlStdIO, sizeof(perlStdIO));
    memcpy(&m_hostperlLIO, &perlLIO, sizeof(perlLIO));
    memcpy(&m_hostperlDir, &perlDir, sizeof(perlDir));
    memcpy(&m_hostperlSock, &perlSock, sizeof(perlSock));
    memcpy(&m_hostperlProc, &perlProc, sizeof(perlProc));

    m_pHostperlMem	    = &m_hostperlMem;
    m_pHostperlMemShared    = &m_hostperlMemShared;
    m_pHostperlMemParse	    = &m_hostperlMemParse;
    m_pHostperlEnv	    = &m_hostperlEnv;
    m_pHostperlStdIO	    = &m_hostperlStdIO;
    m_pHostperlLIO	    = &m_hostperlLIO;
    m_pHostperlDir	    = &m_hostperlDir;
    m_pHostperlSock	    = &m_hostperlSock;
    m_pHostperlProc	    = &m_hostperlProc;

}

CPerlHost::~CPerlHost(void)
{
	if ( m_pVMemParse ) delete m_pVMemParse;
	if ( m_pVMemShared ) delete m_pVMemShared;
	if ( m_pVMem ) delete m_pVMem;
}

char*
CPerlHost::Getenv(const char *varname)
{
	// getenv is always present. In old CLIB, it is implemented
	// to always return NULL. With java loaded on NW411, it will
	// return values set by envset. Is correctly implemented by
	// CLIB on MOAB.
	//
	return getenv(varname);
}

int
CPerlHost::Putenv(const char *envstring)
{
   	return(putenv(envstring));
}


#endif /* ___NWPerlHost_H___ */

