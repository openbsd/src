/* perlhost.h
 *
 * (c) 1999 Microsoft Corporation. All rights reserved.
 * Portions (c) 1999 ActiveState Tool Corp, http://www.ActiveState.com/
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#define CHECK_HOST_INTERP

#ifndef ___PerlHost_H___
#define ___PerlHost_H___

#include <signal.h>
#include <wchar.h>
#include "iperlsys.h"

#include "vmem.h"

#define CRT_ALLOC_BASE
#include "vmem.h"
#undef CRT_ALLOC_BASE

#include "vdir.h"

#ifndef WC_NO_BEST_FIT_CHARS
#  define WC_NO_BEST_FIT_CHARS 0x00000400
#endif

START_EXTERN_C
extern char *	g_getlogin(void);
END_EXTERN_C

class CPerlHost
{
public:
    /* Constructors */
    CPerlHost(void);
    CPerlHost(const struct IPerlMem** ppMem, const struct IPerlMem** ppMemShared,
                 const struct IPerlMem** ppMemParse, const struct IPerlEnv** ppEnv,
                 const struct IPerlStdIO** ppStdIO, const struct IPerlLIO** ppLIO,
                 const struct IPerlDir** ppDir, const struct IPerlSock** ppSock,
                 const struct IPerlProc** ppProc);
    CPerlHost(CPerlHost& host);
    ~CPerlHost(void);
    VMEM_H_NEW_OP;

    static CPerlHost* IPerlMem2Host(const struct IPerlMem** piPerl);
    static CPerlHost* IPerlMemShared2Host(const struct IPerlMem** piPerl);
    static CPerlHost* IPerlMemParse2Host(const struct IPerlMem** piPerl);
    static CPerlHost* IPerlEnv2Host(const struct IPerlEnv** piPerl);
    static CPerlHost* IPerlStdIO2Host(const struct IPerlStdIO** piPerl);
    static CPerlHost* IPerlLIO2Host(const struct IPerlLIO** piPerl);
    static CPerlHost* IPerlDir2Host(const struct IPerlDir** piPerl);
    static CPerlHost* IPerlSock2Host(const struct IPerlSock** piPerl);
    static CPerlHost* IPerlProc2Host(const struct IPerlProc** piPerl);

    BOOL PerlCreate(void);
    int PerlParse(int argc, char** argv, char** env);
    int PerlRun(void);
    void PerlDestroy(void);

/* IPerlMem */
    /* Locks provided but should be unnecessary as this is private pool */
    inline void* Malloc(size_t size) { return m_VMem.Malloc(size); };
    inline void* Realloc(void* ptr, size_t size) { return m_VMem.Realloc(ptr, size); };
    inline void Free(void* ptr) { m_VMem.Free(ptr); };
    inline void* Calloc(size_t num, size_t size)
    {
        size_t count = num*size;
        void* lpVoid = Malloc(count);
        if (lpVoid)
            lpVoid = memset(lpVoid, 0, count);
        return lpVoid;
    };
    inline void GetLock(void) { m_VMem.GetLock(); };
    inline void FreeLock(void) { m_VMem.FreeLock(); };
    inline int IsLocked(void) { return m_VMem.IsLocked(); };

/* IPerlMemShared */
    /* Locks used to serialize access to the pool */
    inline void GetLockShared(void) { m_pVMemShared->GetLock(); };
    inline void FreeLockShared(void) { m_pVMemShared->FreeLock(); };
    inline int IsLockedShared(void) { return m_pVMemShared->IsLocked(); };
    inline void* MallocShared(size_t size)
    {
        void *result;
        GetLockShared();
        result = m_pVMemShared->Malloc(size);
        FreeLockShared();
        return result;
    };
    inline void* ReallocShared(void* ptr, size_t size)
    {
        void *result;
        GetLockShared();
        result = m_pVMemShared->Realloc(ptr, size);
        FreeLockShared();
        return result;
    };
    inline void FreeShared(void* ptr)
    {
        GetLockShared();
        m_pVMemShared->Free(ptr);
        FreeLockShared();
    };
    inline void* CallocShared(size_t num, size_t size)
    {
        size_t count = num*size;
        void* lpVoid = MallocShared(count);
        if (lpVoid)
            lpVoid = memset(lpVoid, 0, count);
        return lpVoid;
    };

/* IPerlMemParse */
    /* Assume something else is using locks to manage serialization
       on a batch basis
     */
    inline void GetLockParse(void) { m_pVMemParse->GetLock(); };
    inline void FreeLockParse(void) { m_pVMemParse->FreeLock(); };
    inline int IsLockedParse(void) { return m_pVMemParse->IsLocked(); };
    inline void* MallocParse(size_t size) { return m_pVMemParse->Malloc(size); };
    inline void* ReallocParse(void* ptr, size_t size) { return m_pVMemParse->Realloc(ptr, size); };
    inline void FreeParse(void* ptr) { m_pVMemParse->Free(ptr); };
    inline void* CallocParse(size_t num, size_t size)
    {
        size_t count = num*size;
        void* lpVoid = MallocParse(count);
        if (lpVoid)
            lpVoid = memset(lpVoid, 0, count);
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
    void* CreateChildEnv(void) { return CreateLocalEnvironmentStrings(m_vDir); };
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
    int Chdir(const char *dirname);

/* IPerllProc */
    void Abort(void);
    void Exit(int status);
    void _Exit(int status);
    int Execl(const char *cmdname, const char *arg0, const char *arg1, const char *arg2, const char *arg3);
    int Execv(const char *cmdname, const char *const *argv);
    int Execvp(const char *cmdname, const char *const *argv);

    inline VMem* GetMem(void) { return (VMem* )&m_VMem; };
    inline VMem* GetMemShared(void) { m_pVMemShared->AddRef(); return m_pVMemShared; };
    inline VMem* GetMemParse(void) { m_pVMemParse->AddRef(); return m_pVMemParse; };
    inline VDir* GetDir(void) { return &m_vDir; };

public:

    const struct IPerlMem*	    m_pHostperlMem;
    const struct IPerlMem*	    m_pHostperlMemShared;
    const struct IPerlMem*	    m_pHostperlMemParse;
    const struct IPerlEnv*	    m_pHostperlEnv;
    const struct IPerlStdIO*	    m_pHostperlStdIO;
    const struct IPerlLIO*	    m_pHostperlLIO;
    const struct IPerlDir*	    m_pHostperlDir;
    const struct IPerlSock*	    m_pHostperlSock;
    const struct IPerlProc*	    m_pHostperlProc;

    inline char* MapPathA(const char *pInName) { return m_vDir.MapPathA(pInName); };
    inline WCHAR* MapPathW(const WCHAR *pInName) { return m_vDir.MapPathW(pInName); };
    inline operator VDir* () { return GetDir(); };
protected:
    VMemNL  m_VMem;
    VMem*   m_pVMemShared;
    VMem*   m_pVMemParse;

    LPSTR*  m_lppEnvList;
    DWORD   m_dwEnvCount;
    BOOL    m_bTopLevel;	// is this a toplevel host?
    static long num_hosts;
public:
    inline  int LastHost(void) { return num_hosts == 1L; };
    struct interpreter *host_perl;
protected:
    VDir   m_vDir;
};

