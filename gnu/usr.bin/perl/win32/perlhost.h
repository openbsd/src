/* perlhost.h
 *
 * (c) 1999 Microsoft Corporation. All rights reserved. 
 * Portions (c) 1999 ActiveState Tool Corp, http://www.ActiveState.com/
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#ifndef ___PerlHost_H___
#define ___PerlHost_H___

#include <signal.h>
#include "iperlsys.h"
#include "vmem.h"
#include "vdir.h"

#if !defined(PERL_OBJECT)
START_EXTERN_C
#endif
extern char *		g_win32_get_privlib(const char *pl);
extern char *		g_win32_get_sitelib(const char *pl);
extern char *		g_win32_get_vendorlib(const char *pl);
extern char *		g_getlogin(void);
extern int		do_spawn2(char *cmd, int exectype);
#if !defined(PERL_OBJECT)
END_EXTERN_C
#endif

#ifdef PERL_OBJECT
extern int		g_do_aspawn(void *vreally, void **vmark, void **vsp);
#define do_aspawn	g_do_aspawn
#endif

class CPerlHost
{
public:
    CPerlHost(void);
    CPerlHost(struct IPerlMem** ppMem, struct IPerlMem** ppMemShared,
		 struct IPerlMem** ppMemParse, struct IPerlEnv** ppEnv,
		 struct IPerlStdIO** ppStdIO, struct IPerlLIO** ppLIO,
		 struct IPerlDir** ppDir, struct IPerlSock** ppSock,
		 struct IPerlProc** ppProc);
    CPerlHost(CPerlHost& host);
    ~CPerlHost(void);

    static CPerlHost* IPerlMem2Host(struct IPerlMem* piPerl);
    static CPerlHost* IPerlMemShared2Host(struct IPerlMem* piPerl);
    static CPerlHost* IPerlMemParse2Host(struct IPerlMem* piPerl);
    static CPerlHost* IPerlEnv2Host(struct IPerlEnv* piPerl);
    static CPerlHost* IPerlStdIO2Host(struct IPerlStdIO* piPerl);
    static CPerlHost* IPerlLIO2Host(struct IPerlLIO* piPerl);
    static CPerlHost* IPerlDir2Host(struct IPerlDir* piPerl);
    static CPerlHost* IPerlSock2Host(struct IPerlSock* piPerl);
    static CPerlHost* IPerlProc2Host(struct IPerlProc* piPerl);

    BOOL PerlCreate(void);
    int PerlParse(int argc, char** argv, char** env);
    int PerlRun(void);
    void PerlDestroy(void);

/* IPerlMem */
    inline void* Malloc(size_t size) { return m_pVMem->Malloc(size); };
    inline void* Realloc(void* ptr, size_t size) { return m_pVMem->Realloc(ptr, size); };
    inline void Free(void* ptr) { m_pVMem->Free(ptr); };
    inline void* Calloc(size_t num, size_t size)
    {
	size_t count = num*size;
	void* lpVoid = Malloc(count);
	if (lpVoid)
	    ZeroMemory(lpVoid, count);
	return lpVoid;
    };
    inline void GetLock(void) { m_pVMem->GetLock(); };
    inline void FreeLock(void) { m_pVMem->FreeLock(); };
    inline int IsLocked(void) { return m_pVMem->IsLocked(); };

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
	if (lpVoid)
	    ZeroMemory(lpVoid, count);
	return lpVoid;
    };
    inline void GetLockShared(void) { m_pVMem->GetLock(); };
    inline void FreeLockShared(void) { m_pVMem->FreeLock(); };
    inline int IsLockedShared(void) { return m_pVMem->IsLocked(); };

/* IPerlMemParse */
    inline void* MallocParse(size_t size) { return m_pVMemParse->Malloc(size); };
    inline void* ReallocParse(void* ptr, size_t size) { return m_pVMemParse->Realloc(ptr, size); };
    inline void FreeParse(void* ptr) { m_pVMemParse->Free(ptr); };
    inline void* CallocParse(size_t num, size_t size)
    {
	size_t count = num*size;
	void* lpVoid = MallocParse(count);
	if (lpVoid)
	    ZeroMemory(lpVoid, count);
	return lpVoid;
    };
    inline void GetLockParse(void) { m_pVMem->GetLock(); };
    inline void FreeLockParse(void) { m_pVMem->FreeLock(); };
    inline int IsLockedParse(void) { return m_pVMem->IsLocked(); };

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
    void* CreateChildEnv(void) { return CreateLocalEnvironmentStrings(*m_pvDir); };
    void FreeChildEnv(void* pStr) { FreeLocalEnvironmentStrings((char*)pStr); };
    char* GetChildDir(void);
    void FreeChildDir(char* pStr);
    void Reset(void);
    void Clearenv(void);

    inline LPSTR GetIndex(DWORD &dwIndex)
    {
	if(dwIndex < m_dwEnvCount)
	{
	    ++dwIndex;
	    return m_lppEnvList[dwIndex-1];
	}
	return NULL;
    };

protected:
    LPSTR Find(LPCSTR lpStr);
    void Add(LPCSTR lpStr);

    LPSTR CreateLocalEnvironmentStrings(VDir &vDir);
    void FreeLocalEnvironmentStrings(LPSTR lpStr);
    LPSTR* Lookup(LPCSTR lpStr);
    DWORD CalculateEnvironmentSpace(void);

public:

/* IPerlDIR */
    virtual int Chdir(const char *dirname);

/* IPerllProc */
    void Abort(void);
    void Exit(int status);
    void _Exit(int status);
    int Execl(const char *cmdname, const char *arg0, const char *arg1, const char *arg2, const char *arg3);
    int Execv(const char *cmdname, const char *const *argv);
    int Execvp(const char *cmdname, const char *const *argv);

    inline VMem* GetMemShared(void) { m_pVMemShared->AddRef(); return m_pVMemShared; };
    inline VMem* GetMemParse(void) { m_pVMemParse->AddRef(); return m_pVMemParse; };
    inline VDir* GetDir(void) { return m_pvDir; };

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

    inline char* MapPathA(const char *pInName) { return m_pvDir->MapPathA(pInName); };
    inline WCHAR* MapPathW(const WCHAR *pInName) { return m_pvDir->MapPathW(pInName); };
protected:

    VDir*   m_pvDir;
    VMem*   m_pVMem;
    VMem*   m_pVMemShared;
    VMem*   m_pVMemParse;

    DWORD   m_dwEnvCount;
    LPSTR*  m_lppEnvList;
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

