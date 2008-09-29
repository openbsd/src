/* Copyright (c) 2004-2005 Nokia. All rights reserved. */

/* The PerlApp application is licensed under the same terms as Perl itself.
 *
 * Note that this PerlApp is for Symbian/Series 60/80/UIQ smartphones
 * and it has nothing whatsoever to do with the ActiveState PerlApp. */

#include "PerlApp.h"

#include <apparc.h>
#include <e32base.h>
#include <e32cons.h>
#include <eikenv.h>
#include <bautils.h>
#include <eikappui.h>
#include <utf.h>
#include <f32file.h>

#include <coemain.h>

#ifndef PerlAppMinimal

#include "PerlApp.hrh"

#endif //#ifndef PerlAppMinimal

#define PERL_GLOBAL_STRUCT
#define PERL_GLOBAL_STRUCT_PRIVATE

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "PerlBase.h"
#include "PerlUtil.h"

#define symbian_get_vars() Dll::Tls() // Not visible from perlXYZ.lib?

const TUid KPerlAppUid = {
#ifdef PerlAppMinimalUid
  PerlAppMinimalUid
#else
  0x102015F6
#endif
};

_LIT(KDefaultScript, "default.pl");

#ifdef PerlAppMinimalName
_LIT_NO_L(KAppName, PerlAppMinimalName);
#else
_LIT(KAppName, "PerlApp");
#endif

#ifndef PerlAppMinimal

_LIT_NO_L(KFlavor, PERL_SYMBIANSDK_FLAVOR);
_LIT(KAboutFormat,
     "Perl %d.%d.%d, Symbian port %d.%d.%d, built for %S SDK %d.%d");
_LIT(KCopyrightFormat,
     "Copyright 1987-2005 Larry Wall and others, Symbian port Copyright Nokia 2004-2005");
_LIT(KInboxPrefix, "\\System\\Mail\\");
_LIT(KScriptPrefix, "\\Perl\\");

_LIT8(KModulePrefix, SITELIB); // SITELIB from Perl config.h

typedef TBuf<256>  TMessageBuffer;
typedef TBuf8<256> TPeekBuffer;
typedef TBuf8<256> TFileName8;

#endif // #ifndef PerlAppMinimal

static void DoRunScriptL(TFileName aScriptName);

TUid CPerlAppApplication::AppDllUid() const
{
    return KPerlAppUid;
}

enum TPerlAppPanic
{
    EPerlAppCommandUnknown = 1
};

void Panic(TPerlAppPanic aReason)
{
    User::Panic(KAppName, aReason);
}

#ifndef PerlAppMinimal

// The isXXX() come from the Perl headers.
#define FILENAME_IS_ABSOLUTE(n) \
        (isALPHA(((n)[0])) && ((n)[1]) == ':' && ((n)[2]) == '\\')

static TBool IsInPerl(TFileName aFileName)
{
    TInt offset = aFileName.FindF(KScriptPrefix);
    return ((offset == 0 && // \foo
             aFileName[0] == '\\')
            ||
            (offset == 2 && // x:\foo
             FILENAME_IS_ABSOLUTE(aFileName)));
}

static TBool IsInInbox(TFileName aFileName)
{
    TInt offset = aFileName.FindF(KInboxPrefix);
    return ((offset == 0 && // \foo
             aFileName[0] == '\\')
            ||
            (offset == 2 && // x:\foo
             FILENAME_IS_ABSOLUTE(aFileName)));
}

static TBool IsPerlModule(TParsePtrC aParsed)
{
    return aParsed.Ext().CompareF(_L(".pm")) == 0;
}

static TBool IsPerlScript(TParsePtrC aParsed)
{
    return aParsed.Ext().CompareF(_L(".pl")) == 0;
}

static void CopyFromInboxL(RFs aFs, const TFileName& aSrc, const TFileName& aDst)
{
    TBool proceed = ETrue;
    TMessageBuffer message;

    message.Format(_L("%S is untrusted. Install only if you trust provider."), &aDst);
    if (CPerlUi::OkCancelDialogL(message)) {
        message.Format(_L("Install as %S?"), &aDst);
        if (CPerlUi::OkCancelDialogL(message)) {
            if (BaflUtils::FileExists(aFs, aDst)) {
                message.Format(_L("Replace old %S?"), &aDst);
                if (!CPerlUi::OkCancelDialogL(message))
                    proceed = EFalse;
            }
            if (proceed) {
                // Create directory?
                TInt err = BaflUtils::CopyFile(aFs, aSrc, aDst);
                if (err == KErrNone) {
                    message.Format(_L("Installed %S"), &aDst);
                    CPerlUi::InformationNoteL(message);
                }
                else {
                    message.Format(_L("Failure %d installing %S"), err, &aDst);
                    CPerlUi::WarningNoteL(message);
                }
            }
        }
    }
}