long CPerlHost::num_hosts = 0L;

extern "C" void win32_checkTLS(struct interpreter *host_perl);

#define STRUCT2RAWPTR(x, y) (CPerlHost*)(((LPBYTE)x)-offsetof(CPerlHost, y))
#ifdef CHECK_HOST_INTERP
inline CPerlHost* CheckInterp(CPerlHost *host)
{
 win32_checkTLS(host->host_perl);
 return host;
}
#define STRUCT2PTR(x, y) CheckInterp(STRUCT2RAWPTR(x, y))
#else
#define STRUCT2PTR(x, y) STRUCT2RAWPTR(x, y)
#endif

inline CPerlHost* IPerlMem2Host(const struct IPerlMem** piPerl)
{
    return STRUCT2RAWPTR(piPerl, m_pHostperlMem);
}

inline CPerlHost* IPerlMemShared2Host(const struct IPerlMem** piPerl)
{
    return STRUCT2RAWPTR(piPerl, m_pHostperlMemShared);
}

inline CPerlHost* IPerlMemParse2Host(const struct IPerlMem** piPerl)
{
    return STRUCT2RAWPTR(piPerl, m_pHostperlMemParse);
}

inline CPerlHost* IPerlEnv2Host(const struct IPerlEnv** piPerl)
{
    return STRUCT2PTR(piPerl, m_pHostperlEnv);
}

inline CPerlHost* IPerlStdIO2Host(const struct IPerlStdIO** piPerl)
{
    return STRUCT2PTR(piPerl, m_pHostperlStdIO);
}

inline CPerlHost* IPerlLIO2Host(const struct IPerlLIO** piPerl)
{
    return STRUCT2PTR(piPerl, m_pHostperlLIO);
}

inline CPerlHost* IPerlDir2Host(const struct IPerlDir** piPerl)
{
    return STRUCT2PTR(piPerl, m_pHostperlDir);
}

inline CPerlHost* IPerlSock2Host(const struct IPerlSock** piPerl)
{
    return STRUCT2PTR(piPerl, m_pHostperlSock);
}

inline CPerlHost* IPerlProc2Host(const struct IPerlProc** piPerl)
{
    return STRUCT2PTR(piPerl, m_pHostperlProc);
}



#undef IPERL2HOST
#define IPERL2HOST(x) IPerlMem2Host(x)

/* IPerlMem */
void*
PerlMemMalloc(const struct IPerlMem** piPerl, size_t size)
{
    return IPERL2HOST(piPerl)->Malloc(size);
}
void*
PerlMemRealloc(const struct IPerlMem** piPerl, void* ptr, size_t size)
{
    return IPERL2HOST(piPerl)->Realloc(ptr, size);
}
void
PerlMemFree(const struct IPerlMem** piPerl, void* ptr)
{
    IPERL2HOST(piPerl)->Free(ptr);
}
void*
PerlMemCalloc(const struct IPerlMem** piPerl, size_t num, size_t size)
{
    return IPERL2HOST(piPerl)->Calloc(num, size);
}

void
PerlMemGetLock(const struct IPerlMem** piPerl)
{
    IPERL2HOST(piPerl)->GetLock();
}

void
PerlMemFreeLock(const struct IPerlMem** piPerl)
{
    IPERL2HOST(piPerl)->FreeLock();
}

int
PerlMemIsLocked(const struct IPerlMem** piPerl)
{
    return IPERL2HOST(piPerl)->IsLocked();
}

const struct IPerlMem perlMem =
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
PerlMemSharedMalloc(const struct IPerlMem** piPerl, size_t size)
{
    return IPERL2HOST(piPerl)->MallocShared(size);
}
void*
PerlMemSharedRealloc(const struct IPerlMem** piPerl, void* ptr, size_t size)
{
    return IPERL2HOST(piPerl)->ReallocShared(ptr, size);
}
void
PerlMemSharedFree(const struct IPerlMem** piPerl, void* ptr)
{
    IPERL2HOST(piPerl)->FreeShared(ptr);
}
void*
PerlMemSharedCalloc(const struct IPerlMem** piPerl, size_t num, size_t size)
{
    return IPERL2HOST(piPerl)->CallocShared(num, size);
}

void
PerlMemSharedGetLock(const struct IPerlMem** piPerl)
{
    IPERL2HOST(piPerl)->GetLockShared();
}

void
PerlMemSharedFreeLock(const struct IPerlMem** piPerl)
{
    IPERL2HOST(piPerl)->FreeLockShared();
}

int
PerlMemSharedIsLocked(const struct IPerlMem** piPerl)
{
    return IPERL2HOST(piPerl)->IsLockedShared();
}

const struct IPerlMem perlMemShared =
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
PerlMemParseMalloc(const struct IPerlMem** piPerl, size_t size)
{
    return IPERL2HOST(piPerl)->MallocParse(size);
}
void*
PerlMemParseRealloc(const struct IPerlMem** piPerl, void* ptr, size_t size)
{
    return IPERL2HOST(piPerl)->ReallocParse(ptr, size);
}
void
PerlMemParseFree(const struct IPerlMem** piPerl, void* ptr)
{
    IPERL2HOST(piPerl)->FreeParse(ptr);
}
void*
PerlMemParseCalloc(const struct IPerlMem** piPerl, size_t num, size_t size)
{
    return IPERL2HOST(piPerl)->CallocParse(num, size);
}

void
PerlMemParseGetLock(const struct IPerlMem** piPerl)
{
    IPERL2HOST(piPerl)->GetLockParse();
}

void
PerlMemParseFreeLock(const struct IPerlMem** piPerl)
{
    IPERL2HOST(piPerl)->FreeLockParse();
}

int
PerlMemParseIsLocked(const struct IPerlMem** piPerl)
{
    return IPERL2HOST(piPerl)->IsLockedParse();
}

const struct IPerlMem perlMemParse =
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
PerlEnvGetenv(const struct IPerlEnv** piPerl, const char *varname)
{
    return IPERL2HOST(piPerl)->Getenv(varname);
};

int
PerlEnvPutenv(const struct IPerlEnv** piPerl, const char *envstring)
{
    return IPERL2HOST(piPerl)->Putenv(envstring);
};

char*
PerlEnvGetenv_len(const struct IPerlEnv** piPerl, const char* varname, unsigned long* len)
{
    return IPERL2HOST(piPerl)->Getenv(varname, len);
}

int
PerlEnvUname(const struct IPerlEnv** piPerl, struct utsname *name)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_uname(name);
}

void
PerlEnvClearenv(const struct IPerlEnv** piPerl)
{
    IPERL2HOST(piPerl)->Clearenv();
}

void*
PerlEnvGetChildenv(const struct IPerlEnv** piPerl)
{
    return IPERL2HOST(piPerl)->CreateChildEnv();
}

void
PerlEnvFreeChildenv(const struct IPerlEnv** piPerl, void* childEnv)
{
    IPERL2HOST(piPerl)->FreeChildEnv(childEnv);
}

