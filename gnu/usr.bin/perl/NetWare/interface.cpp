
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	interface.c
 * DESCRIPTION	:	Perl parsing and running functions.
 * Author		:	SGP
 * Date			:	January 2001.
 *
 */



#include "interface.h"

#include "win32ish.h"		// For "BOOL", "TRUE" and "FALSE"


static void xs_init(pTHX);
//static void xs_init(pTHXo); //(J)

EXTERN_C int RunPerl(int argc, char **argv, char **env);
EXTERN_C void Perl_nw5_init(int *argcp, char ***argvp);
EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);	// (J) pTHXo_

EXTERN_C BOOL Remove_Thread_Ctx(void);


ClsPerlHost::ClsPerlHost()
{

}

ClsPerlHost::~ClsPerlHost()
{

}

ClsPerlHost::VersionNumber()
{
	return 0;
}

int
ClsPerlHost::PerlCreate(PerlInterpreter *my_perl)
{
/*	if (!(my_perl = perl_alloc()))		// Allocate memory for Perl.
		return (1);*/
    perl_construct(my_perl);

	return 1;
}

int
ClsPerlHost::PerlParse(PerlInterpreter *my_perl, int argc, char** argv, char** env)
{
	return(perl_parse(my_perl, xs_init, argc, argv, env));		// Parse the command line.
}

int
ClsPerlHost::PerlRun(PerlInterpreter *my_perl)
{
	return(perl_run(my_perl));	// Run Perl.
}

int
ClsPerlHost::PerlDestroy(PerlInterpreter *my_perl)
{
	int ret = perl_destruct(my_perl);		// Destructor for Perl.
////	perl_free(my_perl);			// Free the memory allocated for Perl.
	return(ret);
}

void
ClsPerlHost::PerlFree(PerlInterpreter *my_perl)
{
	perl_free(my_perl);			// Free the memory allocated for Perl.

	// Remove the thread context set during Perl_set_context
	// This is added here since for web script there is no other place this gets executed
	// and it cannot be included into cgi2perl.xs unless this symbol is exported.
	Remove_Thread_Ctx();
}

/*============================================================================================

 Function		:	xs_init

 Description	:	

 Parameters 	:	pTHX	(IN)	-	

 Returns		:	Nothing.

==============================================================================================*/

static void xs_init(pTHX)
//static void xs_init(pTHXo) //J
{
	char *file = __FILE__;

	dXSUB_SYS;
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}


EXTERN_C
int RunPerl(int argc, char **argv, char **env)
{
	int exitstatus = 0;
	ClsPerlHost nlm;

	PerlInterpreter *my_perl = NULL;		// defined in Perl.h
	PerlInterpreter *new_perl = NULL;		// defined in Perl.h

	//__asm{int 3};
	#ifdef PERL_GLOBAL_STRUCT
		#define PERLVAR(prefix,var,type)
		#define PERLVARA(prefix,var,type)
		#define PERLVARI(prefix,var,type,init) PL_Vars.prefix##var = init;
		#define PERLVARIC(prefix,var,type,init) PL_Vars.prefix##var = init;

		#include "perlvars.h"

		#undef PERLVAR
		#undef PERLVARA
		#undef PERLVARI
		#undef PERLVARIC
	#endif

	PERL_SYS_INIT(&argc, &argv);

	if (!(my_perl = perl_alloc()))		// Allocate memory for Perl.
		return (1);

	if(nlm.PerlCreate(my_perl))
	{
		PL_perl_destruct_level = 0;

		if(!nlm.PerlParse(my_perl, argc, argv, env))
		{
			#if defined(TOP_CLONE) && defined(USE_ITHREADS)		// XXXXXX testing
				#  ifdef PERL_OBJECT
					CPerlHost *h = new CPerlHost();
					new_perl = perl_clone_using(my_perl, 1,
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
					CPerlObj *pPerl = (CPerlObj*)new_perl;
				#  else
					new_perl = perl_clone(my_perl, 1);
				#  endif

				(void) perl_run(new_perl);	// Run Perl.
				PERL_SET_THX(my_perl);
			#else
				(void) nlm.PerlRun(my_perl);
			#endif
		}
		exitstatus = nlm.PerlDestroy(my_perl);
	}
	if(my_perl)
		nlm.PerlFree(my_perl);

	#ifdef USE_ITHREADS
		if (new_perl)
		{
			PERL_SET_THX(new_perl);
			exitstatus = nlm.PerlDestroy(new_perl);
			nlm.PerlFree(my_perl);
		}
	#endif

	PERL_SYS_TERM();
	return exitstatus;
}


// FUNCTION: AllocStdPerl
//
// DESCRIPTION:
//	Allocates a standard perl handler that other perl handlers
//	may delegate to. You should call FreeStdPerl to free this
//	instance when you are done with it.
//
IPerlHost* AllocStdPerl()
{
	return (IPerlHost*) new ClsPerlHost();
}


// FUNCTION: FreeStdPerl
//
// DESCRIPTION:
//	Frees an instance of a standard perl handler allocated by
//	AllocStdPerl.
//
void FreeStdPerl(IPerlHost* pPerlHost)
{
	if (pPerlHost)
		delete (ClsPerlHost*) pPerlHost;
////		delete pPerlHost;
}

