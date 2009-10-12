/* Copyright (c) 2004-2005 Nokia. All rights reserved. */

/* The CPerlBase class is licensed under the same terms as Perl itself. */

/* See PerlBase.pod for documentation. */

#ifndef __PerlBase_h__
#define __PerlBase_h__

#include <e32base.h>

#if !defined(PERL_MINIPERL) && !defined(PERL_PERL)
#  ifndef PERL_IMPLICIT_CONTEXT
#    define PERL_IMPLICIT_CONTEXT
#  endif
#  ifndef PERL_MULTIPLICITY
#    define PERL_MULTIPLICITY
#  endif
#  ifndef PERL_GLOBAL_STRUCT
#    define PERL_GLOBAL_STRUCT
#  endif
#  ifndef PERL_GLOBAL_STRUCT_PRIVATE
#    define PERL_GLOBAL_STRUCT_PRIVATE
#  endif
#endif

#include "EXTERN.h"
#include "perl.h"

typedef enum {
   EPerlNone,
   EPerlAllocated,
   EPerlConstructed,
   EPerlParsed,
   EPerlRunning,
   EPerlTerminated,
   EPerlPaused,
   EPerlSuccess,
   EPerlFailure,
   EPerlDestroying
} TPerlState;

class PerlConsole;

class CPerlBase : public CBase
{
  public:
    CPerlBase();
    IMPORT_C virtual ~CPerlBase();
    IMPORT_C static CPerlBase* NewInterpreter(TBool aCloseStdlib = ETrue,
                                               void (*aStdioInitFunc)(void*) = NULL,
                                               void *aStdioInitCookie = NULL);
    IMPORT_C static CPerlBase* NewInterpreterL(TBool aCloseStdlib = ETrue,
                                               void (*aStdioInitFunc)(void*) = NULL,
                                               void *aStdioInitCookie = NULL);
    IMPORT_C static CPerlBase* NewInterpreterLC(TBool iCloseStdlib = ETrue,
                                                void (*aStdioInitFunc)(void*) = NULL,
                                                void *aStdioInitCookie = NULL);
    IMPORT_C TInt RunScriptL(const TDesC& aFileName, int argc = 2, char **argv = NULL, char *envp[] = NULL);
    IMPORT_C int  Parse(int argc = 0, char *argv[] = NULL, char *envp[] = NULL);
    IMPORT_C void SetupExit();
    IMPORT_C int  Run();
    IMPORT_C int  ParseAndRun(int argc = 0, char *argv[] = 0, char *envp[] = 0);
    IMPORT_C void Destruct();

    IMPORT_C PerlInterpreter* GetInterpreter();

    // These two really should be private but when not using PERLIO
    // certain C callback functions of STDLIB need to be able to call
    // these.  In general, all the console related functionality is
    // intentionally hidden and underdocumented.
    int               ConsoleRead(const int fd, char* buf, int n);
    int               ConsoleWrite(const int fd, const char* buf, int n);

    // Having these public does not feel right, but maybe someone needs
    // to do creative things with them.
    int               (*iReadFunc)(const int fd, char *buf, int n);
    int               (*iWriteFunc)(const int fd, const char *buf, int n);

   protected:
    PerlInterpreter*  iPerl;
#ifdef PERL_GLOBAL_STRUCT
    struct perl_vars* iVars;
#else
    void*             iAppCtx;
#endif
    TPerlState        iState;

   private:
    void              ConstructL();
    CConsoleBase*     iConsole;		/* The screen. */
    TUint16*          iConsoleBuffer;	/* The UTF-16 characters. */
    TUint             iConsoleUsed;	/* How many in iConsoleBuffer. */
    TBool             iCloseStdlib;	/* Close STDLIB on exit? */

    void              (*iStdioInitFunc)(void *);
    void*             iStdioInitCookie;

    int               ConsoleReadLine();
    void              StdioRewire(void*);
};

#define diTHX PerlInterpreter*  my_perl = iPerl
#define diVAR struct perl_vars* my_vars = iVars

#ifdef PERL_GLOBAL_STRUCT
#  define PERL_APPCTX_SET(c) ((c)->iVars->Gappctx = (c))
#else
#  define PERL_APPCTX_SET(c) (PL_appctx = (c))
#endif

#undef Copy
#undef CopyD /* For symmetry, not for Symbian reasons. */
#undef New
#define PerlCopy(s,d,n,t)	(MEM_WRAP_CHECK(n,t), (void)memcpy((char*)(d),(char*)(s), (n) * sizeof(t)))
#define PerlCopyD(s,d,n,t)	(MEM_WRAP_CHECK(n,t), memcpy((char*)(d),(char*)(s), (n) * sizeof(t)))
#define PerlNew(x,v,n,t)	(v = (MEM_WRAP_CHECK(n,t), (t*)safemalloc((MEM_SIZE)((n)*sizeof(t)))))

// This is like the Symbian _LIT() but without the embedded L prefix,
// which enables using #defined constants (which need to carry their
// own L prefix).
#ifndef _LIT_NO_L
# define _LIT_NO_L(n, s) static const TLitC<sizeof(s)/2> n={sizeof(s)/2-1,s}
#endif // #ifndef _LIT_NO_L

#endif /* #ifndef __PerlBase_h__ */