char*
PerlEnvGetChilddir(const struct IPerlEnv** piPerl)
{
    return IPERL2HOST(piPerl)->GetChildDir();
}

void
PerlEnvFreeChilddir(const struct IPerlEnv** piPerl, char* childDir)
{
    IPERL2HOST(piPerl)->FreeChildDir(childDir);
}

unsigned long
PerlEnvOsId(const struct IPerlEnv** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_os_id();
}

char*
PerlEnvLibPath(const struct IPerlEnv** piPerl, WIN32_NO_REGISTRY_M_(const char *pl) STRLEN *const len)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_get_privlib(WIN32_NO_REGISTRY_M_(pl) len);
}

char*
PerlEnvSiteLibPath(const struct IPerlEnv** piPerl, const char *pl, STRLEN *const len)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_get_sitelib(pl, len);
}

char*
PerlEnvVendorLibPath(const struct IPerlEnv** piPerl, const char *pl,
                     STRLEN *const len)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_get_vendorlib(pl, len);
}

void
PerlEnvGetChildIO(const struct IPerlEnv** piPerl, child_IO_table* ptr)
{
    PERL_UNUSED_ARG(piPerl);
    win32_get_child_IO(ptr);
}

const struct IPerlEnv perlEnv =
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
FILE*
PerlStdIOStdin(const struct IPerlStdIO** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_stdin();
}

FILE*
PerlStdIOStdout(const struct IPerlStdIO** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_stdout();
}

FILE*
PerlStdIOStderr(const struct IPerlStdIO** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_stderr();
}

FILE*
PerlStdIOOpen(const struct IPerlStdIO** piPerl, const char *path, const char *mode)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_fopen(path, mode);
}

int
PerlStdIOClose(const struct IPerlStdIO** piPerl, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_fclose((pf));
}

int
PerlStdIOEof(const struct IPerlStdIO** piPerl, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_feof(pf);
}

int
PerlStdIOError(const struct IPerlStdIO** piPerl, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_ferror(pf);
}

void
PerlStdIOClearerr(const struct IPerlStdIO** piPerl, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    win32_clearerr(pf);
}

int
PerlStdIOGetc(const struct IPerlStdIO** piPerl, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_getc(pf);
}

STDCHAR*
PerlStdIOGetBase(const struct IPerlStdIO** piPerl, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
#ifdef FILE_base
    FILE *f = pf;
    return FILE_base(f);
#else
    return NULL;
#endif
}

int
PerlStdIOGetBufsiz(const struct IPerlStdIO** piPerl, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
#ifdef FILE_bufsiz
    FILE *f = pf;
    return FILE_bufsiz(f);
#else
    return (-1);
#endif
}

int
PerlStdIOGetCnt(const struct IPerlStdIO** piPerl, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
#ifdef USE_STDIO_PTR
    FILE *f = pf;
    return FILE_cnt(f);
#else
    return (-1);
#endif
}

STDCHAR*
PerlStdIOGetPtr(const struct IPerlStdIO** piPerl, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
#ifdef USE_STDIO_PTR
    FILE *f = pf;
    return FILE_ptr(f);
#else
    return NULL;
#endif
}

char*
PerlStdIOGets(const struct IPerlStdIO** piPerl, char* s, int n, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_fgets(s, n, pf);
}

int
PerlStdIOPutc(const struct IPerlStdIO** piPerl, int c, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_fputc(c, pf);
}

int
PerlStdIOPuts(const struct IPerlStdIO** piPerl, const char *s, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_fputs(s, pf);
}

int
PerlStdIOFlush(const struct IPerlStdIO** piPerl, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_fflush(pf);
}

int
PerlStdIOUngetc(const struct IPerlStdIO** piPerl,int c, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_ungetc(c, pf);
}

int
PerlStdIOFileno(const struct IPerlStdIO** piPerl, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_fileno(pf);
}

FILE*
PerlStdIOFdopen(const struct IPerlStdIO** piPerl, int fd, const char *mode)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_fdopen(fd, mode);
}

FILE*
PerlStdIOReopen(const struct IPerlStdIO** piPerl, const char*path, const char*mode, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_freopen(path, mode, (FILE*)pf);
}

SSize_t
PerlStdIORead(const struct IPerlStdIO** piPerl, void *buffer, Size_t size, Size_t count, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_fread(buffer, size, count, pf);
}

SSize_t
PerlStdIOWrite(const struct IPerlStdIO** piPerl, const void *buffer, Size_t size, Size_t count, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_fwrite(buffer, size, count, pf);
}

void
PerlStdIOSetBuf(const struct IPerlStdIO** piPerl, FILE* pf, char* buffer)
{
    PERL_UNUSED_ARG(piPerl);
    win32_setbuf(pf, buffer);
}

int
PerlStdIOSetVBuf(const struct IPerlStdIO** piPerl, FILE* pf, char* buffer, int type, Size_t size)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_setvbuf(pf, buffer, type, size);
}

void
PerlStdIOSetCnt(const struct IPerlStdIO** piPerl, FILE* pf, int n)
{
    PERL_UNUSED_ARG(piPerl);
#ifdef STDIO_CNT_LVALUE
    FILE *f = pf;
    FILE_cnt(f) = n;
#else
    PERL_UNUSED_ARG(pf);
    PERL_UNUSED_ARG(n);
#endif
}

void
PerlStdIOSetPtr(const struct IPerlStdIO** piPerl, FILE* pf, STDCHAR * ptr)
{
    PERL_UNUSED_ARG(piPerl);
#ifdef STDIO_PTR_LVALUE
    FILE *f = pf;
    FILE_ptr(f) = ptr;
#else
    PERL_UNUSED_ARG(pf);
    PERL_UNUSED_ARG(ptr);
#endif
}

void
PerlStdIOSetlinebuf(const struct IPerlStdIO** piPerl, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    win32_setvbuf(pf, NULL, _IOLBF, 0);
}

int
PerlStdIOPrintf(const struct IPerlStdIO** piPerl, FILE* pf, const char *format,...)
{
    va_list arglist;
    va_start(arglist, format);
    PERL_UNUSED_ARG(piPerl);
    return win32_vfprintf(pf, format, arglist);
}

int
PerlStdIOVprintf(const struct IPerlStdIO** piPerl, FILE* pf, const char *format, va_list arglist)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_vfprintf(pf, format, arglist);
}

Off_t
PerlStdIOTell(const struct IPerlStdIO** piPerl, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_ftell(pf);
}

int
PerlStdIOSeek(const struct IPerlStdIO** piPerl, FILE* pf, Off_t offset, int origin)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_fseek(pf, offset, origin);
}

void
PerlStdIORewind(const struct IPerlStdIO** piPerl, FILE* pf)
{
    PERL_UNUSED_ARG(piPerl);
    win32_rewind(pf);
}

FILE*
PerlStdIOTmpfile(const struct IPerlStdIO** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_tmpfile();
}

int
PerlStdIOGetpos(const struct IPerlStdIO** piPerl, FILE* pf, Fpos_t *p)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_fgetpos(pf, p);
}

