#include "EXTERN.h"
#include "perl.h"

#ifdef PERL_OBJECT

#define NO_XSLOCKS
#include "XSUB.H"
#include "win32iop.h"

#include <fcntl.h>
#include "perlhost.h"


char *staticlinkmodules[] = {
    "DynaLoader",
    NULL,
};

EXTERN_C void boot_DynaLoader _((CV* cv _CPERLarg));

static void
xs_init(CPERLarg)
{
    char *file = __FILE__;
    dXSUB_SYS;
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

CPerlObj *pPerl;

#undef PERL_SYS_INIT
#define PERL_SYS_INIT(a, c)

int
main(int argc, char **argv, char **env)
{
    CPerlHost host;
    int exitstatus = 1;
#ifndef __BORLANDC__
    /* XXX this _may_ be a problem on some compilers (e.g. Borland) that
     * want to free() argv after main() returns.  As luck would have it,
     * Borland's CRT does the right thing to argv[0] already. */
    char szModuleName[MAX_PATH];

    GetModuleFileName(NULL, szModuleName, sizeof(szModuleName));
    argv[0] = szModuleName;
#endif

    if (!host.PerlCreate())
	exit(exitstatus);

    exitstatus = host.PerlParse(xs_init, argc, argv, NULL);

    if (!exitstatus)
	exitstatus = host.PerlRun();

    host.PerlDestroy();

    return exitstatus;
}

#else  /* PERL_OBJECT */

#ifdef __GNUC__
/*
 * GNU C does not do __declspec()
 */
#define __declspec(foo) 

/* Mingw32 defaults to globing command line 
 * This is inconsistent with other Win32 ports and 
 * seems to cause trouble with passing -DXSVERSION=\"1.6\" 
 * So we turn it off like this:
 */
int _CRT_glob = 0;

#endif


__declspec(dllimport) int RunPerl(int argc, char **argv, char **env, void *ios);

int
main(int argc, char **argv, char **env)
{
#ifndef __BORLANDC__
    /* XXX this _may_ be a problem on some compilers (e.g. Borland) that
     * want to free() argv after main() returns.  As luck would have it,
     * Borland's CRT does the right thing to argv[0] already. */
    char szModuleName[MAX_PATH];
    GetModuleFileName(NULL, szModuleName, sizeof(szModuleName));
    argv[0] = szModuleName;
#endif
    return RunPerl(argc, argv, env, (void*)0);
}

#endif  /* PERL_OBJECT */