void
PerlMemGetLock(struct IPerlMem* piPerl)
{
    IPERL2HOST(piPerl)->GetLock();
}

void
PerlMemFreeLock(struct IPerlMem* piPerl)
{
    IPERL2HOST(piPerl)->FreeLock();
}

int
PerlMemIsLocked(struct IPerlMem* piPerl)
{
    return IPERL2HOST(piPerl)->IsLocked();
}

struct IPerlMem perlMem =
{
    PerlMemMalloc,
    PerlMemRealloc,
    PerlMemFree,
    PerlMemCalloc,
    PerlMemGetLock,
    PerlMemFreeLock,
    PerlMemIsLocked,
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

void
PerlMemSharedGetLock(struct IPerlMem* piPerl)
{
    IPERL2HOST(piPerl)->GetLockShared();
}

void
PerlMemSharedFreeLock(struct IPerlMem* piPerl)
{
    IPERL2HOST(piPerl)->FreeLockShared();
}

int
PerlMemSharedIsLocked(struct IPerlMem* piPerl)
{
    return IPERL2HOST(piPerl)->IsLockedShared();
}

struct IPerlMem perlMemShared =
{
    PerlMemSharedMalloc,
    PerlMemSharedRealloc,
    PerlMemSharedFree,
    PerlMemSharedCalloc,
    PerlMemSharedGetLock,
    PerlMemSharedFreeLock,
    PerlMemSharedIsLocked,
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

void
PerlMemParseGetLock(struct IPerlMem* piPerl)
{
    IPERL2HOST(piPerl)->GetLockParse();
}

void
PerlMemParseFreeLock(struct IPerlMem* piPerl)
{
    IPERL2HOST(piPerl)->FreeLockParse();
}

int
PerlMemParseIsLocked(struct IPerlMem* piPerl)
{
    return IPERL2HOST(piPerl)->IsLockedParse();
}

struct IPerlMem perlMemParse =
{
    PerlMemParseMalloc,
    PerlMemParseRealloc,
    PerlMemParseFree,
    PerlMemParseCalloc,
    PerlMemParseGetLock,
    PerlMemParseFreeLock,
    PerlMemParseIsLocked,
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
    return win32_uname(name);
}

void
PerlEnvClearenv(struct IPerlEnv* piPerl)
{
    IPERL2HOST(piPerl)->Clearenv();
}

void*
PerlEnvGetChildenv(struct IPerlEnv* piPerl)
{
    return IPERL2HOST(piPerl)->CreateChildEnv();
}

void
PerlEnvFreeChildenv(struct IPerlEnv* piPerl, void* childEnv)
{
    IPERL2HOST(piPerl)->FreeChildEnv(childEnv);
}

char*
PerlEnvGetChilddir(struct IPerlEnv* piPerl)
{
    return IPERL2HOST(piPerl)->GetChildDir();
}

void
PerlEnvFreeChilddir(struct IPerlEnv* piPerl, char* childDir)
{
    IPERL2HOST(piPerl)->FreeChildDir(childDir);
}

unsigned long
PerlEnvOsId(struct IPerlEnv* piPerl)
{
    return win32_os_id();
}

char*
PerlEnvLibPath(struct IPerlEnv* piPerl, const char *pl)
{
    return g_win32_get_privlib(pl);
}

char*
PerlEnvSiteLibPath(struct IPerlEnv* piPerl, const char *pl)
{
    return g_win32_get_sitelib(pl);
}

char*
PerlEnvVendorLibPath(struct IPerlEnv* piPerl, const char *pl)
{
    return g_win32_get_vendorlib(pl);
}

void
PerlEnvGetChildIO(struct IPerlEnv* piPerl, child_IO_table* ptr)
{
    win32_get_child_IO(ptr);
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
    PerlEnvOsId,
    PerlEnvLibPath,
    PerlEnvSiteLibPath,
    PerlEnvVendorLibPath,
    PerlEnvGetChildIO,
};

#undef IPERL2HOST
#define IPERL2HOST(x) IPerlStdIO2Host(x)

/* PerlStdIO */
PerlIO*
PerlStdIOStdin(struct IPerlStdIO* piPerl)
{
    return (PerlIO*)win32_stdin();
}

PerlIO*
PerlStdIOStdout(struct IPerlStdIO* piPerl)
{
    return (PerlIO*)win32_stdout();
}

PerlIO*
PerlStdIOStderr(struct IPerlStdIO* piPerl)
{
    return (PerlIO*)win32_stderr();
}

PerlIO*
PerlStdIOOpen(struct IPerlStdIO* piPerl, const char *path, const char *mode)
{
    return (PerlIO*)win32_fopen(path, mode);
}

int
PerlStdIOClose(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    return win32_fclose(((FILE*)pf));
}

int
PerlStdIOEof(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    return win32_feof((FILE*)pf);
}

int
PerlStdIOError(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    return win32_ferror((FILE*)pf);
}

void
PerlStdIOClearerr(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    win32_clearerr((FILE*)pf);
}

int
PerlStdIOGetc(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    return win32_getc((FILE*)pf);
}

char*
PerlStdIOGetBase(struct IPerlStdIO* piPerl, PerlIO* pf)
{
#ifdef FILE_base
    FILE *f = (FILE*)pf;
    return FILE_base(f);
#else
    return Nullch;
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

char*
PerlStdIOGetPtr(struct IPerlStdIO* piPerl, PerlIO* pf)
{
#ifdef USE_STDIO_PTR
    FILE *f = (FILE*)pf;
    return FILE_ptr(f);
#else
    return Nullch;
#endif
}

char*
PerlStdIOGets(struct IPerlStdIO* piPerl, PerlIO* pf, char* s, int n)
{
    return win32_fgets(s, n, (FILE*)pf);
}

int
PerlStdIOPutc(struct IPerlStdIO* piPerl, PerlIO* pf, int c)
{
    return win32_fputc(c, (FILE*)pf);
}

int
PerlStdIOPuts(struct IPerlStdIO* piPerl, PerlIO* pf, const char *s)
{
    return win32_fputs(s, (FILE*)pf);
}

int
PerlStdIOFlush(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    return win32_fflush((FILE*)pf);
}

int
PerlStdIOUngetc(struct IPerlStdIO* piPerl, PerlIO* pf,int c)
{
    return win32_ungetc(c, (FILE*)pf);
}

int
PerlStdIOFileno(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    return win32_fileno((FILE*)pf);
}

PerlIO*
PerlStdIOFdopen(struct IPerlStdIO* piPerl, int fd, const char *mode)
{
    return (PerlIO*)win32_fdopen(fd, mode);
}

PerlIO*
PerlStdIOReopen(struct IPerlStdIO* piPerl, const char*path, const char*mode, PerlIO* pf)
{
    return (PerlIO*)win32_freopen(path, mode, (FILE*)pf);
}

SSize_t
PerlStdIORead(struct IPerlStdIO* piPerl, PerlIO* pf, void *buffer, Size_t size)
{
    return win32_fread(buffer, 1, size, (FILE*)pf);
}

SSize_t
PerlStdIOWrite(struct IPerlStdIO* piPerl, PerlIO* pf, const void *buffer, Size_t size)
{
    return win32_fwrite(buffer, 1, size, (FILE*)pf);
}

void
PerlStdIOSetBuf(struct IPerlStdIO* piPerl, PerlIO* pf, char* buffer)
{
    win32_setbuf((FILE*)pf, buffer);
}

int
PerlStdIOSetVBuf(struct IPerlStdIO* piPerl, PerlIO* pf, char* buffer, int type, Size_t size)
{
    return win32_setvbuf((FILE*)pf, buffer, type, size);
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
PerlStdIOSetPtrCnt(struct IPerlStdIO* piPerl, PerlIO* pf, char * ptr, int n)
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
    win32_setvbuf((FILE*)pf, NULL, _IOLBF, 0);
}

int
PerlStdIOPrintf(struct IPerlStdIO* piPerl, PerlIO* pf, const char *format,...)
{
    va_list(arglist);
    va_start(arglist, format);
    return win32_vfprintf((FILE*)pf, format, arglist);
}

int
PerlStdIOVprintf(struct IPerlStdIO* piPerl, PerlIO* pf, const char *format, va_list arglist)
{
    return win32_vfprintf((FILE*)pf, format, arglist);
}

long
PerlStdIOTell(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    return win32_ftell((FILE*)pf);
}

int
PerlStdIOSeek(struct IPerlStdIO* piPerl, PerlIO* pf, off_t offset, int origin)
{
    return win32_fseek((FILE*)pf, offset, origin);
}

void
PerlStdIORewind(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    win32_rewind((FILE*)pf);
}

PerlIO*
PerlStdIOTmpfile(struct IPerlStdIO* piPerl)
{
    return (PerlIO*)win32_tmpfile();
}

int
PerlStdIOGetpos(struct IPerlStdIO* piPerl, PerlIO* pf, Fpos_t *p)
{
    return win32_fgetpos((FILE*)pf, p);
}

int
PerlStdIOSetpos(struct IPerlStdIO* piPerl, PerlIO* pf, const Fpos_t *p)
{
    return win32_fsetpos((FILE*)pf, p);
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
    return win32_open_osfhandle(osfhandle, flags);
}

int
PerlStdIOGetOSfhandle(struct IPerlStdIO* piPerl, int filenum)
{
    return win32_get_osfhandle(filenum);
}

PerlIO*
PerlStdIOFdupopen(struct IPerlStdIO* piPerl, PerlIO* pf)
{
    PerlIO* pfdup;
    fpos_t pos;
    char mode[3];
    int fileno = win32_dup(win32_fileno((FILE*)pf));

    /* open the file in the same mode */
#ifdef __BORLANDC__
    if(((FILE*)pf)->flags & _F_READ) {
	mode[0] = 'r';
	mode[1] = 0;
    }
    else if(((FILE*)pf)->flags & _F_WRIT) {
	mode[0] = 'a';
	mode[1] = 0;
    }
    else if(((FILE*)pf)->flags & _F_RDWR) {
	mode[0] = 'r';
	mode[1] = '+';
	mode[2] = 0;
    }
#else
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
#endif

    /* it appears that the binmode is attached to the 
     * file descriptor so binmode files will be handled
     * correctly
     */
    pfdup = (PerlIO*)win32_fdopen(fileno, mode);

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
    return win32_access(path, mode);
}

int
PerlLIOChmod(struct IPerlLIO* piPerl, const char *filename, int pmode)
{
    return win32_chmod(filename, pmode);
}

int
PerlLIOChown(struct IPerlLIO* piPerl, const char *filename, uid_t owner, gid_t group)
{
    return chown(filename, owner, group);
}

int
PerlLIOChsize(struct IPerlLIO* piPerl, int handle, long size)
{
    return chsize(handle, size);
}

int
PerlLIOClose(struct IPerlLIO* piPerl, int handle)
{
    return win32_close(handle);
}

int
PerlLIODup(struct IPerlLIO* piPerl, int handle)
{
    return win32_dup(handle);
}

int
PerlLIODup2(struct IPerlLIO* piPerl, int handle1, int handle2)
{
    return win32_dup2(handle1, handle2);
}

int
PerlLIOFlock(struct IPerlLIO* piPerl, int fd, int oper)
{
    return win32_flock(fd, oper);
}

int
PerlLIOFileStat(struct IPerlLIO* piPerl, int handle, struct stat *buffer)
{
    return win32_fstat(handle, buffer);
}

int
PerlLIOIOCtl(struct IPerlLIO* piPerl, int i, unsigned int u, char *data)
{
    return win32_ioctlsocket((SOCKET)i, (long)u, (u_long*)data);
}

int
PerlLIOIsatty(struct IPerlLIO* piPerl, int fd)
{
    return isatty(fd);
}

int
PerlLIOLink(struct IPerlLIO* piPerl, const char*oldname, const char *newname)
{
    return win32_link(oldname, newname);
}

long
PerlLIOLseek(struct IPerlLIO* piPerl, int handle, long offset, int origin)
{
    return win32_lseek(handle, offset, origin);
}

int
PerlLIOLstat(struct IPerlLIO* piPerl, const char *path, struct stat *buffer)
{
    return win32_stat(path, buffer);
}

char*
PerlLIOMktemp(struct IPerlLIO* piPerl, char *Template)
{
    return mktemp(Template);
}

int
PerlLIOOpen(struct IPerlLIO* piPerl, const char *filename, int oflag)
{
    return win32_open(filename, oflag);
}

int
PerlLIOOpen3(struct IPerlLIO* piPerl, const char *filename, int oflag, int pmode)
{
    return win32_open(filename, oflag, pmode);
}

int
PerlLIORead(struct IPerlLIO* piPerl, int handle, void *buffer, unsigned int count)
{
    return win32_read(handle, buffer, count);
}

int
PerlLIORename(struct IPerlLIO* piPerl, const char *OldFileName, const char *newname)
{
    return win32_rename(OldFileName, newname);
}

int
PerlLIOSetmode(struct IPerlLIO* piPerl, int handle, int mode)
{
    return win32_setmode(handle, mode);
}

int
PerlLIONameStat(struct IPerlLIO* piPerl, const char *path, struct stat *buffer)
{
    return win32_stat(path, buffer);
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
    return win32_unlink(filename);
}

int
PerlLIOUtime(struct IPerlLIO* piPerl, char *filename, struct utimbuf *times)
{
    return win32_utime(filename, times);
}

int
PerlLIOWrite(struct IPerlLIO* piPerl, int handle, const void *buffer, unsigned int count)
{
    return win32_write(handle, buffer, count);
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
    return win32_mkdir(dirname, mode);
}

int
PerlDirChdir(struct IPerlDir* piPerl, const char *dirname)
{
    return IPERL2HOST(piPerl)->Chdir(dirname);
}

int
PerlDirRmdir(struct IPerlDir* piPerl, const char *dirname)
{
    return win32_rmdir(dirname);
}

int
PerlDirClose(struct IPerlDir* piPerl, DIR *dirp)
{
    return win32_closedir(dirp);
}

DIR*
PerlDirOpen(struct IPerlDir* piPerl, char *filename)
{
    return win32_opendir(filename);
}

struct direct *
PerlDirRead(struct IPerlDir* piPerl, DIR *dirp)
{
    return win32_readdir(dirp);
}

void
PerlDirRewind(struct IPerlDir* piPerl, DIR *dirp)
{
    win32_rewinddir(dirp);
}

void
PerlDirSeek(struct IPerlDir* piPerl, DIR *dirp, long loc)
{
    win32_seekdir(dirp, loc);
}

long
PerlDirTell(struct IPerlDir* piPerl, DIR *dirp)
{
    return win32_telldir(dirp);
}

char*
PerlDirMapPathA(struct IPerlDir* piPerl, const char* path)
{
    return IPERL2HOST(piPerl)->MapPathA(path);
}

WCHAR*
PerlDirMapPathW(struct IPerlDir* piPerl, const WCHAR* path)
{
    return IPERL2HOST(piPerl)->MapPathW(path);
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
    PerlDirMapPathA,
    PerlDirMapPathW,
};


/* IPerlSock */
u_long
PerlSockHtonl(struct IPerlSock* piPerl, u_long hostlong)
{
    return win32_htonl(hostlong);
}

u_short
PerlSockHtons(struct IPerlSock* piPerl, u_short hostshort)
{
    return win32_htons(hostshort);
}

u_long
PerlSockNtohl(struct IPerlSock* piPerl, u_long netlong)
{
    return win32_ntohl(netlong);
}

u_short
PerlSockNtohs(struct IPerlSock* piPerl, u_short netshort)
{
    return win32_ntohs(netshort);
}

SOCKET PerlSockAccept(struct IPerlSock* piPerl, SOCKET s, struct sockaddr* addr, int* addrlen)
{
    return win32_accept(s, addr, addrlen);
}

int
PerlSockBind(struct IPerlSock* piPerl, SOCKET s, const struct sockaddr* name, int namelen)
{
    return win32_bind(s, name, namelen);
}

int
PerlSockConnect(struct IPerlSock* piPerl, SOCKET s, const struct sockaddr* name, int namelen)
{
    return win32_connect(s, name, namelen);
}

void
PerlSockEndhostent(struct IPerlSock* piPerl)
{
    win32_endhostent();
}

void
PerlSockEndnetent(struct IPerlSock* piPerl)
{
    win32_endnetent();
}

void
PerlSockEndprotoent(struct IPerlSock* piPerl)
{
    win32_endprotoent();
}

void
PerlSockEndservent(struct IPerlSock* piPerl)
{
    win32_endservent();
}

struct hostent*
PerlSockGethostbyaddr(struct IPerlSock* piPerl, const char* addr, int len, int type)
{
    return win32_gethostbyaddr(addr, len, type);
}

struct hostent*
PerlSockGethostbyname(struct IPerlSock* piPerl, const char* name)
{
    return win32_gethostbyname(name);
}

struct hostent*
PerlSockGethostent(struct IPerlSock* piPerl)
{
    dTHXo;
    Perl_croak(aTHX_ "gethostent not implemented!\n");
    return NULL;
}

int
PerlSockGethostname(struct IPerlSock* piPerl, char* name, int namelen)
{
    return win32_gethostname(name, namelen);
}

struct netent *
PerlSockGetnetbyaddr(struct IPerlSock* piPerl, long net, int type)
{
    return win32_getnetbyaddr(net, type);
}

struct netent *
PerlSockGetnetbyname(struct IPerlSock* piPerl, const char *name)
{
    return win32_getnetbyname((char*)name);
}

struct netent *
PerlSockGetnetent(struct IPerlSock* piPerl)
{
    return win32_getnetent();
}

int PerlSockGetpeername(struct IPerlSock* piPerl, SOCKET s, struct sockaddr* name, int* namelen)
{
    return win32_getpeername(s, name, namelen);
}

struct protoent*
PerlSockGetprotobyname(struct IPerlSock* piPerl, const char* name)
{
    return win32_getprotobyname(name);
}

struct protoent*
PerlSockGetprotobynumber(struct IPerlSock* piPerl, int number)
{
    return win32_getprotobynumber(number);
}

struct protoent*
PerlSockGetprotoent(struct IPerlSock* piPerl)
{
    return win32_getprotoent();
}

struct servent*
PerlSockGetservbyname(struct IPerlSock* piPerl, const char* name, const char* proto)
{
    return win32_getservbyname(name, proto);
}

struct servent*
PerlSockGetservbyport(struct IPerlSock* piPerl, int port, const char* proto)
{
    return win32_getservbyport(port, proto);
}

struct servent*
PerlSockGetservent(struct IPerlSock* piPerl)
{
    return win32_getservent();
}

int
PerlSockGetsockname(struct IPerlSock* piPerl, SOCKET s, struct sockaddr* name, int* namelen)
{
    return win32_getsockname(s, name, namelen);
}

int
PerlSockGetsockopt(struct IPerlSock* piPerl, SOCKET s, int level, int optname, char* optval, int* optlen)
{
    return win32_getsockopt(s, level, optname, optval, optlen);
}

unsigned long
PerlSockInetAddr(struct IPerlSock* piPerl, const char* cp)
{
    return win32_inet_addr(cp);
}

char*
PerlSockInetNtoa(struct IPerlSock* piPerl, struct in_addr in)
{
    return win32_inet_ntoa(in);
}

int
PerlSockListen(struct IPerlSock* piPerl, SOCKET s, int backlog)
{
    return win32_listen(s, backlog);
}

int
PerlSockRecv(struct IPerlSock* piPerl, SOCKET s, char* buffer, int len, int flags)
{
    return win32_recv(s, buffer, len, flags);
}

int
PerlSockRecvfrom(struct IPerlSock* piPerl, SOCKET s, char* buffer, int len, int flags, struct sockaddr* from, int* fromlen)
{
    return win32_recvfrom(s, buffer, len, flags, from, fromlen);
}

int
PerlSockSelect(struct IPerlSock* piPerl, int nfds, char* readfds, char* writefds, char* exceptfds, const struct timeval* timeout)
{
    return win32_select(nfds, (Perl_fd_set*)readfds, (Perl_fd_set*)writefds, (Perl_fd_set*)exceptfds, timeout);
}

int
PerlSockSend(struct IPerlSock* piPerl, SOCKET s, const char* buffer, int len, int flags)
{
    return win32_send(s, buffer, len, flags);
}

int
PerlSockSendto(struct IPerlSock* piPerl, SOCKET s, const char* buffer, int len, int flags, const struct sockaddr* to, int tolen)
{
    return win32_sendto(s, buffer, len, flags, to, tolen);
}

void
PerlSockSethostent(struct IPerlSock* piPerl, int stayopen)
{
    win32_sethostent(stayopen);
}

void
PerlSockSetnetent(struct IPerlSock* piPerl, int stayopen)
{
    win32_setnetent(stayopen);
}

void
PerlSockSetprotoent(struct IPerlSock* piPerl, int stayopen)
{
    win32_setprotoent(stayopen);
}

void
PerlSockSetservent(struct IPerlSock* piPerl, int stayopen)
{
    win32_setservent(stayopen);
}

int
PerlSockSetsockopt(struct IPerlSock* piPerl, SOCKET s, int level, int optname, const char* optval, int optlen)
{
    return win32_setsockopt(s, level, optname, optval, optlen);
}

int
PerlSockShutdown(struct IPerlSock* piPerl, SOCKET s, int how)
{
    return win32_shutdown(s, how);
}

SOCKET
PerlSockSocket(struct IPerlSock* piPerl, int af, int type, int protocol)
{
    return win32_socket(af, type, protocol);
}

int
PerlSockSocketpair(struct IPerlSock* piPerl, int domain, int type, int protocol, int* fds)
{
    dTHXo;
    Perl_croak(aTHX_ "socketpair not implemented!\n");
    return 0;
}

int
PerlSockClosesocket(struct IPerlSock* piPerl, SOCKET s)
{
    return win32_closesocket(s);
}

int
PerlSockIoctlsocket(struct IPerlSock* piPerl, SOCKET s, long cmd, u_long *argp)
{
    return win32_ioctlsocket(s, cmd, argp);
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
    PerlSockClosesocket,
};


/* IPerlProc */

#define EXECF_EXEC 1
#define EXECF_SPAWN 2

void
PerlProcAbort(struct IPerlProc* piPerl)
{
    win32_abort();
}

char *
PerlProcCrypt(struct IPerlProc* piPerl, const char* clear, const char* salt)
{
    return win32_crypt(clear, salt);
}

void
PerlProcExit(struct IPerlProc* piPerl, int status)
{
    exit(status);
}

void
PerlProc_Exit(struct IPerlProc* piPerl, int status)
{
    _exit(status);
}

int
PerlProcExecl(struct IPerlProc* piPerl, const char *cmdname, const char *arg0, const char *arg1, const char *arg2, const char *arg3)
{
    return execl(cmdname, arg0, arg1, arg2, arg3);
}

int
PerlProcExecv(struct IPerlProc* piPerl, const char *cmdname, const char *const *argv)
{
    return win32_execvp(cmdname, argv);
}

int
PerlProcExecvp(struct IPerlProc* piPerl, const char *cmdname, const char *const *argv)
{
    return win32_execvp(cmdname, argv);
}

uid_t
PerlProcGetuid(struct IPerlProc* piPerl)
{
    return getuid();
}

uid_t
PerlProcGeteuid(struct IPerlProc* piPerl)
{
    return geteuid();
}

gid_t
PerlProcGetgid(struct IPerlProc* piPerl)
{
    return getgid();
}

gid_t
PerlProcGetegid(struct IPerlProc* piPerl)
{
    return getegid();
}

char *
PerlProcGetlogin(struct IPerlProc* piPerl)
{
    return g_getlogin();
}

int
PerlProcKill(struct IPerlProc* piPerl, int pid, int sig)
{
    return win32_kill(pid, sig);
}

int
PerlProcKillpg(struct IPerlProc* piPerl, int pid, int sig)
{
    dTHXo;
    Perl_croak(aTHX_ "killpg not implemented!\n");
    return 0;
}

int
PerlProcPauseProc(struct IPerlProc* piPerl)
{
    return win32_sleep((32767L << 16) + 32767);
}

PerlIO*
PerlProcPopen(struct IPerlProc* piPerl, const char *command, const char *mode)
{
    dTHXo;
    PERL_FLUSHALL_FOR_CHILD;
    return (PerlIO*)win32_popen(command, mode);
}

int
PerlProcPclose(struct IPerlProc* piPerl, PerlIO *stream)
{
    return win32_pclose((FILE*)stream);
}

int
PerlProcPipe(struct IPerlProc* piPerl, int *phandles)
{
    return win32_pipe(phandles, 512, O_BINARY);
}

int
PerlProcSetuid(struct IPerlProc* piPerl, uid_t u)
{
    return setuid(u);
}

int
PerlProcSetgid(struct IPerlProc* piPerl, gid_t g)
{
    return setgid(g);
}

int
PerlProcSleep(struct IPerlProc* piPerl, unsigned int s)
{
    return win32_sleep(s);
}

int
PerlProcTimes(struct IPerlProc* piPerl, struct tms *timebuf)
{
    return win32_times(timebuf);
}

int
PerlProcWait(struct IPerlProc* piPerl, int *status)
{
    return win32_wait(status);
}

int
PerlProcWaitpid(struct IPerlProc* piPerl, int pid, int *status, int flags)
{
    return win32_waitpid(pid, status, flags);
}

Sighandler_t
PerlProcSignal(struct IPerlProc* piPerl, int sig, Sighandler_t subcode)
{
    return signal(sig, subcode);
}

#ifdef USE_ITHREADS
static THREAD_RET_TYPE
win32_start_child(LPVOID arg)
{
    PerlInterpreter *my_perl = (PerlInterpreter*)arg;
    GV *tmpgv;
    int status;
#ifdef PERL_OBJECT
    CPerlObj *pPerl = (CPerlObj*)my_perl;
#endif
#ifdef PERL_SYNC_FORK
    static long sync_fork_id = 0;
    long id = ++sync_fork_id;
#endif


    PERL_SET_THX(my_perl);

    /* set $$ to pseudo id */
#ifdef PERL_SYNC_FORK
    w32_pseudo_id = id;
#else
    w32_pseudo_id = GetCurrentThreadId();
    if (IsWin95()) {
	int pid = (int)w32_pseudo_id;
	if (pid < 0)
	    w32_pseudo_id = -pid;
    }
#endif
    if (tmpgv = gv_fetchpv("$", TRUE, SVt_PV))
	sv_setiv(GvSV(tmpgv), -(IV)w32_pseudo_id);
    hv_clear(PL_pidstatus);

    /* push a zero on the stack (we are the child) */
    {
	dSP;
	dTARGET;
	PUSHi(0);
	PUTBACK;
    }

    /* continue from next op */
    PL_op = PL_op->op_next;

    {
	dJMPENV;
	volatile int oldscope = PL_scopestack_ix;

restart:
	JMPENV_PUSH(status);
	switch (status) {
	case 0:
	    CALLRUNOPS(aTHX);
	    status = 0;
	    break;
	case 2:
	    while (PL_scopestack_ix > oldscope)
		LEAVE;
	    FREETMPS;
	    PL_curstash = PL_defstash;
	    if (PL_endav && !PL_minus_c)
		call_list(oldscope, PL_endav);
	    status = STATUS_NATIVE_EXPORT;
	    break;
	case 3:
	    if (PL_restartop) {
		POPSTACK_TO(PL_mainstack);
		PL_op = PL_restartop;
		PL_restartop = Nullop;
		goto restart;
	    }
	    PerlIO_printf(Perl_error_log, "panic: restartop\n");
	    FREETMPS;
	    status = 1;
	    break;
	}
	JMPENV_POP;

	/* XXX hack to avoid perl_destruct() freeing optree */
	PL_main_root = Nullop;
    }

    /* close the std handles to avoid fd leaks */
    {
	do_close(gv_fetchpv("STDIN", TRUE, SVt_PVIO), FALSE);
	do_close(gv_fetchpv("STDOUT", TRUE, SVt_PVIO), FALSE);
	do_close(gv_fetchpv("STDERR", TRUE, SVt_PVIO), FALSE);
    }

    /* destroy everything (waits for any pseudo-forked children) */
    perl_destruct(my_perl);
    perl_free(my_perl);

#ifdef PERL_SYNC_FORK
    return id;
#else
    return (DWORD)status;
#endif
}
#endif /* USE_ITHREADS */

int
PerlProcFork(struct IPerlProc* piPerl)
{
    dTHXo;
#ifdef USE_ITHREADS
    DWORD id;
    HANDLE handle;
    CPerlHost *h;

    if (w32_num_pseudo_children >= MAXIMUM_WAIT_OBJECTS) {
	errno = EAGAIN;
	return -1;
    }
    h = new CPerlHost(*(CPerlHost*)w32_internal_host);
    PerlInterpreter *new_perl = perl_clone_using((PerlInterpreter*)aTHXo, 1,
						 h->m_pHostperlMem,
						 h->m_pHostperlMemShared,
						 h->m_pHostperlMemParse,
						 h->m_pHostperlEnv,
						 h->m_pHostperlStdIO,
						 h->m_pHostperlLIO,
						 h->m_pHostperlDir,
						 h->m_pHostperlSock,
						 h->m_pHostperlProc
						 );
    new_perl->Isys_intern.internal_host = h;
#  ifdef PERL_SYNC_FORK
    id = win32_start_child((LPVOID)new_perl);
    PERL_SET_THX(aTHXo);
#  else
#    ifdef USE_RTL_THREAD_API
    handle = (HANDLE)_beginthreadex((void*)NULL, 0, win32_start_child,
				    (void*)new_perl, 0, (unsigned*)&id);
#    else
    handle = CreateThread(NULL, 0, win32_start_child,
			  (LPVOID)new_perl, 0, &id);
#    endif
    PERL_SET_THX(aTHXo);	/* XXX perl_clone*() set TLS */
    if (!handle) {
	errno = EAGAIN;
	return -1;
    }
    if (IsWin95()) {
	int pid = (int)id;
	if (pid < 0)
	    id = -pid;
    }
    w32_pseudo_child_handles[w32_num_pseudo_children] = handle;
    w32_pseudo_child_pids[w32_num_pseudo_children] = id;
    ++w32_num_pseudo_children;
#  endif
    return -(int)id;
#else
    Perl_croak(aTHX_ "fork() not implemented!\n");
    return -1;
#endif /* USE_ITHREADS */
}

int
PerlProcGetpid(struct IPerlProc* piPerl)
{
    return win32_getpid();
}

void*
PerlProcDynaLoader(struct IPerlProc* piPerl, const char* filename)
{
    return win32_dynaload(filename);
}

void
PerlProcGetOSError(struct IPerlProc* piPerl, SV* sv, DWORD dwErr)
{
    win32_str_os_error(sv, dwErr);
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
    return win32_spawnvp(mode, cmdname, argv);
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
    PerlProcDynaLoader,
    PerlProcGetOSError,
    PerlProcDoCmd,
    PerlProcSpawn,
    PerlProcSpawnvp,
    PerlProcASpawn,
};


/*
 * CPerlHost
 */

CPerlHost::CPerlHost(void)
{
    m_pvDir = new VDir();
    m_pVMem = new VMem();
    m_pVMemShared = new VMem();
    m_pVMemParse =  new VMem();

    m_pvDir->Init(NULL, m_pVMem);

    m_dwEnvCount = 0;
    m_lppEnvList = NULL;

    CopyMemory(&m_hostperlMem, &perlMem, sizeof(perlMem));
    CopyMemory(&m_hostperlMemShared, &perlMemShared, sizeof(perlMemShared));
    CopyMemory(&m_hostperlMemParse, &perlMemParse, sizeof(perlMemParse));
    CopyMemory(&m_hostperlEnv, &perlEnv, sizeof(perlEnv));
    CopyMemory(&m_hostperlStdIO, &perlStdIO, sizeof(perlStdIO));
    CopyMemory(&m_hostperlLIO, &perlLIO, sizeof(perlLIO));
    CopyMemory(&m_hostperlDir, &perlDir, sizeof(perlDir));
    CopyMemory(&m_hostperlSock, &perlSock, sizeof(perlSock));
    CopyMemory(&m_hostperlProc, &perlProc, sizeof(perlProc));

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
    m_pvDir = new VDir(0);
    m_pVMem = new VMem();
    m_pVMemShared = new VMem();
    m_pVMemParse =  new VMem();

    m_pvDir->Init(NULL, m_pVMem);

    m_dwEnvCount = 0;
    m_lppEnvList = NULL;

    CopyMemory(&m_hostperlMem, &perlMem, sizeof(perlMem));
    CopyMemory(&m_hostperlMemShared, &perlMemShared, sizeof(perlMemShared));
    CopyMemory(&m_hostperlMemParse, &perlMemParse, sizeof(perlMemParse));
    CopyMemory(&m_hostperlEnv, &perlEnv, sizeof(perlEnv));
    CopyMemory(&m_hostperlStdIO, &perlStdIO, sizeof(perlStdIO));
    CopyMemory(&m_hostperlLIO, &perlLIO, sizeof(perlLIO));
    CopyMemory(&m_hostperlDir, &perlDir, sizeof(perlDir));
    CopyMemory(&m_hostperlSock, &perlSock, sizeof(perlSock));
    CopyMemory(&m_hostperlProc, &perlProc, sizeof(perlProc));

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

CPerlHost::CPerlHost(CPerlHost& host)
{
    m_pVMem = new VMem();
    m_pVMemShared = host.GetMemShared();
    m_pVMemParse =  host.GetMemParse();

    /* duplicate directory info */
    m_pvDir = new VDir(0);
    m_pvDir->Init(host.GetDir(), m_pVMem);

    CopyMemory(&m_hostperlMem, &perlMem, sizeof(perlMem));
    CopyMemory(&m_hostperlMemShared, &perlMemShared, sizeof(perlMemShared));
    CopyMemory(&m_hostperlMemParse, &perlMemParse, sizeof(perlMemParse));
    CopyMemory(&m_hostperlEnv, &perlEnv, sizeof(perlEnv));
    CopyMemory(&m_hostperlStdIO, &perlStdIO, sizeof(perlStdIO));
    CopyMemory(&m_hostperlLIO, &perlLIO, sizeof(perlLIO));
    CopyMemory(&m_hostperlDir, &perlDir, sizeof(perlDir));
    CopyMemory(&m_hostperlSock, &perlSock, sizeof(perlSock));
    CopyMemory(&m_hostperlProc, &perlProc, sizeof(perlProc));
    m_pHostperlMem	    = &m_hostperlMem;
    m_pHostperlMemShared    = &m_hostperlMemShared;
    m_pHostperlMemParse	    = &m_hostperlMemParse;
    m_pHostperlEnv	    = &m_hostperlEnv;
    m_pHostperlStdIO	    = &m_hostperlStdIO;
    m_pHostperlLIO	    = &m_hostperlLIO;
    m_pHostperlDir	    = &m_hostperlDir;
    m_pHostperlSock	    = &m_hostperlSock;
    m_pHostperlProc	    = &m_hostperlProc;

    m_dwEnvCount = 0;
    m_lppEnvList = NULL;

    /* duplicate environment info */
    LPSTR lpPtr;
    DWORD dwIndex = 0;
    while(lpPtr = host.GetIndex(dwIndex))
	Add(lpPtr);
}

CPerlHost::~CPerlHost(void)
{
//  Reset();
    delete m_pvDir;
    m_pVMemParse->Release();
    m_pVMemShared->Release();
    m_pVMem->Release();
}

LPSTR
CPerlHost::Find(LPCSTR lpStr)
{
    LPSTR lpPtr;
    LPSTR* lppPtr = Lookup(lpStr);
    if(lppPtr != NULL) {
	for(lpPtr = *lppPtr; *lpPtr != '\0' && *lpPtr != '='; ++lpPtr)
	    ;

	if(*lpPtr == '=')
	    ++lpPtr;

	return lpPtr;
    }
    return NULL;
}

int
lookup(const void *arg1, const void *arg2)
{   // Compare strings
    char*ptr1, *ptr2;
    char c1,c2;

    ptr1 = *(char**)arg1;
    ptr2 = *(char**)arg2;
    for(;;) {
	c1 = *ptr1++;
	c2 = *ptr2++;
	if(c1 == '\0' || c1 == '=') {
	    if(c2 == '\0' || c2 == '=')
		break;

	    return -1; // string 1 < string 2
	}
	else if(c2 == '\0' || c2 == '=')
	    return 1; // string 1 > string 2
	else if(c1 != c2) {
	    c1 = toupper(c1);
	    c2 = toupper(c2);
	    if(c1 != c2) {
		if(c1 < c2)
		    return -1; // string 1 < string 2

		return 1; // string 1 > string 2
	    }
	}
    }
    return 0;
}

LPSTR*
CPerlHost::Lookup(LPCSTR lpStr)
{
    return (LPSTR*)bsearch(&lpStr, m_lppEnvList, m_dwEnvCount, sizeof(LPSTR), lookup);
}

int
compare(const void *arg1, const void *arg2)
{   // Compare strings
    char*ptr1, *ptr2;
    char c1,c2;

    ptr1 = *(char**)arg1;
    ptr2 = *(char**)arg2;
    for(;;) {
	c1 = *ptr1++;
	c2 = *ptr2++;
	if(c1 == '\0' || c1 == '=') {
	    if(c1 == c2)
		break;

	    return -1; // string 1 < string 2
	}
	else if(c2 == '\0' || c2 == '=')
	    return 1; // string 1 > string 2
	else if(c1 != c2) {
	    c1 = toupper(c1);
	    c2 = toupper(c2);
	    if(c1 != c2) {
		if(c1 < c2)
		    return -1; // string 1 < string 2
	    
		return 1; // string 1 > string 2
	    }
	}
    }
    return 0;
}

void
CPerlHost::Add(LPCSTR lpStr)
{
    dTHXo;
    char szBuffer[1024];
    LPSTR *lpPtr;
    int index, length = strlen(lpStr)+1;

    for(index = 0; lpStr[index] != '\0' && lpStr[index] != '='; ++index)
	szBuffer[index] = lpStr[index];

    szBuffer[index] = '\0';

    // replacing ?
    lpPtr = Lookup(szBuffer);
    if(lpPtr != NULL) {
	Renew(*lpPtr, length, char);
	strcpy(*lpPtr, lpStr);
    }
    else {
	++m_dwEnvCount;
	Renew(m_lppEnvList, m_dwEnvCount, LPSTR);
	New(1, m_lppEnvList[m_dwEnvCount-1], length, char);
	if(m_lppEnvList[m_dwEnvCount-1] != NULL) {
	    strcpy(m_lppEnvList[m_dwEnvCount-1], lpStr);
	    qsort(m_lppEnvList, m_dwEnvCount, sizeof(LPSTR), compare);
	}
	else
	    --m_dwEnvCount;
    }
}

DWORD
CPerlHost::CalculateEnvironmentSpace(void)
{
    DWORD index;
    DWORD dwSize = 0;
    for(index = 0; index < m_dwEnvCount; ++index)
	dwSize += strlen(m_lppEnvList[index]) + 1;

    return dwSize;
}

void
CPerlHost::FreeLocalEnvironmentStrings(LPSTR lpStr)
{
    dTHXo;
    Safefree(lpStr);
}

char*
CPerlHost::GetChildDir(void)
{
    dTHXo;
    int length;
    char* ptr;
    New(0, ptr, MAX_PATH+1, char);
    if(ptr) {
	m_pvDir->GetCurrentDirectoryA(MAX_PATH+1, ptr);
	length = strlen(ptr);
	if (length > 3) {
	    if ((ptr[length-1] == '\\') || (ptr[length-1] == '/'))
		ptr[length-1] = 0;
	}
    }
    return ptr;
}

void
CPerlHost::FreeChildDir(char* pStr)
{
    dTHXo;
    Safefree(pStr);
}

LPSTR
CPerlHost::CreateLocalEnvironmentStrings(VDir &vDir)
{
    dTHXo;
    LPSTR lpStr, lpPtr, lpEnvPtr, lpTmp, lpLocalEnv, lpAllocPtr;
    DWORD dwSize, dwEnvIndex;
    int nLength, compVal;

    // get the process environment strings
    lpAllocPtr = lpTmp = (LPSTR)GetEnvironmentStrings();

    // step over current directory stuff
    while(*lpTmp == '=')
	lpTmp += strlen(lpTmp) + 1;

    // save the start of the environment strings
    lpEnvPtr = lpTmp;
    for(dwSize = 1; *lpTmp != '\0'; lpTmp += strlen(lpTmp) + 1) {
	// calculate the size of the environment strings
	dwSize += strlen(lpTmp) + 1;
    }

    // add the size of current directories
    dwSize += vDir.CalculateEnvironmentSpace();

    // add the additional space used by changes made to the environment
    dwSize += CalculateEnvironmentSpace();

    New(1, lpStr, dwSize, char);
    lpPtr = lpStr;
    if(lpStr != NULL) {
	// build the local environment
	lpStr = vDir.BuildEnvironmentSpace(lpStr);

	dwEnvIndex = 0;
	lpLocalEnv = GetIndex(dwEnvIndex);
	while(*lpEnvPtr != '\0') {
	    if(!lpLocalEnv) {
		// all environment overrides have been added
		// so copy string into place
		strcpy(lpStr, lpEnvPtr);
		nLength = strlen(lpEnvPtr) + 1;
		lpStr += nLength;
		lpEnvPtr += nLength;
	    }
	    else {	
		// determine which string to copy next
		compVal = compare(&lpEnvPtr, &lpLocalEnv);
		if(compVal < 0) {
		    strcpy(lpStr, lpEnvPtr);
		    nLength = strlen(lpEnvPtr) + 1;
		    lpStr += nLength;
		    lpEnvPtr += nLength;
		}
		else {
		    char *ptr = strchr(lpLocalEnv, '=');
		    if(ptr && ptr[1]) {
			strcpy(lpStr, lpLocalEnv);
			lpStr += strlen(lpLocalEnv) + 1;
		    }
		    lpLocalEnv = GetIndex(dwEnvIndex);
		    if(compVal == 0) {
			// this string was replaced
			lpEnvPtr += strlen(lpEnvPtr) + 1;
		    }
		}
	    }
	}

	while(lpLocalEnv) {
	    // still have environment overrides to add
	    // so copy the strings into place
	    strcpy(lpStr, lpLocalEnv);
	    nLength = strlen(lpLocalEnv) + 1;
	    lpStr += nLength;
	    lpEnvPtr += nLength;
	    lpLocalEnv = GetIndex(dwEnvIndex);
	}

	// add final NULL
	*lpStr = '\0';
    }

    // release the process environment strings
    FreeEnvironmentStrings(lpAllocPtr);

    return lpPtr;
}

void
CPerlHost::Reset(void)
{
    dTHXo;
    if(m_lppEnvList != NULL) {
	for(DWORD index = 0; index < m_dwEnvCount; ++index) {
	    Safefree(m_lppEnvList[index]);
	    m_lppEnvList[index] = NULL;
	}
    }
    m_dwEnvCount = 0;
}

void
CPerlHost::Clearenv(void)
{
    char ch;
    LPSTR lpPtr, lpStr, lpEnvPtr;
    if(m_lppEnvList != NULL) {
	/* set every entry to an empty string */
	for(DWORD index = 0; index < m_dwEnvCount; ++index) {
	    char* ptr = strchr(m_lppEnvList[index], '=');
	    if(ptr) {
		*++ptr = 0;
	    }
	}
    }

    /* get the process environment strings */
    lpStr = lpEnvPtr = (LPSTR)GetEnvironmentStrings();

    /* step over current directory stuff */
    while(*lpStr == '=')
	lpStr += strlen(lpStr) + 1;

    while(*lpStr) {
	lpPtr = strchr(lpStr, '=');
	if(lpPtr) {
	    ch = *++lpPtr;
	    *lpPtr = 0;
	    Add(lpStr);
	    *lpPtr = ch;
	}
	lpStr += strlen(lpStr) + 1;
    }

    FreeEnvironmentStrings(lpEnvPtr);
}


char*
CPerlHost::Getenv(const char *varname)
{
    char* pEnv = Find(varname);
    if(pEnv == NULL) {
	pEnv = win32_getenv(varname);
    }
    else {
	if(!*pEnv)
	    pEnv = 0;
    }

    return pEnv;
}

int
CPerlHost::Putenv(const char *envstring)
{
    Add(envstring);
    return 0;
}

int
CPerlHost::Chdir(const char *dirname)
{
    dTHXo;
    int ret;
    if (USING_WIDE()) {
	WCHAR wBuffer[MAX_PATH];
	A2WHELPER(dirname, wBuffer, sizeof(wBuffer));
	ret = m_pvDir->SetCurrentDirectoryW(wBuffer);
    }
    else
	ret = m_pvDir->SetCurrentDirectoryA((char*)dirname);
    if(ret < 0) {
	errno = ENOENT;
    }
    return ret;
}

#endif /* ___PerlHost_H___ */