int
PerlStdIOSetpos(const struct IPerlStdIO** piPerl, FILE* pf, const Fpos_t *p)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_fsetpos(pf, p);
}
void
PerlStdIOInit(const struct IPerlStdIO** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
}

void
PerlStdIOInitOSExtras(const struct IPerlStdIO** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    Perl_init_os_extras();
}

int
PerlStdIOOpenOSfhandle(const struct IPerlStdIO** piPerl, intptr_t osfhandle, int flags)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_open_osfhandle(osfhandle, flags);
}

intptr_t
PerlStdIOGetOSfhandle(const struct IPerlStdIO** piPerl, int filenum)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_get_osfhandle(filenum);
}

FILE*
PerlStdIOFdupopen(const struct IPerlStdIO** piPerl, FILE* pf)
{
    FILE* pfdup;
    fpos_t pos;
    char mode[3];
    int fileno = win32_dup(win32_fileno(pf));
    PERL_UNUSED_ARG(piPerl);

    /* open the file in the same mode */
    if (PERLIO_FILE_flag(pf) & PERLIO_FILE_flag_RD) {
        mode[0] = 'r';
        mode[1] = 0;
    }
    else if (PERLIO_FILE_flag(pf) & PERLIO_FILE_flag_WR) {
        mode[0] = 'a';
        mode[1] = 0;
    }
    else if (PERLIO_FILE_flag(pf) & PERLIO_FILE_flag_RW) {
        mode[0] = 'r';
        mode[1] = '+';
        mode[2] = 0;
    }

    /* it appears that the binmode is attached to the
     * file descriptor so binmode files will be handled
     * correctly
     */
    pfdup = win32_fdopen(fileno, mode);

    /* move the file pointer to the same position */
    if (!fgetpos(pf, &pos)) {
        fsetpos(pfdup, &pos);
    }
    return pfdup;
}

const struct IPerlStdIO perlStdIO =
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


#undef IPERL2HOST
#define IPERL2HOST(x) IPerlLIO2Host(x)

/* IPerlLIO */
int
PerlLIOAccess(const struct IPerlLIO** piPerl, const char *path, int mode)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_access(path, mode);
}

int
PerlLIOChmod(const struct IPerlLIO** piPerl, const char *filename, int pmode)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_chmod(filename, pmode);
}

int
PerlLIOChown(const struct IPerlLIO** piPerl, const char *filename, uid_t owner, gid_t group)
{
    PERL_UNUSED_ARG(piPerl);
    return chown(filename, owner, group);
}

int
PerlLIOChsize(const struct IPerlLIO** piPerl, int handle, Off_t size)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_chsize(handle, size);
}

int
PerlLIOClose(const struct IPerlLIO** piPerl, int handle)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_close(handle);
}

int
PerlLIODup(const struct IPerlLIO** piPerl, int handle)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_dup(handle);
}

int
PerlLIODup2(const struct IPerlLIO** piPerl, int handle1, int handle2)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_dup2(handle1, handle2);
}

int
PerlLIOFlock(const struct IPerlLIO** piPerl, int fd, int oper)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_flock(fd, oper);
}

int
PerlLIOFileStat(const struct IPerlLIO** piPerl, int handle, Stat_t *buffer)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_fstat(handle, buffer);
}

int
PerlLIOIOCtl(const struct IPerlLIO** piPerl, int i, unsigned int u, char *data)
{
    u_long u_long_arg;
    int retval;
    PERL_UNUSED_ARG(piPerl);

    /* mauke says using memcpy avoids alignment issues */
    memcpy(&u_long_arg, data, sizeof u_long_arg); 
    retval = win32_ioctlsocket((SOCKET)i, (long)u, &u_long_arg);
    memcpy(data, &u_long_arg, sizeof u_long_arg);
    return retval;
}

int
PerlLIOIsatty(const struct IPerlLIO** piPerl, int fd)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_isatty(fd);
}

int
PerlLIOLink(const struct IPerlLIO** piPerl, const char*oldname, const char *newname)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_link(oldname, newname);
}

int
PerlLIOSymLink(const struct IPerlLIO** piPerl, const char*oldname, const char *newname)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_symlink(oldname, newname);
}

int
PerlLIOReadLink(const struct IPerlLIO** piPerl, const char *path, char *buf, size_t bufsiz)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_readlink(path, buf, bufsiz);
}

Off_t
PerlLIOLseek(const struct IPerlLIO** piPerl, int handle, Off_t offset, int origin)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_lseek(handle, offset, origin);
}

int
PerlLIOLstat(const struct IPerlLIO** piPerl, const char *path, Stat_t *buffer)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_lstat(path, buffer);
}

char*
PerlLIOMktemp(const struct IPerlLIO** piPerl, char *Template)
{
    PERL_UNUSED_ARG(piPerl);
    return mktemp(Template);
}

int
PerlLIOOpen(const struct IPerlLIO** piPerl, const char *filename, int oflag)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_open(filename, oflag);
}

int
PerlLIOOpen3(const struct IPerlLIO** piPerl, const char *filename, int oflag, int pmode)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_open(filename, oflag, pmode);
}

int
PerlLIORead(const struct IPerlLIO** piPerl, int handle, void *buffer, unsigned int count)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_read(handle, buffer, count);
}

int
PerlLIORename(const struct IPerlLIO** piPerl, const char *OldFileName, const char *newname)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_rename(OldFileName, newname);
}

int
PerlLIOSetmode(const struct IPerlLIO** piPerl, int handle, int mode)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_setmode(handle, mode);
}

int
PerlLIONameStat(const struct IPerlLIO** piPerl, const char *path, Stat_t *buffer)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_stat(path, buffer);
}

char*
PerlLIOTmpnam(const struct IPerlLIO** piPerl, char *string)
{
    PERL_UNUSED_ARG(piPerl);
    return tmpnam(string);
}

int
PerlLIOUmask(const struct IPerlLIO** piPerl, int pmode)
{
    PERL_UNUSED_ARG(piPerl);
    return umask(pmode);
}

int
PerlLIOUnlink(const struct IPerlLIO** piPerl, const char *filename)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_unlink(filename);
}

int
PerlLIOUtime(const struct IPerlLIO** piPerl, const char *filename, struct utimbuf *times)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_utime(filename, times);
}

int
PerlLIOWrite(const struct IPerlLIO** piPerl, int handle, const void *buffer, unsigned int count)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_write(handle, buffer, count);
}

const struct IPerlLIO perlLIO =
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
    PerlLIOSymLink,
    PerlLIOReadLink
};


#undef IPERL2HOST
#define IPERL2HOST(x) IPerlDir2Host(x)

/* IPerlDIR */
int
PerlDirMakedir(const struct IPerlDir** piPerl, const char *dirname, int mode)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_mkdir(dirname, mode);
}

int
PerlDirChdir(const struct IPerlDir** piPerl, const char *dirname)
{
    PERL_UNUSED_ARG(piPerl);
    return IPERL2HOST(piPerl)->Chdir(dirname);
}

int
PerlDirRmdir(const struct IPerlDir** piPerl, const char *dirname)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_rmdir(dirname);
}

int
PerlDirClose(const struct IPerlDir** piPerl, DIR *dirp)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_closedir(dirp);
}

