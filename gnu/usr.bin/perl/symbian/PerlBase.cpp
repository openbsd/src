/* Copyright (c) 2004-2005 Nokia. All rights reserved. */

/* The CPerlBase class is licensed under the same terms as Perl itself. */

/* See PerlBase.pod for documentation. */

#define PERLBASE_CPP

#include <e32cons.h>
#include <e32keys.h>
#include <utf.h>

#include "PerlBase.h"

const TUint KPerlConsoleBufferMaxTChars = 0x0200;
const TUint KPerlConsoleNoPos           = 0xffff;

CPerlBase::CPerlBase()
{
}

EXPORT_C void CPerlBase::Destruct()
{
    dTHX;
    iState = EPerlDestroying;
    if (iConsole) {
        iConsole->Printf(_L("[Any key to continue]"));
        iConsole->Getch();
    }
    if (iPerl)  {
        (void)perl_destruct(iPerl);
        perl_free(iPerl);
        iPerl = NULL;
        PERL_SYS_TERM();
    }
    if (iConsole) {
        delete iConsole;
        iConsole = NULL;
    }
    if (iConsoleBuffer) {
        free(iConsoleBuffer);
        iConsoleBuffer = NULL;
    }
#ifdef PERL_GLOBAL_STRUCT
    if (iVars) {
        PerlInterpreter* my_perl = NULL;
        free_global_struct(iVars);
        iVars = NULL;
    }
#endif
}

CPerlBase::~CPerlBase()
{
    Destruct();
}

EXPORT_C CPerlBase* CPerlBase::NewInterpreter(TBool aCloseStdlib,
                                               void (*aStdioInitFunc)(void*),
                                               void *aStdioInitCookie)
{
   CPerlBase* self = new (ELeave) CPerlBase;
   self->iCloseStdlib     = aCloseStdlib;
   self->iStdioInitFunc   = aStdioInitFunc;
   self->iStdioInitCookie = aStdioInitCookie;
   self->ConstructL();
   PERL_APPCTX_SET(self);
   return self;
}

EXPORT_C CPerlBase* CPerlBase::NewInterpreterL(TBool aCloseStdlib,
                                               void (*aStdioInitFunc)(void*),
                                               void *aStdioInitCookie)
{
    CPerlBase* self =
      CPerlBase::NewInterpreterLC(aCloseStdlib,
                                  aStdioInitFunc,
                                  aStdioInitCookie);
    CleanupStack::Pop(self);
    return self;
}

EXPORT_C CPerlBase* CPerlBase::NewInterpreterLC(TBool aCloseStdlib,
                                                void (*aStdioInitFunc)(void*),
                                                void *aStdioInitCookie)
{
    CPerlBase* self = new (ELeave) CPerlBase;
    CleanupStack::PushL(self);
    self->iCloseStdlib     = aCloseStdlib;
    self->iStdioInitFunc   = aStdioInitFunc;
    self->iStdioInitCookie = aStdioInitCookie;
    self->ConstructL();
    PERL_APPCTX_SET(self);
    return self;
}

static int _console_stdin(void* cookie, char* buf, int n)
{
    return ((CPerlBase*)cookie)->ConsoleRead(0, buf, n);
}

static int _console_stdout(void* cookie, const char* buf, int n)
{
    return ((CPerlBase*)cookie)->ConsoleWrite(1, buf, n);
}

static int _console_stderr(void* cookie, const char* buf, int n)
{
    return ((CPerlBase*)cookie)->ConsoleWrite(2, buf, n);
}

void CPerlBase::StdioRewire(void *arg) {
    _REENT->_sf[0]._cookie = (void*)this;
    _REENT->_sf[0]._read   = &_console_stdin;
    _REENT->_sf[0]._write  = 0;
    _REENT->_sf[0]._seek   = 0;
    _REENT->_sf[0]._close  = 0;

    _REENT->_sf[1]._cookie = (void*)this;
    _REENT->_sf[1]._read   = 0;
    _REENT->_sf[1]._write  = &_console_stdout;
    _REENT->_sf[1]._seek   = 0;
    _REENT->_sf[1]._close  = 0;

    _REENT->_sf[2]._cookie = (void*)this;
    _REENT->_sf[2]._read   = 0;
    _REENT->_sf[2]._write  = &_console_stderr;
    _REENT->_sf[2]._seek   = 0;
    _REENT->_sf[2]._close  = 0;
}

