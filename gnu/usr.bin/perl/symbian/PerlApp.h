/* Copyright (c) 2004-2005 Nokia. All rights reserved. */

/* The PerlApp application is licensed under the same terms as Perl itself. */

#ifndef __PerlApp_h__
#define __PerlApp_h__

#include "PerlUi.h"

/* The source code can be compiled into "PerlApp" which is the simple
 * launchpad application/demonstrator, or into "PerlAppMinimal", which
 * is the minimal Perl launchpad application.  Define the cpp symbols
 * CreatePerlAppMinimal (a boolean), PerlAppMinimalUid (the Symbian
 * application uid in the 0x... format), and PerlAppMinimalName (a C
 * wide string, with the L prefix) to compile as "PerlAppMinimal". */

// #define CreatePerlAppMinimal

#ifdef CreatePerlAppMinimal
# define PerlAppMinimal
# ifndef PerlAppMinimalUid // PerlApp is ...F6, PerlRecog is ...F7
#  define PerlAppMinimalUid 0x102015F8
# endif
# ifndef PerlAppMinimalName
#  define PerlAppMinimalName L"PerlAppMinimal"
# endif
#endif

#ifdef PerlAppMinimal
# ifndef PerlAppMinimalUid
#   error PerlAppMinimal defined but PerlAppMinimalUid undefined
# endif
# ifndef PerlAppMinimalName
#  error PerlAppMinimal defined but PerlAppMinimalName undefined
# endif
#endif

class CPerlAppDocument : public CgPerlUiDocument
{
  public:
    CPerlAppDocument(CEikApplication& aApp) : CgPerlUiDocument(aApp) {;}
#ifndef PerlAppMinimal
    CFileStore* OpenFileL(TBool aDoOpen, const TDesC& aFilename, RFs& aFs);
#endif // #ifndef PerlAppMinimal
  private: // from CEikDocument
    CEikAppUi* CreateAppUiL();
};

class CPerlAppApplication : public CPerlUiApplication
{
  private:
    CApaDocument* CreateDocumentL();
    TUid AppDllUid() const;
};

class CPerlAppAppView;

class CPerlAppAppUi : public CPerlUiAppUi
{
  public:
    TBool ProcessCommandParametersL(TApaCommand aCommand, TFileName& aDocumentName, const TDesC8& aTail);
    void DoHandleCommandL(TInt aCommand);
#ifndef PerlAppMinimal
    void OpenFileL(const TDesC& aFileName);
    void InstallOrRunL(const TFileName& aFileName);
    void SetFs(const RFs& aFs);
#endif // #ifndef PerlAppMinimal
    ~CPerlAppAppUi();
  private:
    RFs* iFs;
};

class CPerlAppAppView : public CPerlUiAppView
{
  public:
#if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__)
    void HandleCommandL(TInt aCommand);
#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__) */
};

#endif // __PerlApp_h__