DIR*
PerlDirOpen(const struct IPerlDir** piPerl, const char *filename)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_opendir(filename);
}

struct direct *
PerlDirRead(const struct IPerlDir** piPerl, DIR *dirp)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_readdir(dirp);
}

void
PerlDirRewind(const struct IPerlDir** piPerl, DIR *dirp)
{
    PERL_UNUSED_ARG(piPerl);
    win32_rewinddir(dirp);
}

void
PerlDirSeek(const struct IPerlDir** piPerl, DIR *dirp, long loc)
{
    PERL_UNUSED_ARG(piPerl);
    win32_seekdir(dirp, loc);
}

long
PerlDirTell(const struct IPerlDir** piPerl, DIR *dirp)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_telldir(dirp);
}

char*
PerlDirMapPathA(const struct IPerlDir** piPerl, const char* path)
{
    return IPERL2HOST(piPerl)->MapPathA(path);
}

WCHAR*
PerlDirMapPathW(const struct IPerlDir** piPerl, const WCHAR* path)
{
    return IPERL2HOST(piPerl)->MapPathW(path);
}

const struct IPerlDir perlDir =
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
PerlSockHtonl(const struct IPerlSock** piPerl, u_long hostlong)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_htonl(hostlong);
}

u_short
PerlSockHtons(const struct IPerlSock** piPerl, u_short hostshort)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_htons(hostshort);
}

u_long
PerlSockNtohl(const struct IPerlSock** piPerl, u_long netlong)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_ntohl(netlong);
}

u_short
PerlSockNtohs(const struct IPerlSock** piPerl, u_short netshort)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_ntohs(netshort);
}

SOCKET PerlSockAccept(const struct IPerlSock** piPerl, SOCKET s, struct sockaddr* addr, int* addrlen)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_accept(s, addr, addrlen);
}

int
PerlSockBind(const struct IPerlSock** piPerl, SOCKET s, const struct sockaddr* name, int namelen)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_bind(s, name, namelen);
}

int
PerlSockConnect(const struct IPerlSock** piPerl, SOCKET s, const struct sockaddr* name, int namelen)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_connect(s, name, namelen);
}

void
PerlSockEndhostent(const struct IPerlSock** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    win32_endhostent();
}

void
PerlSockEndnetent(const struct IPerlSock** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    win32_endnetent();
}

void
PerlSockEndprotoent(const struct IPerlSock** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    win32_endprotoent();
}

void
PerlSockEndservent(const struct IPerlSock** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    win32_endservent();
}

struct hostent*
PerlSockGethostbyaddr(const struct IPerlSock** piPerl, const char* addr, int len, int type)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_gethostbyaddr(addr, len, type);
}

struct hostent*
PerlSockGethostbyname(const struct IPerlSock** piPerl, const char* name)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_gethostbyname(name);
}

struct hostent*
PerlSockGethostent(const struct IPerlSock** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    win32_croak_not_implemented("gethostent");
    return NULL;
}

int
PerlSockGethostname(const struct IPerlSock** piPerl, char* name, int namelen)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_gethostname(name, namelen);
}

struct netent *
PerlSockGetnetbyaddr(const struct IPerlSock** piPerl, long net, int type)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_getnetbyaddr(net, type);
}

struct netent *
PerlSockGetnetbyname(const struct IPerlSock** piPerl, const char *name)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_getnetbyname((char*)name);
}

struct netent *
PerlSockGetnetent(const struct IPerlSock** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_getnetent();
}

int PerlSockGetpeername(const struct IPerlSock** piPerl, SOCKET s, struct sockaddr* name, int* namelen)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_getpeername(s, name, namelen);
}

struct protoent*
PerlSockGetprotobyname(const struct IPerlSock** piPerl, const char* name)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_getprotobyname(name);
}

struct protoent*
PerlSockGetprotobynumber(const struct IPerlSock** piPerl, int number)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_getprotobynumber(number);
}

struct protoent*
PerlSockGetprotoent(const struct IPerlSock** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_getprotoent();
}

struct servent*
PerlSockGetservbyname(const struct IPerlSock** piPerl, const char* name, const char* proto)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_getservbyname(name, proto);
}

struct servent*
PerlSockGetservbyport(const struct IPerlSock** piPerl, int port, const char* proto)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_getservbyport(port, proto);
}

struct servent*
PerlSockGetservent(const struct IPerlSock** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_getservent();
}

int
PerlSockGetsockname(const struct IPerlSock** piPerl, SOCKET s, struct sockaddr* name, int* namelen)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_getsockname(s, name, namelen);
}

int
PerlSockGetsockopt(const struct IPerlSock** piPerl, SOCKET s, int level, int optname, char* optval, int* optlen)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_getsockopt(s, level, optname, optval, optlen);
}

unsigned long
PerlSockInetAddr(const struct IPerlSock** piPerl, const char* cp)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_inet_addr(cp);
}

char*
PerlSockInetNtoa(const struct IPerlSock** piPerl, struct in_addr in)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_inet_ntoa(in);
}

int
PerlSockListen(const struct IPerlSock** piPerl, SOCKET s, int backlog)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_listen(s, backlog);
}

int
PerlSockRecv(const struct IPerlSock** piPerl, SOCKET s, char* buffer, int len, int flags)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_recv(s, buffer, len, flags);
}

int
PerlSockRecvfrom(const struct IPerlSock** piPerl, SOCKET s, char* buffer, int len, int flags, struct sockaddr* from, int* fromlen)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_recvfrom(s, buffer, len, flags, from, fromlen);
}

int
PerlSockSelect(const struct IPerlSock** piPerl, int nfds, char* readfds, char* writefds, char* exceptfds, const struct timeval* timeout)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_select(nfds, (Perl_fd_set*)readfds, (Perl_fd_set*)writefds, (Perl_fd_set*)exceptfds, timeout);
}

int
PerlSockSend(const struct IPerlSock** piPerl, SOCKET s, const char* buffer, int len, int flags)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_send(s, buffer, len, flags);
}

int
PerlSockSendto(const struct IPerlSock** piPerl, SOCKET s, const char* buffer, int len, int flags, const struct sockaddr* to, int tolen)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_sendto(s, buffer, len, flags, to, tolen);
}

void
PerlSockSethostent(const struct IPerlSock** piPerl, int stayopen)
{
    PERL_UNUSED_ARG(piPerl);
    win32_sethostent(stayopen);
}

void
PerlSockSetnetent(const struct IPerlSock** piPerl, int stayopen)
{
    PERL_UNUSED_ARG(piPerl);
    win32_setnetent(stayopen);
}

void
PerlSockSetprotoent(const struct IPerlSock** piPerl, int stayopen)
{
    PERL_UNUSED_ARG(piPerl);
    win32_setprotoent(stayopen);
}

void
PerlSockSetservent(const struct IPerlSock** piPerl, int stayopen)
{
    PERL_UNUSED_ARG(piPerl);
    win32_setservent(stayopen);
}

int
PerlSockSetsockopt(const struct IPerlSock** piPerl, SOCKET s, int level, int optname, const char* optval, int optlen)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_setsockopt(s, level, optname, optval, optlen);
}

