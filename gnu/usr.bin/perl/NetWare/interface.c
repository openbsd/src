
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME     :   interface.c
 * DESCRIPTION  :   Calling Perl APIs.
 * Author       :   SGP
 * Date	Created :   January 2001.
 * Date Modified:   July 2nd 2001.
 */



#include "interface.h"
#include "nwtinfo.h"

static void xs_init(pTHX);

EXTERN_C int RunPerl(int argc, char **argv, char **env);
EXTERN_C void Perl_nw5_init(int *argcp, char ***argvp);
EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

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

bool
ClsPerlHost::RegisterWithThreadTable()
{
	return(fnRegisterWithThreadTable());
}

bool
ClsPerlHost::UnregisterWithThreadTable()
{
	return(fnUnregisterWithThreadTable());
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

void
ClsPerlHost::PerlDestroy(PerlInterpreter *my_perl)
{
	perl_destruct(my_perl);		// Destructor for Perl.
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

	#ifdef PERL_GLOBAL_STRUCT
		#define PERLVAR(var,type)
		#define PERLVARA(var,type)
		#define PERLVARI(var,type,init) PL_Vars.var = init;
		#define PERLVARIC(var,type,init) PL_Vars.var = init;

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

		exitstatus = nlm.PerlParse(my_perl, argc, argv, env);
		if(exitstatus == 0)
		{
			#if defined(TOP_CLONE) && defined(USE_ITHREADS)		// XXXXXX testing
				new_perl = perl_clone(my_perl, 1);

				exitstatus = perl_run(new_perl);	// Run Perl.
				PERL_SET_THX(my_perl);
			#else
				exitstatus = nlm.PerlRun(my_perl);
			#endif
		}
		nlm.PerlDestroy(my_perl);
	}
	if(my_perl)
		nlm.PerlFree(my_perl);

	#ifdef USE_ITHREADS
		if (new_perl)
		{
			PERL_SET_THX(new_perl);
			nlm.PerlDestroy(new_perl);
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
}

