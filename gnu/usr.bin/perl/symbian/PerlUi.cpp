/* Copyright (c) 2005 Nokia. All rights reserved. */

/* The PerlUi class is licensed under the same terms as Perl itself. */

#include "PerlUi.h"

#ifdef __SERIES60__
# include <avkon.hrh>
# include <aknnotewrappers.h> 
# include <AknCommonDialogs.h>
# ifndef __SERIES60_1X__
#  include <CAknFileSelectionDialog.h>
# endif
#endif /* #ifdef __SERIES60__ */

#if defined(__SERIES80__) || defined(__SERIES90__)
# include <eikon.hrh>
# include <cknflash.h>
# include <ckndgopn.h>
# include <ckndgfob.h>
# include <eiklabel.h>
# include <cknconf.h>
#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) */

#ifdef __UIQ__
# include <qikon.hrh>
# include <eikedwin.h>
# include <eiklabel.h>
#endif /* #ifdef __UIQ__ */

#include <apparc.h>
#include <e32base.h>
#include <e32cons.h>
#include <eikenv.h>
#include <bautils.h>
#include <eikappui.h>
#include <utf.h>
#include <f32file.h>

#include <coemain.h>

#include "PerlUi.hrh"
#include "PerlUi.rsg"

#if defined(__SERIES80__) || defined(__SERIES90__)
#include "Eikon.rsg"
#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "PerlBase.h"
#include "PerlUtil.h"

#define symbian_get_vars() Dll::Tls() // Not visible from perlXYZ.lib?

_LIT(KDefaultScript, "default.pl");

EXPORT_C void CPerlUiAppUi::ConstructL()
{
    BaseConstructL();
    iAppView = CPerlUiAppView::NewL(ClientRect());
    AddToStackL(iAppView);
    CEikonEnv::Static()->DisableExitChecks(ETrue); // Symbian FAQ-0577.
}

EXPORT_C TBool CPerlUi::OkCancelDialogL(TDesC& aMessage)
{
#ifdef __SERIES60__
    CAknNoteDialog* dlg =
        new (ELeave) CAknNoteDialog(CAknNoteDialog::EConfirmationTone);
    dlg->PrepareLC(R_PERLUI_OK_CANCEL_DIALOG);
    dlg->SetTextL(aMessage);
    return dlg->RunDlgLD() == EAknSoftkeyOk;
#endif /* #ifdef __SERIES60__ */
#if defined(__SERIES80__) || defined(__SERIES90__)
    return CCknConfirmationDialog::RunDlgWithDefaultIconLD(aMessage, R_EIK_BUTTONS_CANCEL_OK);
#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) */
#ifdef __UIQ__
    CEikDialog* dlg = new (ELeave) CEikDialog();
    return dlg->ExecuteLD(R_PERLUI_OK_CANCEL_DIALOG) == EEikBidOk;
#endif /* #ifdef __UIQ__ */
}

EXPORT_C TBool CPerlUi::YesNoDialogL(TDesC& aMessage)
{
#ifdef __SERIES60__
    CAknNoteDialog* dlg =
        new (ELeave) CAknNoteDialog(CAknNoteDialog::EConfirmationTone);
    dlg->PrepareLC(R_PERLUI_YES_NO_DIALOG);
    dlg->SetTextL(aMessage);
    return dlg->RunDlgLD() == EAknSoftkeyOk;
#endif /* #ifdef __SERIES60__ */
#if defined(__SERIES80__) || defined(__SERIES90__)
    return CCknConfirmationDialog::RunDlgWithDefaultIconLD(aMessage, R_EIK_BUTTONS_NO_YES);
#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) */
#ifdef __UIQ__
    CEikDialog* dlg = new (ELeave) CEikDialog();
    return dlg->ExecuteLD(R_PERLUI_YES_NO_DIALOG) == EEikBidOk;
#endif /* #ifdef __UIQ__ */
}

EXPORT_C void CPerlUi::InformationNoteL(TDesC& aMessage)
{
#ifdef __SERIES60__
    CAknInformationNote* note = new (ELeave) CAknInformationNote;
    note->ExecuteLD(aMessage);
#endif /* #ifdef __SERIES60__ */
#if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__)
    CEikonEnv::Static()->InfoMsg(aMessage);
#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__) */
}

EXPORT_C TInt CPerlUi::WarningNoteL(TDesC& aMessage)
{
#ifdef __SERIES60__
    CAknWarningNote* note = new (ELeave) CAknWarningNote;
    return note->ExecuteLD(aMessage);
#endif /* #ifdef __SERIES60__ */
#if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__)
    CEikonEnv::Static()->AlertWin(aMessage);
    return ETrue;
#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__) */
}

#if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__)

CPerlUiTextQueryDialog::CPerlUiTextQueryDialog(HBufC*& aBuffer) :
  iData(aBuffer)
{
}