int
PerlSockShutdown(const struct IPerlSock** piPerl, SOCKET s, int how)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_shutdown(s, how);
}

SOCKET
PerlSockSocket(const struct IPerlSock** piPerl, int af, int type, int protocol)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_socket(af, type, protocol);
}

int
PerlSockSocketpair(const struct IPerlSock** piPerl, int domain, int type, int protocol, int* fds)
{
    PERL_UNUSED_ARG(piPerl);
    return Perl_my_socketpair(domain, type, protocol, fds);
}

int
PerlSockClosesocket(const struct IPerlSock** piPerl, SOCKET s)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_closesocket(s);
}

int
PerlSockIoctlsocket(const struct IPerlSock** piPerl, SOCKET s, long cmd, u_long *argp)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_ioctlsocket(s, cmd, argp);
}

const struct IPerlSock perlSock =
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
PerlProcAbort(const struct IPerlProc** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    win32_abort();
}

char *
PerlProcCrypt(const struct IPerlProc** piPerl, const char* clear, const char* salt)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_crypt(clear, salt);
}

PERL_CALLCONV_NO_RET void
PerlProcExit(const struct IPerlProc** piPerl, int status)
{
    PERL_UNUSED_ARG(piPerl);
    exit(status);
}

PERL_CALLCONV_NO_RET void
PerlProc_Exit(const struct IPerlProc** piPerl, int status)
{
    PERL_UNUSED_ARG(piPerl);
    _exit(status);
}

int
PerlProcExecl(const struct IPerlProc** piPerl, const char *cmdname, const char *arg0, const char *arg1, const char *arg2, const char *arg3)
{
    PERL_UNUSED_ARG(piPerl);
    return execl(cmdname, arg0, arg1, arg2, arg3);
}

int
PerlProcExecv(const struct IPerlProc** piPerl, const char *cmdname, const char *const *argv)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_execvp(cmdname, argv);
}

int
PerlProcExecvp(const struct IPerlProc** piPerl, const char *cmdname, const char *const *argv)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_execvp(cmdname, argv);
}

uid_t
PerlProcGetuid(const struct IPerlProc** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    return getuid();
}

uid_t
PerlProcGeteuid(const struct IPerlProc** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    return geteuid();
}

gid_t
PerlProcGetgid(const struct IPerlProc** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    return getgid();
}

gid_t
PerlProcGetegid(const struct IPerlProc** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    return getegid();
}

char *
PerlProcGetlogin(const struct IPerlProc** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    return g_getlogin();
}

int
PerlProcKill(const struct IPerlProc** piPerl, int pid, int sig)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_kill(pid, sig);
}

int
PerlProcKillpg(const struct IPerlProc** piPerl, int pid, int sig)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_kill(pid, -sig);
}

int
PerlProcPauseProc(const struct IPerlProc** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_pause();
}

PerlIO*
PerlProcPopen(const struct IPerlProc** piPerl, const char *command, const char *mode)
{
    dTHX;
    PERL_FLUSHALL_FOR_CHILD;
    PERL_UNUSED_ARG(piPerl);
    return win32_popen(command, mode);
}

PerlIO*
PerlProcPopenList(const struct IPerlProc** piPerl, const char *mode, IV narg, SV **args)
{
    dTHX;
    PERL_FLUSHALL_FOR_CHILD;
    PERL_UNUSED_ARG(piPerl);
    return win32_popenlist(mode, narg, args);
}

int
PerlProcPclose(const struct IPerlProc** piPerl, PerlIO *stream)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_pclose(stream);
}

int
PerlProcPipe(const struct IPerlProc** piPerl, int *phandles)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_pipe(phandles, 512, O_BINARY);
}

int
PerlProcSetuid(const struct IPerlProc** piPerl, uid_t u)
{
    PERL_UNUSED_ARG(piPerl);
    return setuid(u);
}

int
PerlProcSetgid(const struct IPerlProc** piPerl, gid_t g)
{
    PERL_UNUSED_ARG(piPerl);
    return setgid(g);
}

int
PerlProcSleep(const struct IPerlProc** piPerl, unsigned int s)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_sleep(s);
}

int
PerlProcTimes(const struct IPerlProc** piPerl, struct tms *timebuf)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_times(timebuf);
}

int
PerlProcWait(const struct IPerlProc** piPerl, int *status)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_wait(status);
}

int
PerlProcWaitpid(const struct IPerlProc** piPerl, int pid, int *status, int flags)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_waitpid(pid, status, flags);
}

Sighandler_t
PerlProcSignal(const struct IPerlProc** piPerl, int sig, Sighandler_t subcode)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_signal(sig, subcode);
}

int
PerlProcGetTimeOfDay(const struct IPerlProc** piPerl, struct timeval *t, void *z)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_gettimeofday(t, z);
}

#ifdef USE_ITHREADS
PERL_STACK_REALIGN
static THREAD_RET_TYPE
win32_start_child(LPVOID arg)
{
    PerlInterpreter *my_perl = (PerlInterpreter*)arg;
    int status;
    HWND parent_message_hwnd;
#ifdef PERL_SYNC_FORK
    static long sync_fork_id = 0;
    long id = ++sync_fork_id;
#endif


    PERL_SET_THX(my_perl);
    win32_checkTLS(my_perl);

#ifdef PERL_SYNC_FORK
    w32_pseudo_id = id;
#else
    w32_pseudo_id = GetCurrentThreadId();
#endif
#ifdef PERL_USES_PL_PIDSTATUS    
    hv_clear(PL_pidstatus);
#endif    

    /* create message window and tell parent about it */
    parent_message_hwnd = w32_message_hwnd;
    w32_message_hwnd = win32_create_message_window();
    if (parent_message_hwnd != NULL)
        PostMessage(parent_message_hwnd, WM_USER_MESSAGE, w32_pseudo_id, (LPARAM)w32_message_hwnd);

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
        volatile int oldscope = 1; /* We are responsible for all scopes */

restart:
        JMPENV_PUSH(status);
        switch (status) {
        case 0:
            CALLRUNOPS(aTHX);
            /* We may have additional unclosed scopes if fork() was called
             * from within a BEGIN block.  See perlfork.pod for more details.
             * We cannot clean up these other scopes because they belong to a
             * different interpreter, but we also cannot leave PL_scopestack_ix
             * dangling because that can trigger an assertion in perl_destruct().
             */
            if (PL_scopestack_ix > oldscope) {
                PL_scopestack[oldscope-1] = PL_scopestack[PL_scopestack_ix-1];
                PL_scopestack_ix = oldscope;
            }
            status = 0;
            break;
        case 2:
            while (PL_scopestack_ix > oldscope)
                LEAVE;
            FREETMPS;
            PL_curstash = PL_defstash;
            if (PL_curstash != PL_defstash) {
                SvREFCNT_dec(PL_curstash);
                PL_curstash = (HV *)SvREFCNT_inc(PL_defstash);
            }
            if (PL_endav && !PL_minus_c) {
                PERL_SET_PHASE(PERL_PHASE_END);
                call_list(oldscope, PL_endav);
            }
            status = STATUS_EXIT;
            break;
        case 3:
            if (PL_restartop) {
                POPSTACK_TO(PL_mainstack);
                PL_op = PL_restartop;
                PL_restartop = (OP*)NULL;
                goto restart;
            }
            PerlIO_printf(Perl_error_log, "panic: restartop\n");
            FREETMPS;
            status = 1;
            break;
        }
        JMPENV_POP;

        /* XXX hack to avoid perl_destruct() freeing optree */
        win32_checkTLS(my_perl);
        PL_main_root = (OP*)NULL;
    }

    win32_checkTLS(my_perl);
    /* close the std handles to avoid fd leaks */
    {
        do_close(PL_stdingv, FALSE);
        do_close(gv_fetchpv("STDOUT", TRUE, SVt_PVIO), FALSE); /* PL_stdoutgv - ISAGN */
        do_close(PL_stderrgv, FALSE);
    }

    /* destroy everything (waits for any pseudo-forked children) */
    win32_checkTLS(my_perl);
    perl_destruct(my_perl);
    win32_checkTLS(my_perl);
    perl_free(my_perl);

