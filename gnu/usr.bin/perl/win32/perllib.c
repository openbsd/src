/*
 * "The Road goes ever on and on, down from the door where it began."
 */


#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

static void xs_init _((void));

DllExport int
RunPerl(int argc, char **argv, char **env, void *iosubsystem)
{
    int exitstatus;
    PerlInterpreter *my_perl;

#ifdef PERL_GLOBAL_STRUCT
#define PERLVAR(var,type) /**/
#define PERLVARI(var,type,init) PL_Vars.var = init;
#define PERLVARIC(var,type,init) PL_Vars.var = init;
#include "perlvars.h"
#undef PERLVAR
#undef PERLVARI
#undef PERLVARIC
#endif

    PERL_SYS_INIT(&argc,&argv);

    perl_init_i18nl10n(1);

    if (!(my_perl = perl_alloc()))
	return (1);
    perl_construct( my_perl );
    PL_perl_destruct_level = 0;

    exitstatus = perl_parse( my_perl, xs_init, argc, argv, env);
    if (!exitstatus) {
	exitstatus = perl_run( my_perl );
    }

    perl_destruct( my_perl );
    perl_free( my_perl );

    PERL_SYS_TERM();

    return (exitstatus);
}

extern HANDLE w32_perldll_handle;

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
	w32_perldll_handle = hModule;
	break;

	/* The DLL is detaching from a process due to
	 * process termination or call to FreeLibrary.
	 */
    case DLL_PROCESS_DETACH:
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

/* Register any extra external extensions */

char *staticlinkmodules[] = {
    "DynaLoader",
    NULL,
};

EXTERN_C void boot_DynaLoader _((CV* cv));

static void
xs_init()
{
    char *file = __FILE__;
    dXSUB_SYS;
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

