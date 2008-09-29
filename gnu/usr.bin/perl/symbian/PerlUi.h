/* Copyright (c) 2005 Nokia. All rights reserved. */

/* The PerlUi class is licensed under the same terms as Perl itself. */

#ifndef __PerlUi_h__
#define __PerlUi_h__

#ifdef __SERIES60__
# include <aknapp.h>
# include <aknappui.h>
# include <akndoc.h>
#endif /* #ifdef __SERIES60__ */

#if defined(__SERIES80__) || defined(__SERIES90__)
# include <eikapp.h>
# include <eikappui.h>
# include <eikdoc.h>
# include <eikbctrl.h>
# include <eikgted.h>
# include <eikdialg.h>
#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) */

#ifdef __UIQ__
# include <qikapplication.h>
# include <qikappui.h>
# include <qikdocument.h>
# include <eikdialg.h>
#endif /* #ifdef __UIQ____ */

#include <coecntrl.h>
#include <f32file.h>

#ifdef __SERIES60__
# define CgPerlUiDocument    CAknDocument
# define CgPerlUiApplication CAknApplication
# define CgPerlUiAppUi       CAknAppUi
# define CgPerlUiNoteDialog  CAknNoteDialog
# define CgPerlUiAppView     CCoeControl
#endif /* #ifdef __SERIES60__ */

#if defined(__SERIES80__) || defined(__SERIES90__)
# define CgPerlUiDocument    CEikDocument
# define CgPerlUiApplication CEikApplication
# define CgPerlUiAppUi       CEikAppUi
# define CgPerlUiNoteDialog  CCknFlashingDialog
# define CgPerlUiAppView     CEikBorderedControl
#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) */

#ifdef __UIQ__
# define CgPerlUiDocument    CEikDocument
# define CgPerlUiApplication CQikApplication
# define CgPerlUiAppUi       CQikAppUi
# define CgPerlUiNoteDialog  CCknFlashingDialog
# define CgPerlUiAppView     CCoeControl
#endif /* #ifdef __UIQ__ */

class CPerlUiApplication : public CgPerlUiApplication
{
};

const TUint KPerlUiPromptSize   = 20;
const TUint KPerlUiOneLinerSize = 128;

class CPerlUiAppView;

class CPerlUiAppUi : public CgPerlUiAppUi
{
  public:
    IMPORT_C void ConstructL();
    void virtual DoHandleCommandL(TInt aCommand) = 0;
    IMPORT_C void HandleCommandL(TInt aCommand);
    TBuf<KPerlUiOneLinerSize> iOneLiner; // Perl code to evaluate.
    CPerlUiAppView* iAppView;
};

class CPerlUiAppView : public CgPerlUiAppView
{
  public:
    static CPerlUiAppView* NewL(const TRect& aRect);
    static CPerlUiAppView* NewLC(const TRect& aRect);
    ~CPerlUiAppView();
    void Draw(const TRect& aRect) const;
#if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__)
    IMPORT_C void HandleCommandL(TInt aCommand, CPerlUiAppUi* aAppUi);
#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__) */
  private:
    void ConstructL(const TRect& aRect);
};

#if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__)

class CPerlUiTextQueryDialog : public CEikDialog
{
  public:
    CPerlUiTextQueryDialog(HBufC*& aBuffer);
    /* TODO: OfferKeyEventL() so that newline can be seen as 'OK'. */
    HBufC*& iData;
    TPtrC iTitle;  // used in S80 but not in S60
    TPtrC iPrompt; // used in S60 and S80
    TInt iMaxLength;
  protected:
    void PreLayoutDynInitL();
  private:
    TBool OkToExitL(TInt aKeycode);
};

#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__) */

class CPerlUi : public CgPerlUiAppUi
{
  public:
    IMPORT_C static TBool OkCancelDialogL(TDesC& aMessage);
    IMPORT_C static TBool YesNoDialogL(TDesC& aMessage);
    IMPORT_C static void  InformationNoteL(TDesC& aMessage);
    IMPORT_C static TInt  WarningNoteL(TDesC& aMessage);
    IMPORT_C static TBool TextQueryDialogL(const TDesC& aTitle, const TDesC& aPrompt, TDes& aData, const TInt aMaxLength);
    IMPORT_C static TBool FileQueryDialogL(TDes& aFilename);
};

#endif // __PerlUi_h__