#ifdef PERL_SYNC_FORK
    return id;
#else
    return (DWORD)status;
#endif
}
#endif /* USE_ITHREADS */

int
PerlProcFork(const struct IPerlProc** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
#ifdef USE_ITHREADS
    dTHX;
    DWORD id;
    HANDLE handle;
    CPerlHost *h;

    if (w32_num_pseudo_children >= MAXIMUM_WAIT_OBJECTS) {
        errno = EAGAIN;
        return -1;
    }
    h = new CPerlHost(*(CPerlHost*)w32_internal_host);
    PerlInterpreter *new_perl = perl_clone_using((PerlInterpreter*)aTHX,
                                                 CLONEf_COPY_STACKS,
                                                 &h->m_pHostperlMem,
                                                 &h->m_pHostperlMemShared,
                                                 &h->m_pHostperlMemParse,
                                                 &h->m_pHostperlEnv,
                                                 &h->m_pHostperlStdIO,
                                                 &h->m_pHostperlLIO,
                                                 &h->m_pHostperlDir,
                                                 &h->m_pHostperlSock,
                                                 &h->m_pHostperlProc
                                                 );
    new_perl->Isys_intern.internal_host = h;
    h->host_perl = new_perl;
#  ifdef PERL_SYNC_FORK
    id = win32_start_child((LPVOID)new_perl);
    PERL_SET_THX(aTHX);
#  else
    if (w32_message_hwnd == INVALID_HANDLE_VALUE)
        w32_message_hwnd = win32_create_message_window();
    new_perl->Isys_intern.message_hwnd = w32_message_hwnd;
    w32_pseudo_child_message_hwnds[w32_num_pseudo_children] =
        (w32_message_hwnd == NULL) ? (HWND)NULL : (HWND)INVALID_HANDLE_VALUE;
#    ifdef USE_RTL_THREAD_API
    handle = (HANDLE)_beginthreadex((void*)NULL, 0, win32_start_child,
                                    (void*)new_perl, 0, (unsigned*)&id);
#    else
    handle = CreateThread(NULL, 0, win32_start_child,
                          (LPVOID)new_perl, 0, &id);
#    endif
    PERL_SET_THX(aTHX);	/* XXX perl_clone*() set TLS */
    if (!handle) {
        errno = EAGAIN;
        return -1;
    }
    w32_pseudo_child_handles[w32_num_pseudo_children] = handle;
    w32_pseudo_child_pids[w32_num_pseudo_children] = id;
    w32_pseudo_child_sigterm[w32_num_pseudo_children] = 0;
    ++w32_num_pseudo_children;
#  endif
    return -(int)id;
#else
    win32_croak_not_implemented("fork()");
    return -1;
#endif /* USE_ITHREADS */
}

int
PerlProcGetpid(const struct IPerlProc** piPerl)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_getpid();
}

void*
PerlProcDynaLoader(const struct IPerlProc** piPerl, const char* filename)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_dynaload(filename);
}

void
PerlProcGetOSError(const struct IPerlProc** piPerl, SV* sv, DWORD dwErr)
{
    PERL_UNUSED_ARG(piPerl);
    win32_str_os_error(sv, dwErr);
}

int
PerlProcSpawnvp(const struct IPerlProc** piPerl, int mode, const char *cmdname, const char *const *argv)
{
    PERL_UNUSED_ARG(piPerl);
    return win32_spawnvp(mode, cmdname, argv);
}

int
PerlProcLastHost(const struct IPerlProc** piPerl)
{
 /* this dTHX is unused in an optimized build since CPerlHost::num_hosts
    is a static */
    dTHX;
    CPerlHost *h = (CPerlHost*)w32_internal_host;
    PERL_UNUSED_ARG(piPerl);
    return h->LastHost();
}

const struct IPerlProc perlProc =
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
    PerlProcSpawnvp,
    PerlProcLastHost,
    PerlProcPopenList,
    PerlProcGetTimeOfDay
};


/*
 * CPerlHost
 */

