
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	perllib.c
 * DESCRIPTION	:	Library functions for parsing and running Perl.
 *                  This is based on the perllib.c file of Win32 port.
 * Author		:	SGP
 * Date			:	January 2001.
 *
 */

/*
 * "The Road goes ever on and on, down from the door where it began."
 */



#include "EXTERN.h"
#include "perl.h"


#ifdef PERL_OBJECT
#define NO_XSLOCKS
#endif

//CHKSGP
//Including this is giving premature end-of-file error during compilation
//#include "XSUB.h"

#ifdef PERL_IMPLICIT_SYS

#include "nw5iop.h"
#include <fcntl.h>

#endif	//PERL_IMPLICIT_SYS


#ifdef PERL_IMPLICIT_SYS

#include "nwperlhost.h"
#define w32_internal_host		(PL_sys_intern.internal_host)	// (J)


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
#ifdef PERL_OBJECT
	    CPerlObj* pPerl = (CPerlObj*)my_perl;
#endif
	    w32_internal_host = pHost;
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
#ifdef PERL_OBJECT
	    CPerlObj* pPerl = (CPerlObj*)my_perl;
#endif
		//The following Should be uncommented - CHKSGP
	    w32_internal_host = pHost;
	}
    }
    return my_perl;
}

EXTERN_C void
nw_delete_internal_host(void *h)
{
    CPerlHost *host = (CPerlHost*)h;
    if(host && h)
    {
        delete host;
        host=NULL;
        h=NULL;
    }
}

#ifdef PERL_OBJECT

EXTERN_C void
perl_construct(PerlInterpreter* my_perl)
{
    CPerlObj* pPerl = (CPerlObj*)my_perl;
    try
    {
	Perl_construct();
    }
    catch(...)
    {
	win32_fprintf(stderr, "%s\n",
		      "Error: Unable to construct data structures");
	perl_free(my_perl);
    }
}

EXTERN_C void
perl_destruct(PerlInterpreter* my_perl)
{
    CPerlObj* pPerl = (CPerlObj*)my_perl;
#ifdef DEBUGGING
    Perl_destruct();
#else
    try
    {
	Perl_destruct();
    }
    catch(...)
    {
    }
#endif
}

EXTERN_C void
perl_free(PerlInterpreter* my_perl)
{
    CPerlObj* pPerl = (CPerlObj*)my_perl;
    void *host = w32_internal_host;
#ifdef DEBUGGING
    Perl_free();
#else
    try
    {
	Perl_free();
    }
    catch(...)
    {
    }
#endif
    win32_delete_internal_host(host);
    PERL_SET_THX(NULL);
}

EXTERN_C int
perl_run(PerlInterpreter* my_perl)
{
    CPerlObj* pPerl = (CPerlObj*)my_perl;
    int retVal;
#ifdef DEBUGGING
    retVal = Perl_run();
#else
    try
    {
	retVal = Perl_run();
    }
    catch(...)
    {
	win32_fprintf(stderr, "Error: Runtime exception\n");
	retVal = -1;
    }
#endif
    return retVal;
}

EXTERN_C int
perl_parse(PerlInterpreter* my_perl, void (*xsinit)(CPerlObj*), int argc, char** argv, char** env)
{
    int retVal;
    CPerlObj* pPerl = (CPerlObj*)my_perl;
#ifdef DEBUGGING
    retVal = Perl_parse(xsinit, argc, argv, env);
#else
    try
    {
	retVal = Perl_parse(xsinit, argc, argv, env);
    }
    catch(...)
    {
	win32_fprintf(stderr, "Error: Parse exception\n");
	retVal = -1;
    }
#endif
    *win32_errno() = 0;
    return retVal;
}

#undef PL_perl_destruct_level
#define PL_perl_destruct_level int dummy

#endif /* PERL_OBJECT */
#endif /* PERL_IMPLICIT_SYS */


