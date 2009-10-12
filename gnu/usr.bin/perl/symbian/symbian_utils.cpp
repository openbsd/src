/*
 *      symbian_utils.cpp
 *
 *      Copyright (c) Nokia 2004-2005.  All rights reserved.
 *      This code is licensed under the same terms as Perl itself.
 *
 */

#define SYMBIAN_UTILS_CPP
#include <e32base.h>
#include <e32std.h>
#include <utf.h>
#include <hal.h>

#include <eikenv.h>

#include <string.h>
#include <ctype.h>

#include "PerlUi.h"
#include "PerlBase.h"
#include "PerlUtil.h"

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

extern "C" {
    EXPORT_C int symbian_sys_init(int *argcp, char ***argvp)
    {
#ifdef PERL_GLOBAL_STRUCT /* Avoid unused variable warning. */
        dVAR;
#endif
        (void)times(&PL_timesbase);
        return 0;
    }
    XS(XS_PerlApp_TextQuery) // Can't be made static because of XS().
    {
        dXSARGS;
	if (items != 0)
	    Perl_croak(aTHX_ "PerlApp::TextQuery: no arguments, please");
	SP -= items;
	// TODO: parse arguments for title, prompt, and maxsize.
	// Suggested syntax:
	// TextQuery(title => ..., prompt => ..., maxsize => ...)
	// For an example see e.g. universal.c:XS_PerlIO_get_layers().
	_LIT(KTitle,  "Title");
	_LIT(KPrompt, "Prompt");
        HBufC* cData = HBufC::New(KPerlUiOneLinerSize);
	TBool cSuccess = EFalse;
	if (cData) {
	    TPtr cPtr(cData->Des());
	    if (CPerlUi::TextQueryDialogL(KTitle,
					  KPrompt,
					  cPtr,
					  KPerlUiOneLinerSize)) {
	        ST(0) = sv_2mortal(PerlUtil::newSvPVfromTDesC16(*cData));
		cSuccess = ETrue;
	    }
	    delete cData;
	}
	if (cSuccess)
	    XSRETURN(1);
	else
	    XSRETURN_UNDEF;
    }
    EXPORT_C void init_os_extras(void)
    {
        dTHX;
	char *file = __FILE__;
	dXSUB_SYS;
	newXS("PerlApp::TextQuery", XS_PerlApp_TextQuery, file);
    }
    EXPORT_C SSize_t symbian_read_stdin(const int fd, char *b, int n)
    {
#ifdef PERL_GLOBAL_STRUCT /* Avoid unused variable warning. */
        dVAR;
#endif
        if(!PL_appctx)
        	((CPerlBase*)PL_appctx) = CPerlBase::NewInterpreter();
        return ((CPerlBase*)PL_appctx)->ConsoleRead(fd, b, n);
    }
    EXPORT_C SSize_t symbian_write_stdout(const int fd, const char *b, int n)
    {
#ifdef PERL_GLOBAL_STRUCT /* Avoid unused variable warning. */
        dVAR;
#endif
        if(!PL_appctx)
        	((CPerlBase*)PL_appctx) = CPerlBase::NewInterpreter();
        return ((CPerlBase*)PL_appctx)->ConsoleWrite(fd, b, n);
    }
    static const char NullErr[] = "";
    EXPORT_C char* symbian_get_error_string(TInt error)
    {
	// CTextResolver seems to be unreliable, so we roll our own
        // at least for the basic Symbian errors (this does not cover
        // the various subsystems).
        dTHX;
        if (error >= 0)
            return strerror(error);
	error = -error; // flip
	const TInt KErrStringMax = 256;
	typedef struct {
	  const char* kerr;
	  const char* desc;
	} kerritem;
	static const kerritem kerrtable[] = {
	  { "None",           /*    0 */ "No error"},
	  { "NotFound",       /*   -1 */ "Unable to find the specified object"},
	  { "General",        /*   -2 */ "General (unspecified) error"},
	  { "Cancel",         /*   -3 */ "The operation was cancelled"},
	  { "NoMemory",       /*   -4 */ "Not enough memory"},
	  { "NotSupported",   /*   -5 */ "The operation requested is not supported"},
	  { "Argument",       /*   -6 */ "Bad request"},
	  { "TotalLossOfPrecision",
	                      /*   -7 */ "Total loss of precision"},
	  { "BadHandle",      /*   -8 */ "Bad object"},
	  { "Overflow",       /*   -9 */ "Overflow"},
	  { "Underflow",      /*  -10 */ "Underflow"},
	  { "AlreadyExists",  /*  -11 */ "Already exists"},
	  { "PathNotFound",   /*  -12 */ "Unable to find the specified folder"},
	  { "Died",           /*  -13 */ "Closed"},
	  { "InUse",          /*  -14 */
	    "The specified object is currently in use by another program"},
	  { "ServerTerminated",       /*  -15 */ "Server has closed"},
	  { "ServerBusy",     /*  -16 */ "Server busy"},
	  { "Completion",     /*  -17 */ "Completion error"},
	  { "NotReady",       /*  -18 */ "Not ready"},
	  { "Unknown",        /*  -19 */ "Unknown error"},
	  { "Corrupt",        /*  -20 */ "Corrupt"},
	  { "AccessDenied",   /*  -21 */ "Access denied"},
	  { "Locked",         /*  -22 */ "Locked"},
	  { "Write",          /*  -23 */ "Failed to write"},
	  { "DisMounted",     /*  -24 */ "Wrong disk present"},
	  { "Eof",            /*  -25 */ "Unexpected end of file"},
	  { "DiskFull",       /*  -26 */ "Disk full"},
	  { "BadDriver",      /*  -27 */ "Bad device driver"},
	  { "BadName",        /*  -28 */ "Bad name"},
	  { "CommsLineFail",  /*  -29 */ "Comms line failed"},
	  { "CommsFrame",     /*  -30 */ "Comms frame error"},
	  { "CommsOverrun",   /*  -31 */ "Comms overrun error"},
	  { "CommsParity",    /*  -32 */ "Comms parity error"},
	  { "TimedOut",       /*  -33 */ "Timed out"},
	  { "CouldNotConnect",/*  -34 */ "Failed to connect"},
	  { "CouldNotDisconnect",
	                      /* -35 */ "Failed to disconnect"},
	  { "Disconnected",   /* -36 */ "Disconnected"},
	  { "BadLibraryEntryPoint",
	                      /*  -37 */ "Bad library entry point"},
	  { "BadDescriptor",  /*  -38 */ "Bad descriptor"},
	  { "Abort",          /*  -39 */ "Interrupted"},
	  { "TooBig",         /*  -40 */ "Too big"},
	  { "DivideByZero",   /*  -41 */ "Divide by zero"},
	  { "BadPower",       /*  -42 */ "Batteries too low"},
	  { "DirFull",        /*  -43 */ "Folder full"},
	  { "KErrHardwareNotAvailable",
	                      /*  -44 */ "Hardware is not available"},
	  { "SessionClosed",  /*  -45 */ "Session was closed"},
	  { "PermissionDenied",
	                      /*  -46 */ "Permission denied"}
	};
	const TInt n = sizeof(kerrtable) / sizeof(kerritem *);
	TBuf8<KErrStringMax> buf8;
	if (error >= 0 && error < n) {
	  const char *kerr = kerrtable[error].kerr;
	  const char *desc = kerrtable[error].desc;
	  const TPtrC8 kerrp((const unsigned char *)kerr, strlen(kerr));
	  const TPtrC8 descp((const unsigned char *)desc, strlen(desc));
	  TBuf8<KErrStringMax> ckerr;
	  TBuf8<KErrStringMax> cdesc;
	  ckerr.Copy(kerrp);
	  cdesc.Copy(descp);
	  buf8.Format(_L8("K%S (%d) %S"), &ckerr, error, &cdesc);
		     
	} else {
	  buf8.Format(_L8("Symbian error %d"), error);
	}
        SV* sv = Perl_get_sv(aTHX_ "\005", TRUE); /* $^E or ${^OS_ERROR} */
        if (!sv)
            return (char*)NullErr;
        sv_setpv(sv, (const char *)buf8.PtrZ());
        return SvPV_nolen(sv);
    }
    EXPORT_C void symbian_sleep_usec(const long usec)
    {
        User::After((TTimeIntervalMicroSeconds32) usec);
    }
#define PERL_SYMBIAN_CLK_TCK 100
    EXPORT_C int symbian_get_cpu_time(long* sec, long* usec)
    {
        // The RThread().GetCpuTime() does not seem to work?
        // (it always returns KErrNotSupported)
        // TTimeIntervalMicroSeconds ti;
        // TInt err = me.GetCpuTime(ti);
        dTHX;
        TInt periodus; /* tick period in microseconds */
        if (HAL::Get(HALData::ESystemTickPeriod, periodus) != KErrNone)
            return -1;
        TUint  tick   = User::TickCount();
        if (PL_timesbase.tms_utime == 0) {
            PL_timesbase.tms_utime = tick;
            PL_clocktick = PERL_SYMBIAN_CLK_TCK;
        }
        tick -= PL_timesbase.tms_utime;
        TInt64 tickus = TInt64(tick) * TInt64(periodus);
        TInt64 tmps   = tickus / 1000000;
#ifdef __SERIES60_3X__
        if (sec)  *sec  = I64LOW(tmps);
        if (usec) *usec = I64LOW(tickus) - I64LOW(tmps) * 1000000;
#else
        if (sec)  *sec  = tmps.Low();
        if (usec) *usec = tickus.Low() - tmps.Low() * 1000000;
#endif //__SERIES60_3X__
        return 0;
    }
    EXPORT_C int symbian_usleep(unsigned int usec)
    {
        if (usec >= 1000000) {
            errno = EINVAL;
            return -1;
        }
        symbian_sleep_usec((const long) usec);
        return 0;
    }
#define SEC_USEC_TO_CLK_TCK(s, u) \
        (((s) * PERL_SYMBIAN_CLK_TCK) + (u / (1000000 / PERL_SYMBIAN_CLK_TCK)))
    EXPORT_C clock_t symbian_times(struct tms *tmsbuf) 
    {
        long s, u;
        if (symbian_get_cpu_time(&s, &u) == -1) {
            errno = EINVAL;
            return -1;
        } else {
            tmsbuf->tms_utime  = SEC_USEC_TO_CLK_TCK(s, u);
            tmsbuf->tms_stime  = 0;
            tmsbuf->tms_cutime = 0;
            tmsbuf->tms_cstime = 0;
            return tmsbuf->tms_utime;
        }
    }
    class CProcessWait : public CActive
    {
    public:
        CProcessWait() : CActive(EPriorityStandard) {
          CActiveScheduler::Add(this);
        }
#ifdef __WINS__
        TInt Wait(RThread& aProcess)
#else
        TInt Wait(RProcess& aProcess)
#endif
        {
            aProcess.Logon(iStatus);
            aProcess.Resume();
            SetActive();
            CActiveScheduler::Start();
            return iStatus.Int();
        }
    private:
      void DoCancel() {;}
      void RunL() {
          CActiveScheduler::Stop();
      }
    };
    class CSpawnIoRedirect : public CBase
    {
    public:
        CSpawnIoRedirect();
        // NOTE: there is no real implementation of I/O redirection yet.
    protected:
    private:
    };
    CSpawnIoRedirect::CSpawnIoRedirect()
    {
    }
    typedef enum {
        ESpawnNone = 0x00000000,
        ESpawnWait = 0x00000001
    } TSpawnFlag;
    static int symbian_spawn(const TDesC& aFilename,
                             const TDesC& aCommand,
                             const TSpawnFlag aFlag,
                             const CSpawnIoRedirect& aIoRedirect) {
        TInt error = KErrNone;
#ifdef __WINS__
        const TInt KStackSize = 0x1000;
        const TInt KHeapMin   = 0x1000;
        const TInt KHeapMax   = 0x100000;
        RThread proc;
        RLibrary lib;
        HBufC* command = aCommand.Alloc();
        error = lib.Load(aFilename);
        if (error == KErrNone) {
            TThreadFunction func = (TThreadFunction)(lib.Lookup(1));
            if (func)
                error = proc.Create(aFilename,
                                    func,
                                    KStackSize,
#ifdef __SERIES60_3X__                                    
                                    KHeapMin,
                                    KHeapMax,
                                    (TAny*)command,
#else
                                    (TAny*)command,
                                    &lib,
                                    RThread().Heap(),
                                    KHeapMin,
                                    KHeapMax,
#endif                                    
                                    EOwnerProcess);
            else
                error = KErrNotFound;
            lib.Close();
        }
        else
            delete command;
#else
        RProcess proc;
        error = proc.Create(aFilename, aCommand);
#endif
        if (error == KErrNone) {
            if ((TInt)aFlag & (TInt)ESpawnWait) {
              CProcessWait* w = new CProcessWait();
              if (w) {
                  error = w->Wait(proc);
                  delete w;
              } else
                  error = KErrNoMemory;
            } else
                proc.Resume();
            proc.Close();
        }
        return error;
    }
    static int symbian_spawner(const char *command, TSpawnFlag aFlags)
     {
        TBuf<KMaxFileName> aFilename;
        TBuf<KMaxFileName> aCommand;
        TSpawnFlag aSpawnFlags = ESpawnWait;
        CSpawnIoRedirect iord;
        char *p = (char*)command;

        // The recognized syntax is: "cmd [args] [&]".  Since one
        // cannot pass more than (an argv[0] and) an argv[1] to a
        // Symbian process anyway, not much is done to the cmd or
        // the args, only backslash quoting.

        // Strip leading whitespace.
        while (*p && isspace(*p)) p++;
        if (*p) {
            // Build argv[0].
            while (*p && !isspace(*p) && *p != '&') {
                if (*p == '\\') {
                    if (p[1]) {
                        aFilename.Append(p[1]);
                        p++;
                    }
                    
                }
                else
                    aFilename.Append(*p);
                p++;
            }

            if (*p) {
                // Skip whitespace between argv[0] and argv[1].
                while(*p && isspace(*p)) p++;
                // Build argv[1].
                if (*p) {
                    char *a = p;
                    char *b = p + 1;

                    while (*b) b++;
                    if (isspace(b[-1])) {
                        b--;
                        while (b > a && isspace(*b)) b--;
                        b++;
                    }
                    if (b > a && b[-1] == '&') {
                        // Parse backgrounding in any case,
                        // but turn it off only if wanted.
                        if ((aFlags & ESpawnWait))
                          aSpawnFlags =
                            (TSpawnFlag) (aSpawnFlags & ~ESpawnWait);
                        b--;
                        if (isspace(b[-1])) {
                            b--;
                            while (b > a && isspace(*b)) b--;
                            b++;
                        }
                    }
                    for (p = a; p < b; p++) {
                        if (*p == '\\') {
                            if (p[1])
                                aCommand.Append(p[1]);
                            p++;
                        }
                        else
                            aCommand.Append(*p);
                    }
                }
                // NOTE: I/O redirection is not yet done.
                // Implementing that may require a separate server.
            }
        }
        int spawned = symbian_spawn(aFilename, aCommand, aSpawnFlags, iord);
        return spawned == KErrNone ? 0 : -1;
    }
    EXPORT_C int symbian_do_spawn(const char *command)
    {
        return symbian_spawner(command, ESpawnWait);
    }
    EXPORT_C int symbian_do_spawn_nowait(const char *command)
    {
        return symbian_spawner(command, ESpawnNone);
    }
    EXPORT_C int symbian_do_aspawn(void* vreally, void* vmark, void* sp)
    {
        return -1;
    }
}