CPerlHost::CPerlHost(void)
{
    /* Construct a host from scratch */
    InterlockedIncrement(&num_hosts);

    m_pVMemShared = new VMem();
    m_pVMemParse =  new VMem();

    m_vDir.Init(NULL);

    m_dwEnvCount = 0;
    m_lppEnvList = NULL;
    m_bTopLevel = TRUE;

    m_pHostperlMem	    = &perlMem;
    m_pHostperlMemShared    = &perlMemShared;
    m_pHostperlMemParse	    = &perlMemParse;
    m_pHostperlEnv	    = &perlEnv;
    m_pHostperlStdIO	    = &perlStdIO;
    m_pHostperlLIO	    = &perlLIO;
    m_pHostperlDir	    = &perlDir;
    m_pHostperlSock	    = &perlSock;
    m_pHostperlProc	    = &perlProc;
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

CPerlHost::CPerlHost(const struct IPerlMem** ppMem, const struct IPerlMem** ppMemShared,
                 const struct IPerlMem** ppMemParse, const struct IPerlEnv** ppEnv,
                 const struct IPerlStdIO** ppStdIO, const struct IPerlLIO** ppLIO,
                 const struct IPerlDir** ppDir, const struct IPerlSock** ppSock,
                 const struct IPerlProc** ppProc)
{
    InterlockedIncrement(&num_hosts);

    m_pVMemShared = new VMem();
    m_pVMemParse =  new VMem();

    m_vDir.Init(NULL, 0);

    m_dwEnvCount = 0;
    m_lppEnvList = NULL;
    m_bTopLevel = FALSE;

    SETUPEXCHANGE(ppMem,	m_pHostperlMem,		perlMem);
    SETUPEXCHANGE(ppMemShared,	m_pHostperlMemShared,	perlMemShared);
    SETUPEXCHANGE(ppMemParse,	m_pHostperlMemParse,	perlMemParse);
    SETUPEXCHANGE(ppEnv,	m_pHostperlEnv,		perlEnv);
    SETUPEXCHANGE(ppStdIO,	m_pHostperlStdIO,	perlStdIO);
    SETUPEXCHANGE(ppLIO,	m_pHostperlLIO,		perlLIO);
    SETUPEXCHANGE(ppDir,	m_pHostperlDir,		perlDir);
    SETUPEXCHANGE(ppSock,	m_pHostperlSock,	perlSock);
    SETUPEXCHANGE(ppProc,	m_pHostperlProc,	perlProc);
}
#undef SETUPEXCHANGE

CPerlHost::CPerlHost(CPerlHost& host)
{
    /* Construct a host from another host */
    InterlockedIncrement(&num_hosts);

    m_pVMemShared = host.GetMemShared();
    m_pVMemParse =  host.GetMemParse();

    /* duplicate directory info */
    m_vDir.Init(host.GetDir(), 0);

    m_pHostperlMem	    = &perlMem;
    m_pHostperlMemShared    = &perlMemShared;
    m_pHostperlMemParse	    = &perlMemParse;
    m_pHostperlEnv	    = &perlEnv;
    m_pHostperlStdIO	    = &perlStdIO;
    m_pHostperlLIO	    = &perlLIO;
    m_pHostperlDir	    = &perlDir;
    m_pHostperlSock	    = &perlSock;
    m_pHostperlProc	    = &perlProc;

    m_dwEnvCount = 0;
    m_lppEnvList = NULL;
    m_bTopLevel = FALSE;

    /* duplicate environment info */
    LPSTR lpPtr;
    DWORD dwIndex = 0;
    while((lpPtr = host.GetIndex(dwIndex)))
        Add(lpPtr);
}

CPerlHost::~CPerlHost(void)
{
    Reset();
    InterlockedDecrement(&num_hosts);
    //delete m_vDir;
    m_pVMemParse->Release();
    m_pVMemShared->Release();
    //m_VMem.Release();
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
    if (!lpStr)
        return NULL;
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
    LPSTR *lpPtr;
    STRLEN length = strlen(lpStr)+1;

    // replacing ?
    lpPtr = Lookup(lpStr);
    if (lpPtr != NULL) {
        // must allocate things via host memory allocation functions 
        // rather than perl's Renew() et al, as the perl interpreter
        // may either not be initialized enough when we allocate these,
        // or may already be dead when we go to free these
        *lpPtr = (char*)Realloc(*lpPtr, length * sizeof(char));
        strcpy(*lpPtr, lpStr);
    }
    else {
        m_lppEnvList = (LPSTR*)Realloc(m_lppEnvList, (m_dwEnvCount+1) * sizeof(LPSTR));
        if (m_lppEnvList) {
            m_lppEnvList[m_dwEnvCount] = (char*)Malloc(length * sizeof(char));
            if (m_lppEnvList[m_dwEnvCount] != NULL) {
                strcpy(m_lppEnvList[m_dwEnvCount], lpStr);
                ++m_dwEnvCount;
                qsort(m_lppEnvList, m_dwEnvCount, sizeof(LPSTR), compare);
            }
        }
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
    Safefree(lpStr);
}

char*
CPerlHost::GetChildDir(void)
{
    char* ptr;
    size_t length;

    Newx(ptr, MAX_PATH+1, char);
    m_vDir.GetCurrentDirectoryA(MAX_PATH+1, ptr);
    length = strlen(ptr);
    if (length > 3) {
        if ((ptr[length-1] == '\\') || (ptr[length-1] == '/'))
            ptr[length-1] = 0;
    }
    return ptr;
}

void
CPerlHost::FreeChildDir(char* pStr)
{
    Safefree(pStr);
}

LPSTR
CPerlHost::CreateLocalEnvironmentStrings(VDir &vDir)
{
    LPSTR lpStr, lpPtr, lpEnvPtr, lpTmp, lpLocalEnv, lpAllocPtr;
    DWORD dwSize, dwEnvIndex;
    int nLength, compVal;

    // get the process environment strings
    lpAllocPtr = lpTmp = (LPSTR)win32_getenvironmentstrings();

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

    Newx(lpStr, dwSize, char);
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
            // so copy the strings into place if not an override
            char *ptr = strchr(lpLocalEnv, '=');
            if(ptr && ptr[1]) {
                strcpy(lpStr, lpLocalEnv);
                lpStr += strlen(lpLocalEnv) + 1;
            }
            lpLocalEnv = GetIndex(dwEnvIndex);
        }

        // add final NULL
        *lpStr = '\0';
    }

    // release the process environment strings
    win32_freeenvironmentstrings(lpAllocPtr);

    return lpPtr;
}

void
CPerlHost::Reset(void)
{
    if(m_lppEnvList != NULL) {
        for(DWORD index = 0; index < m_dwEnvCount; ++index) {
            Free(m_lppEnvList[index]);
            m_lppEnvList[index] = NULL;
        }
    }
    m_dwEnvCount = 0;
    Free(m_lppEnvList);
    m_lppEnvList = NULL;
}

void
CPerlHost::Clearenv(void)
{
    char ch;
    LPSTR lpPtr, lpStr, lpEnvPtr;
    if (m_lppEnvList != NULL) {
        /* set every entry to an empty string */
        for(DWORD index = 0; index < m_dwEnvCount; ++index) {
            char* ptr = strchr(m_lppEnvList[index], '=');
            if(ptr) {
                *++ptr = 0;
            }
        }
    }

    /* get the process environment strings */
    lpStr = lpEnvPtr = (LPSTR)win32_getenvironmentstrings();

    /* step over current directory stuff */
    while(*lpStr == '=')
        lpStr += strlen(lpStr) + 1;

    while(*lpStr) {
        lpPtr = strchr(lpStr, '=');
        if(lpPtr) {
            ch = *++lpPtr;
            *lpPtr = 0;
            Add(lpStr);
            if (m_bTopLevel)
                (void)win32_putenv(lpStr);
            *lpPtr = ch;
        }
        lpStr += strlen(lpStr) + 1;
    }

    win32_freeenvironmentstrings(lpEnvPtr);
}


char*
CPerlHost::Getenv(const char *varname)
{
    if (!m_bTopLevel) {
        char *pEnv = Find(varname);
        if (pEnv && *pEnv)
            return pEnv;
    }
    return win32_getenv(varname);
}

int
CPerlHost::Putenv(const char *envstring)
{
    Add(envstring);
    if (m_bTopLevel)
        return win32_putenv(envstring);

    return 0;
}

int
CPerlHost::Chdir(const char *dirname)
{
    int ret;
    if (!dirname) {
        errno = ENOENT;
        return -1;
    }
    ret = m_vDir.SetCurrentDirectoryA((char*)dirname);
    if(ret < 0) {
        errno = ENOENT;
    }
    return ret;
}

static inline VMemNL * VDToVM(VDir * pvd) {
    VDir * vd = (VDir *)pvd;
    size_t p_szt = ((size_t)vd)-((size_t)((CPerlHost*)NULL)->GetDir());
    CPerlHost * cph = (CPerlHost*)p_szt;
    VMemNL * vm = cph->GetMem();
    return vm;
}

#endif /* ___PerlHost_H___ */