TBool CPerlUiTextQueryDialog::OkToExitL(TInt /* aKeycode */)
{
  iData = static_cast<CEikEdwin*>(Control(EPerlUiTextQueryInputField))->GetTextInHBufL();
  return ETrue;
}

void CPerlUiTextQueryDialog::PreLayoutDynInitL()
{
  SetTitleL(iTitle);
  CEikLabel* promptLabel = ControlCaption(EPerlUiTextQueryInputField);
  promptLabel->SetTextL(iPrompt);
}

/* TODO: OfferKeyEventL() so that newline can be seen as 'OK'.
 * Or a hotkey for the button? */

#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__) */

EXPORT_C TBool CPerlUi::TextQueryDialogL(const TDesC& aTitle, const TDesC& aPrompt, TDes& aData, const TInt aMaxLength)
{
#ifdef __SERIES60__
    CAknTextQueryDialog* dlg =
        new (ELeave) CAknTextQueryDialog(aData);
    dlg->SetPromptL(aPrompt);
    dlg->SetMaxLength(aMaxLength);
    return dlg->ExecuteLD(R_PERLUI_TEXT_QUERY_DIALOG);
#endif /* #ifdef __SERIES60__ */
#if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__)
    HBufC* data = NULL;
    CPerlUiTextQueryDialog* dlg =
      new (ELeave) CPerlUiTextQueryDialog(data);
    dlg->iTitle.Set(aTitle);
    dlg->iPrompt.Set(aPrompt);
    dlg->iMaxLength = aMaxLength;
    if (dlg->ExecuteLD(R_PERLUI_ONELINER_DIALOG)) {
        aData.Copy(*data);
        return ETrue;
    }
    return EFalse;
#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__) */
}

EXPORT_C TBool CPerlUi::FileQueryDialogL(TDes& aFilename)
{
#ifdef __SERIES60__
  return AknCommonDialogs::RunSelectDlgLD(aFilename,
                                          R_PERLUI_FILE_SELECTION_DIALOG);
#endif /* #ifdef __SERIES60__ */
#if defined(__SERIES80__) || defined(__SERIES90__)
  if (CCknOpenFileDialog::RunDlgLD(aFilename,
                                    CCknFileListDialogBase::EShowAllDrives
                                   |CCknFileListDialogBase::EShowSystemFilesAndFolders
                                   |CCknFileListDialogBase::EShowBothFilesAndFolders
                                   )) {
    TEntry aEntry; // Be paranoid and check that the file is there.
    RFs aFs;
    aFs.Connect();
    if (aFs.Entry(aFilename, aEntry) == KErrNone)
      return ETrue;
    else
      CEikonEnv::Static()->InfoMsg(_L("File not found"));
  }
  return EFalse;
#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) */
#ifdef __UIQ__
  return EFalse; // No filesystem access in UIQ 2.x!
#endif /* #ifdef __UIQ__ */
}

#ifdef __SERIES60__

EXPORT_C void CPerlUiAppUi::HandleCommandL(TInt aCommand)
{
    switch(aCommand)
    {
    case EEikCmdExit:
    case EAknSoftkeyExit:
        Exit();
        break;
    default:
        DoHandleCommandL(aCommand);
        break;
    }
}

#endif /* #ifdef __SERIES60__ */

#if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__)

EXPORT_C void CPerlUiAppView::HandleCommandL(TInt aCommand, CPerlUiAppUi* aAppUi) {
    aAppUi->DoHandleCommandL(aCommand);
}

EXPORT_C void CPerlUiAppUi::HandleCommandL(TInt aCommand) {
    switch(aCommand)
    {
    case EEikCmdExit:
        Exit();
        break;
    default:
        iAppView->HandleCommandL(aCommand, this);
        break;
    }
}

#endif /* #if defined(__SERIES80__) || defined(__SERIES90__) || defined(__UIQ__) */

CPerlUiAppView* CPerlUiAppView::NewL(const TRect& aRect)
{
    CPerlUiAppView* self = CPerlUiAppView::NewLC(aRect);
    CleanupStack::Pop(self);
    return self;
}

CPerlUiAppView* CPerlUiAppView::NewLC(const TRect& aRect)
{
    CPerlUiAppView* self = new (ELeave) CPerlUiAppView;
    CleanupStack::PushL(self);
    self->ConstructL(aRect);
    return self;
}

void CPerlUiAppView::ConstructL(const TRect& aRect)
{
    CreateWindowL();
    SetRect(aRect);
    ActivateL();
}

CPerlUiAppView::~CPerlUiAppView()
{
}

void CPerlUiAppView::Draw(const TRect& /*aRect*/) const
{
    CWindowGc& gc = SystemGc();
    TRect rect = Rect();
    gc.Clear(rect);
}