static TBool FindPerlPackageName(TPeekBuffer aPeekBuffer, TInt aOff, TFileName& aFn)
{
    aFn.SetMax();
    TInt m = aFn.MaxLength();
    TInt n = aPeekBuffer.Length();
    TInt i = 0;
    TInt j = aOff;

    aFn.SetMax();
    // The following is a little regular expression
    // engine that matches Perl package names.
    if (j < n && isSPACE(aPeekBuffer[j])) {
        while (j < n && isSPACE(aPeekBuffer[j])) j++;
        if (j < n && isALPHA(aPeekBuffer[j])) {
            while (j < n && isALNUM(aPeekBuffer[j])) {
                while (j < n &&
                       isALNUM(aPeekBuffer[j]) &&
                       i < m)
                    aFn[i++] = aPeekBuffer[j++];
                if (j + 1 < n &&
                    aPeekBuffer[j    ] == ':' &&
                    aPeekBuffer[j + 1] == ':' &&
                    i < m) {
                    aFn[i++] = '\\';
                    j += 2;
                    if (j < n &&
                        isALPHA(aPeekBuffer[j])) {
                        while (j < n &&
                               isALNUM(aPeekBuffer[j]) &&
                               i < m)
                            aFn[i++] = aPeekBuffer[j++];
                    }
                }
            }
            while (j < n && isSPACE(aPeekBuffer[j])) j++;
            if (j < n && aPeekBuffer[j] == ';' && i + 3 < m) {
                aFn.SetLength(i);
                aFn.Append(_L(".pm"));
                return ETrue;
            }
        }
    }
    return EFalse;
}

static void GuessPerlModule(TFileName& aGuess, TPeekBuffer aPeekBuffer, TParse aDrive)
{
   TInt offset = aPeekBuffer.Find(_L8("package"));
   if (offset != KErrNotFound) {
       const TInt KPackageLen = 7;
       TFileName q;

       if (!FindPerlPackageName(aPeekBuffer, offset + KPackageLen, q))
           return;

       TFileName8 p;
       p.Copy(aDrive.Drive());
       p.Append(KModulePrefix);

       aGuess.SetMax();
       if (p.Length() + 1 + q.Length() < aGuess.MaxLength()) {
           TInt i = 0, j;

           for (j = 0; j < p.Length(); j++)
               aGuess[i++] = p[j];
           aGuess[i++] = '\\';
           for (j = 0; j < q.Length(); j++)
               aGuess[i++] = q[j];
           aGuess.SetLength(i);
       }
       else
           aGuess.SetLength(0);
   }
}

static TBool LooksLikePerlL(TPeekBuffer aPeekBuffer)
{
    return aPeekBuffer.Left(2).Compare(_L8("#!")) == 0 &&
           aPeekBuffer.Find(_L8("perl")) != KErrNotFound;
}

static TBool InstallStuffL(const TFileName &aSrc, TParse aDrive, TParse aFile, TPeekBuffer aPeekBuffer, RFs aFs)
{
    TFileName aDst;
    TPtrC drive  = aDrive.Drive();
    TPtrC namext = aFile.NameAndExt();

    aDst.Format(_L("%S%S%S"), &drive, &KScriptPrefix, &namext);
    if (!IsPerlScript(aDst) && !LooksLikePerlL(aPeekBuffer)) {
        aDst.SetLength(0);
        if (IsPerlModule(aDst))
            GuessPerlModule(aDst, aPeekBuffer, aDrive);
    }
    if (aDst.Length() > 0) {
        CopyFromInboxL(aFs, aSrc, aDst);
        return ETrue;
    }

    return EFalse;
}

static TBool RunStuffL(const TFileName& aScriptName, TPeekBuffer aPeekBuffer)
{
    TBool isModule = EFalse;

    if (IsInPerl(aScriptName) &&
        (IsPerlScript(aScriptName) ||
         (isModule = IsPerlModule(aScriptName)) ||
         LooksLikePerlL(aPeekBuffer))) {
        TMessageBuffer message;

        if (isModule)
            message.Format(_L("Really run module %S?"), &aScriptName);
        else
            message.Format(_L("Run %S?"), &aScriptName);
        if (CPerlUi::YesNoDialogL(message))
            DoRunScriptL(aScriptName);
        return ETrue;
    }

    return EFalse;
}