void CPerlBase::ConstructL()
{
    iState = EPerlNone;
#ifdef PERL_GLOBAL_STRUCT
    PerlInterpreter *my_perl = 0;
    iVars = init_global_struct();
    User::LeaveIfNull(iVars);
#endif
    iPerl = perl_alloc();
    User::LeaveIfNull(iPerl);
    iState = EPerlAllocated;
    perl_construct(iPerl); // returns void
    if (!iStdioInitFunc) {
        iConsole =
          Console::NewL(_L("Perl Console"),
                        TSize(KConsFullScreen, KConsFullScreen));
        iConsoleBuffer =
          (TUint16*)malloc(sizeof(TUint) *
                           KPerlConsoleBufferMaxTChars);
        User::LeaveIfNull(iConsoleBuffer);
        iConsoleUsed = 0;
#ifndef USE_PERLIO
        iStdioInitFunc = &StdioRewire;
#endif
    }
    if (iStdioInitFunc)
        iStdioInitFunc(iStdioInitCookie);
    iReadFunc  = NULL;
    iWriteFunc = NULL;
    iState = EPerlConstructed;
}

EXPORT_C PerlInterpreter* CPerlBase::GetInterpreter()
{
    return (PerlInterpreter*) iPerl;
}

#ifdef PERL_MINIPERL
static void boot_DynaLoader(pTHX_ CV* cv) { }
#else
EXTERN_C void boot_DynaLoader(pTHX_ CV* cv);
#endif

static void xs_init(pTHX)
{
    dXSUB_SYS;
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, __FILE__);
}

EXPORT_C TInt CPerlBase::RunScriptL(const TDesC& aFileName,
                                    int argc,
                                    char **argv,
                                    char *envp[]) {
    TBuf8<KMaxFileName> scriptUtf8;
    TInt error;
    error = CnvUtfConverter::ConvertFromUnicodeToUtf8(scriptUtf8, aFileName);
    User::LeaveIfError(error);
    char *filename = (char*)scriptUtf8.PtrZ();
    struct stat st;
    if (stat(filename, &st) == -1)
        return KErrNotFound;
    if (argc < 2)
        return KErrGeneral; /* Anything better? */
    char **Argv = (char**)malloc(argc * sizeof(char*));
    User::LeaveIfNull(Argv);
    TCleanupItem ArgvCleanupItem = TCleanupItem(free, Argv);
    CleanupStack::PushL(ArgvCleanupItem);
    Argv[0] = "perl";
    if (argv && argc > 2)
        for (int i = 2; i < argc - 1; i++)
            Argv[i] = argv[i];
    Argv[argc - 1] = filename;
    error = this->ParseAndRun(argc, Argv, envp);
    CleanupStack::PopAndDestroy(Argv);
    Argv = 0;
    return error == 0 ? KErrNone : KErrGeneral;
}


EXPORT_C int CPerlBase::Parse(int argc, char *argv[], char *envp[])
{
    if (iState == EPerlConstructed) {
        const char* const NullArgv[] = { "perl", "-e", "0" };
        if (argc == 0 || argv == 0) {
            argc = 3;
            argv = (char**) NullArgv;
        }
        PERL_SYS_INIT(&argc, &argv);
        int parsed = perl_parse(iPerl, xs_init, argc, argv, envp);
        if (parsed == 0)
            iState = EPerlParsed;
        return parsed;
    } else
        return -1;
}

EXPORT_C void CPerlBase::SetupExit()
{
    if (iState == EPerlParsed) {
        diTHX;
        PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
	// PL_perl_destruct level of 2 would be nice but
	// it causes "Unbalanced scopes" for some reason.
        PL_perl_destruct_level = 1;
    }
}

EXPORT_C int CPerlBase::Run()
{
    if (iState == EPerlParsed) {
        SetupExit();
        iState = EPerlRunning;
        int ran = perl_run(iPerl);
        iState = (ran == 0) ? EPerlSuccess : EPerlFailure;
        return ran;
    } else
        return -1;
}

EXPORT_C int CPerlBase::ParseAndRun(int argc, char *argv[], char *envp[])
{
    int parsed = Parse(argc, argv, envp);
    int ran    = (parsed == 0) ? Run() : -1;
    return ran;
}

