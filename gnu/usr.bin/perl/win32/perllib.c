/*
 * "The Road goes ever on and on, down from the door where it began."
 */
#define PERLIO_NOT_STDIO 0
#include "EXTERN.h"
#include "perl.h"

#include "XSUB.h"

#ifdef PERL_IMPLICIT_SYS
#include "win32iop.h"
#include <fcntl.h>
#endif /* PERL_IMPLICIT_SYS */


/* Register any extra external extensions */
char *staticlinkmodules[] = {
    "DynaLoader",
    NULL,
};

EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

static void
xs_init(pTHX)
{
    char *file = __FILE__;
    dXSUB_SYS;
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

#ifdef PERL_IMPLICIT_SYS

#include "perlhost.h"

void
win32_checkTLS(PerlInterpreter *host_perl)
{
    dTHX;
    if (host_perl != my_perl) {
	int *nowhere = NULL;
        *nowhere = 0; 
	abort();
    }
}

EXTERN_C void
perl_get_host_info(struct IPerlMemInfo* perlMemInfo,
		   struct IPerlMemInfo* perlMemSharedInfo,
		   struct IPerlMemInfo* perlMemParseInfo,
		   struct IPerlEnvInfo* perlEnvInfo,
		   struct IPerlStdIOInfo* perlStdIOInfo,
		   struct IPerlLIOInfo* perlLIOInfo,
		   struct IPerlDirInfo* perlDirInfo,
		   struct IPerlSockInfo* perlSockInfo,
		   struct IPerlProcInfo* perlProcInfo)
{
    if (perlMemInfo) {
	Copy(&perlMem, &perlMemInfo->perlMemList, perlMemInfo->nCount, void*);
	perlMemInfo->nCount = (sizeof(struct IPerlMem)/sizeof(void*));
    }
    if (perlMemSharedInfo) {
	Copy(&perlMem, &perlMemSharedInfo->perlMemList, perlMemSharedInfo->nCount, void*);
	perlMemSharedInfo->nCount = (sizeof(struct IPerlMem)/sizeof(void*));
    }
    if (perlMemParseInfo) {
	Copy(&perlMem, &perlMemParseInfo->perlMemList, perlMemParseInfo->nCount, void*);
	perlMemParseInfo->nCount = (sizeof(struct IPerlMem)/sizeof(void*));
    }
    if (perlEnvInfo) {
	Copy(&perlEnv, &perlEnvInfo->perlEnvList, perlEnvInfo->nCount, void*);
	perlEnvInfo->nCount = (sizeof(struct IPerlEnv)/sizeof(void*));
    }
    if (perlStdIOInfo) {
	Copy(&perlStdIO, &perlStdIOInfo->perlStdIOList, perlStdIOInfo->nCount, void*);
	perlStdIOInfo->nCount = (sizeof(struct IPerlStdIO)/sizeof(void*));
    }
    if (perlLIOInfo) {
	Copy(&perlLIO, &perlLIOInfo->perlLIOList, perlLIOInfo->nCount, void*);
	perlLIOInfo->nCount = (sizeof(struct IPerlLIO)/sizeof(void*));
    }
    if (perlDirInfo) {
	Copy(&perlDir, &perlDirInfo->perlDirList, perlDirInfo->nCount, void*);
	perlDirInfo->nCount = (sizeof(struct IPerlDir)/sizeof(void*));
    }
    if (perlSockInfo) {
	Copy(&perlSock, &perlSockInfo->perlSockList, perlSockInfo->nCount, void*);
	perlSockInfo->nCount = (sizeof(struct IPerlSock)/sizeof(void*));
    }
    if (perlProcInfo) {
	Copy(&perlProc, &perlProcInfo->perlProcList, perlProcInfo->nCount, void*);
	perlProcInfo->nCount = (sizeof(struct IPerlProc)/sizeof(void*));
    }
}

EXTERN_C PerlInterpreter*
perl_alloc_override(struct IPerlMem** ppMem, struct IPerlMem** ppMemShared,
		 struct IPerlMem** ppMemParse, struct IPerlEnv** ppEnv,
		 struct IPerlStdIO** ppStdIO, struct IPerlLIO** ppLIO,
		 struct IPerlDir** ppDir, struct IPerlSock** ppSock,
		 struct IPerlProc** ppProc)
{
    PerlInterpreter *my_perl = NULL;
    CPerlHost* pHost = new CPerlHost(ppMem, ppMemShared, ppMemParse, ppEnv,
				     ppStdIO, ppLIO, ppDir, ppSock, ppProc);

    if (pHost) {
	my_perl = perl_alloc_using(pHost->m_pHostperlMem,
				   pHost->m_pHostperlMemShared,
				   pHost->m_pHostperlMemParse,
				   pHost->m_pHostperlEnv,
				   pHost->m_pHostperlStdIO,
				   pHost->m_pHostperlLIO,
				   pHost->m_pHostperlDir,
				   pHost->m_pHostperlSock,
				   pHost->m_pHostperlProc);
	if (my_perl) {
	    w32_internal_host = pHost;
	    pHost->host_perl  = my_perl;
	}
    }
    return my_perl;
}

EXTERN_C PerlInterpreter*
perl_alloc(void)
{
    PerlInterpreter* my_perl = NULL;
    CPerlHost* pHost = new CPerlHost();
    if (pHost) {
	my_perl = perl_alloc_using(pHost->m_pHostperlMem,
				   pHost->m_pHostperlMemShared,
				   pHost->m_pHostperlMemParse,
				   pHost->m_pHostperlEnv,
				   pHost->m_pHostperlStdIO,
				   pHost->m_pHostperlLIO,
				   pHost->m_pHostperlDir,
				   pHost->m_pHostperlSock,
				   pHost->m_pHostperlProc);
	if (my_perl) {
	    w32_internal_host = pHost;
            pHost->host_perl  = my_perl;
	}
    }
    return my_perl;
}

EXTERN_C void
win32_delete_internal_host(void *h)
{
    CPerlHost *host = (CPerlHost*)h;
    delete host;
}

#endif /* PERL_IMPLICIT_SYS */

EXTERN_C HANDLE w32_perldll_handle;

EXTERN_C DllExport int
RunPerl(int argc, char **argv, char **env)
{
    int exitstatus;
    PerlInterpreter *my_perl, *new_perl = NULL;

#ifndef __BORLANDC__
    /* XXX this _may_ be a problem on some compilers (e.g. Borland) that
     * want to free() argv after main() returns.  As luck would have it,
     * Borland's CRT does the right thing to argv[0] already. */
    char szModuleName[MAX_PATH];

    GetModuleFileName(NULL, szModuleName, sizeof(szModuleName));
    (void)win32_longpath(szModuleName);
    argv[0] = szModuleName;
#endif

#ifdef PERL_GLOBAL_STRUCT
#define PERLVAR(var,type) /**/
#define PERLVARA(var,type) /**/
#define PERLVARI(var,type,init) PL_Vars.var = init;
#define PERLVARIC(var,type,init) PL_Vars.var = init;
#include "perlvars.h"
#undef PERLVAR
#undef PERLVARA
#undef PERLVARI
#undef PERLVARIC
#endif

    PERL_SYS_INIT(&argc,&argv);

    if (!(my_perl = perl_alloc()))
	return (1);
    perl_construct(my_perl);
    PL_perl_destruct_level = 0;

    exitstatus = perl_parse(my_perl, xs_init, argc, argv, env);
    if (!exitstatus) {
#if defined(TOP_CLONE) && defined(USE_ITHREADS)		/* XXXXXX testing */
	new_perl = perl_clone(my_perl, 1);
	exitstatus = perl_run(new_perl);
	PERL_SET_THX(my_perl);
#else
	exitstatus = perl_run(my_perl);
#endif
    }

    perl_destruct(my_perl);
    perl_free(my_perl);
#ifdef USE_ITHREADS
    if (new_perl) {
	PERL_SET_THX(new_perl);
	perl_destruct(new_perl);
	perl_free(new_perl);
    }
#endif

    PERL_SYS_TERM();

    return (exitstatus);
}

EXTERN_C void
set_w32_module_name(void);

EXTERN_C void
EndSockets(void);


#ifdef __MINGW32__
EXTERN_C		/* GCC in C++ mode mangles the name, otherwise */
#endif
BOOL APIENTRY
DllMain(HANDLE hModule,		/* DLL module handle */
	DWORD fdwReason,	/* reason called */
	LPVOID lpvReserved)	/* reserved */
{ 
    switch (fdwReason) {
	/* The DLL is attaching to a process due to process
	 * initialization or a call to LoadLibrary.
	 */
    case DLL_PROCESS_ATTACH:
/* #define DEFAULT_BINMODE */
#ifdef DEFAULT_BINMODE
	setmode( fileno( stdin  ), O_BINARY );
	setmode( fileno( stdout ), O_BINARY );
	setmode( fileno( stderr ), O_BINARY );
	_fmode = O_BINARY;
#endif
	DisableThreadLibraryCalls((HMODULE)hModule);
	w32_perldll_handle = hModule;
	set_w32_module_name();
	break;

	/* The DLL is detaching from a process due to
	 * process termination or call to FreeLibrary.
	 */
    case DLL_PROCESS_DETACH:
        /* As long as we use TerminateProcess()/TerminateThread() etc. for mimicing kill()
           anything here had better be harmless if:
            A. Not called at all.
            B. Called after memory allocation for Heap has been forcibly removed by OS.
            PerlIO_cleanup() was done here but fails (B).
         */     
	EndSockets();
#if defined(USE_5005THREADS) || defined(USE_ITHREADS)
	if (PL_curinterp)
	    FREE_THREAD_KEY;
#endif
	break;

	/* The attached process creates a new thread. */
    case DLL_THREAD_ATTACH:
	break;

	/* The thread of the attached process terminates. */
    case DLL_THREAD_DETACH:
	break;

    default:
	break;
    }
    return TRUE;
}

#if defined(USE_ITHREADS) && defined(PERL_IMPLICIT_SYS)
EXTERN_C PerlInterpreter *
perl_clone_host(PerlInterpreter* proto_perl, UV flags) {
    dTHX;
    CPerlHost *h;
    h = new CPerlHost(*(CPerlHost*)PL_sys_intern.internal_host);
    proto_perl = perl_clone_using(proto_perl, flags,
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
    proto_perl->Isys_intern.internal_host = h;
    h->host_perl  = proto_perl;
    return proto_perl;
	
}
#endif