void CPerlAppAppUi::InstallOrRunL(const TFileName& aFileName)
{
    TParse aFile;
    TParse aDrive;
    TMessageBuffer message;

    aFile.Set(aFileName, NULL, NULL);
    if (FILENAME_IS_ABSOLUTE(aFileName)) {
        aDrive.Set(aFileName, NULL, NULL);
    } else {
        TFileName appName =
          CEikonEnv::Static()->EikAppUi()->Application()->AppFullName();
        aDrive.Set(appName, NULL, NULL);
    }
    if (!iFs)
        iFs = &CEikonEnv::Static()->FsSession();
    RFile f;
    TInt err = f.Open(*iFs, aFileName, EFileRead);
    if (err == KErrNone) {
        TPeekBuffer aPeekBuffer;
        err = f.Read(aPeekBuffer);
        f.Close();  // Release quickly.
        if (err == KErrNone) {
            if (!(IsInInbox(aFileName) ?
                  InstallStuffL(aFileName, aDrive, aFile, aPeekBuffer, *iFs) :
                  RunStuffL(aFileName, aPeekBuffer))) {
                message.Format(_L("Failed for file %S"), &aFileName);
                CPerlUi::WarningNoteL(message);
            }
        } else {
            message.Format(_L("Error %d reading %S"), err, &aFileName);
            CPerlUi::WarningNoteL(message);
        }
    } else {
        message.Format(_L("Error %d opening %S"), err, &aFileName);
        CPerlUi::WarningNoteL(message);
    }
    if (iDoorObserver)
        delete CEikonEnv::Static()->EikAppUi();
    else
        Exit();
}

#endif /* #ifndef PerlAppMinimal */

CPerlAppAppUi::~CPerlAppAppUi()
{
    if (iAppView) {
        iEikonEnv->RemoveFromStack(iAppView);
        delete iAppView;
        iAppView = NULL;
    }
    if (iFs) {
        delete iFs;
        iFs = NULL;
    }
    if (iDoorObserver) // Otherwise the embedding application waits forever.
        iDoorObserver->NotifyExit(MApaEmbeddedDocObserver::EEmpty);
}

static void DoRunScriptL(TFileName aScriptName)
{
    CPerlBase* perl = CPerlBase::NewInterpreterLC();
    TRAPD(error, perl->RunScriptL(aScriptName));
#ifndef PerlAppMinimal
    if (error != KErrNone) {
        TMessageBuffer message;
        message.Format(_L("Error %d"), error);
        CPerlUi::YesNoDialogL(message);
    }
#endif // #ifndef PerlAppMinimal
    CleanupStack::PopAndDestroy(perl);
}

#ifndef PerlAppMinimal

void CPerlAppAppUi::OpenFileL(const TDesC& aFileName)
{
    InstallOrRunL(aFileName);
    return;
}

#endif // #ifndef PerlAppMinimal

TBool CPerlAppAppUi::ProcessCommandParametersL(TApaCommand aCommand, TFileName& /* aDocumentName */, const TDesC8& /* aTail */)
{
    if (aCommand == EApaCommandRun) {
        TFileName appName = Application()->AppFullName();
        TParse p;
        p.Set(KDefaultScript, &appName, NULL);
        TEntry aEntry;
        RFs aFs;
        aFs.Connect();
        if (aFs.Entry(p.FullName(), aEntry) == KErrNone) {
            DoRunScriptL(p.FullName());
            Exit();
        }
    }
    return aCommand == EApaCommandOpen ? ETrue : EFalse;
}

#ifndef PerlAppMinimal

void CPerlAppAppUi::SetFs(const RFs& aFs)
{
    iFs = (RFs*) &aFs;
}

#endif // #ifndef PerlAppMinimal