int CPerlBase::ConsoleReadLine()
{
    if (!iConsole)
        return -EIO;

    TUint currX  = KPerlConsoleNoPos;
    TUint currY  = KPerlConsoleNoPos;
    TUint prevX  = KPerlConsoleNoPos;
    TUint prevY  = KPerlConsoleNoPos;
    TUint maxX   = KPerlConsoleNoPos;
    TUint offset = 0;

    for (;;) {
        TKeyCode code = iConsole->Getch();

        if (code == EKeyLineFeed || code == EKeyEnter) {
            if (offset < KPerlConsoleBufferMaxTChars) {
                iConsoleBuffer[offset++] = '\n';
                iConsole->Printf(_L("\n"));
                iConsoleBuffer[offset++] = 0;
            }
            break;
        }
        else {
            TBool doBackward  = EFalse;
            TBool doBackspace = EFalse;

            prevX = currX;
            prevY = currY;
            if (code == EKeyBackspace) {
                if (offset > 0) {
                    iConsoleBuffer[--offset] = 0;
                    doBackward  = ETrue;
                    doBackspace = ETrue;
                }
            }
            else if (offset < KPerlConsoleBufferMaxTChars) {
                TChar ch = TChar(code);

                if (ch.IsPrint()) {
                    iConsoleBuffer[offset++] = (unsigned short)code;
                    iConsole->Printf(_L("%c"), code);
                }
            }
            currX = iConsole->WhereX();
            currY = iConsole->WhereY();
            if (maxX  == KPerlConsoleNoPos && prevX != KPerlConsoleNoPos &&
                prevY != KPerlConsoleNoPos && currY == prevY + 1)
                maxX = prevX;
            if (doBackward) {
                if (currX > 0)
                    iConsole->SetPos(currX - 1);
                else if (currY > 0)
                    iConsole->SetPos(maxX, currY - 1);
                if (doBackspace) {
                    TUint nowX = iConsole->WhereX();
                    TUint nowY = iConsole->WhereY();
                    iConsole->Printf(_L(" ")); /* scrub */
                    iConsole->SetPos(nowX, nowY);
                }
            }
         }
    }

    return offset;
}

int CPerlBase::ConsoleRead(const int fd, char* buf, int n)
{
    if (iReadFunc)
        return iReadFunc(fd, buf, n);

    if (!iConsole) {
        errno = EIO;
        return -1;
    }

    if (n < 0) {
        errno = EINVAL;
        return -1;
    }

    if (n == 0)
        return 0;

    TBuf8<4 * KPerlConsoleBufferMaxTChars> aBufferUtf8;
    TBuf16<KPerlConsoleBufferMaxTChars>    aBufferUtf16;
    int length = ConsoleReadLine();
    int i;

    iConsoleUsed += length;

    aBufferUtf16.SetLength(length);
    for (i = 0; i < length; i++)
        aBufferUtf16[i] = iConsoleBuffer[i];
    aBufferUtf8.SetLength(4 * length);

    CnvUtfConverter::ConvertFromUnicodeToUtf8(aBufferUtf8, aBufferUtf16);

    char *pUtf8 = (char*)aBufferUtf8.PtrZ();
    int nUtf8 = aBufferUtf8.Size();
    if (nUtf8 > n)
        nUtf8 = n; /* Potential data loss. */
#ifdef PERL_SYMBIAN_CONSOLE_UTF8
    for (i = 0; i < nUtf8; i++)
        buf[i] = pUtf8[i];
#else
    dTHX;
    for (i = 0; i < nUtf8; i+= UTF8SKIP(pUtf8 + i)) {
        unsigned long u = utf8_to_uvchr_buf((U8*)(pUtf8 + i),
                                            (U8*)(pUtf8 + nUtf8),
                                            0);
        if (u > 0xFF) {
            iConsole->Printf(_L("(keycode > 0xFF)\n"));
            buf[i] = 0;
            return -1;
        }
        buf[i] = u;
    }
#endif
    if (nUtf8 < n)
        buf[nUtf8] = 0;
    return nUtf8;
}

int CPerlBase::ConsoleWrite(const int fd, const char* buf, int n)
{
    if (iWriteFunc)
        return iWriteFunc(fd, buf, n);

    if (!iConsole) {
        errno = EIO;
        return -1;
    }

    if (n < 0) {
        errno = EINVAL;
        return -1;
    }

    if (n == 0)
        return 0;

    int wrote = 0;
#ifdef PERL_SYMBIAN_CONSOLE_UTF8
    dTHX;
    if (is_utf8_string((U8*)buf, n)) {
        for (int i = 0; i < n; i += UTF8SKIP(buf + i)) {
            TChar u = valid_utf8_to_uvchr((U8*)(buf + i), 0);
            iConsole->Printf(_L("%c"), u);
            wrote++;
        }
    } else {
        iConsole->Printf(_L("(malformed utf8: "));
        for (int i = 0; i < n; i++)
            iConsole->Printf(_L("%02x "), buf[i]);
        iConsole->Printf(_L(")\n"));
    }
#else
    for (int i = 0; i < n; i++) {
        iConsole->Printf(_L("%c"), buf[i]);
    }
    wrote = n;
#endif
    iConsoleUsed += wrote;
    return n;
}