void CPerlAppAppUi::DoHandleCommandL(TInt aCommand) {
#ifndef PerlAppMinimal
    TMessageBuffer message;
#endif // #ifndef PerlAppMinimal

    switch(aCommand)
    {
#ifndef PerlAppMinimal
    case EPerlAppCommandAbout:
        {
            message.Format(KAboutFormat,
                           PERL_REVISION,
                           PERL_VERSION,
                           PERL_SUBVERSION,
                           PERL_SYMBIANPORT_MAJOR,
                           PERL_SYMBIANPORT_MINOR,
                           PERL_SYMBIANPORT_PATCH,
                           &KFlavor,
                           PERL_SYMBIANSDK_MAJOR,
                           PERL_SYMBIANSDK_MINOR
                           );
            CPerlUi::InformationNoteL(message);
        }
        break;
    case EPerlAppCommandTime:
        {
            CPerlBase* perl = CPerlBase::NewInterpreterLC();
            const char *const argv[] =
              { "perl", "-le",
                "print 'Running in ', $^O, \"\\n\", scalar localtime" };
            perl->ParseAndRun(sizeof(argv)/sizeof(char*), (char **)argv, 0);
            CleanupStack::PopAndDestroy(perl);
        }
        break;
#ifndef __UIQ__
     case EPerlAppCommandRunFile:
        {
            TFileName aScriptUtf16;
            aScriptUtf16.Copy(_L("C:\\"));
            if (CPerlUi::FileQueryDialogL(aScriptUtf16))
              DoRunScriptL(aScriptUtf16);
        }
        break;
#endif
     case EPerlAppCommandOneLiner:
        {
#ifdef __SERIES60__
            _LIT(prompt, "Oneliner:");
#endif /* #ifdef __SERIES60__ */
#if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__)
            _LIT(prompt, "Code:"); // The title has "Oneliner" already.
#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__) */
            CPerlAppAppUi* cAppUi =
              static_cast<CPerlAppAppUi*>(CEikonEnv::Static()->EikAppUi());
            if (CPerlUi::TextQueryDialogL(_L("Oneliner"),
                                          prompt,
                                          cAppUi->iOneLiner,
                                          KPerlUiOneLinerSize)) {
                const TUint KPerlUiUtf8Multi = 3; // Expansion multiplier.
                TBuf8<KPerlUiUtf8Multi * KPerlUiOneLinerSize> utf8;

                CnvUtfConverter::ConvertFromUnicodeToUtf8(utf8,
                                                          cAppUi->iOneLiner);
                CPerlBase* perl = CPerlBase::NewInterpreterLC();
#ifdef __SERIES90__
		int argc = 5;
#else
		int argc = 3;
#endif
		char **argv = (char**) malloc(argc * sizeof(char *));
                User::LeaveIfNull(argv);

                TCleanupItem argvCleanupItem = TCleanupItem(free, argv);
                CleanupStack::PushL(argvCleanupItem);
                argv[0] = (char *) "perl";
                argv[1] = (char *) "-le";
#ifdef __SERIES90__
                argv[2] = (char *) "unshift @INC, 'C:/Mydocs';";
                argv[3] = (char *) "-e";
                argv[4] = (char *) utf8.PtrZ();
#else
                argv[2] = (char *) utf8.PtrZ();
#endif
		perl->ParseAndRun(argc, argv);
                CleanupStack::PopAndDestroy(2, perl);
            }
        }
        break;
     case EPerlAppCommandCopyright:
        {
            message.Format(KCopyrightFormat);
            CPerlUi::InformationNoteL(message);
        }
        break;
     case EPerlAppCommandAboutCopyright:
        {
            TMessageBuffer m1;
            TMessageBuffer m2;
            m1.Format(KAboutFormat,
                      PERL_REVISION,
                      PERL_VERSION,
                      PERL_SUBVERSION,
                      PERL_SYMBIANPORT_MAJOR,
                      PERL_SYMBIANPORT_MINOR,
                      PERL_SYMBIANPORT_PATCH,
                      &KFlavor,
                      PERL_SYMBIANSDK_MAJOR,
                      PERL_SYMBIANSDK_MINOR
                      );
            CPerlUi::InformationNoteL(m1);
            User::After((TTimeIntervalMicroSeconds32) (1000*1000)); // 1 sec.
            m2.Format(KCopyrightFormat);
            CPerlUi::InformationNoteL(m2);
        }
        break;
#endif // #ifndef PerlAppMinimal
    default:
        Panic(EPerlAppCommandUnknown);
    }
}

CApaDocument* CPerlAppApplication::CreateDocumentL()
{
    CPerlAppDocument* cDoc = new (ELeave) CPerlAppDocument(*this);
    return cDoc;
}

CEikAppUi* CPerlAppDocument::CreateAppUiL()
{
    CPerlAppAppUi* cAppUi = new (ELeave) CPerlAppAppUi();
    return cAppUi;
}


#ifndef PerlAppMinimal

CFileStore* CPerlAppDocument::OpenFileL(TBool aDoOpen, const TDesC& aFileName, RFs& aFs)
{
    CPerlAppAppUi* cAppUi =
      static_cast<CPerlAppAppUi*>(CEikonEnv::Static()->EikAppUi());
    cAppUi->SetFs(aFs);
    if (aDoOpen)
        cAppUi->OpenFileL(aFileName);
    return NULL;
}

#endif // #ifndef PerlAppMinimal

EXPORT_C CApaApplication* NewApplication()
{
    return new CPerlAppApplication;
}

GLDEF_C TInt E32Dll(TDllReason /*aReason*/)
{
    return KErrNone;
}

