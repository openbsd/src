
// gui.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "aboutbox.h"
#include "log.h"

#include "fsplit.h"
#include "mainfrm.h"
#include "regview.h"
#include "regdoc.h"
#include "expwin.h"
#include "gdbdoc.h"
#include "browserl.h"
#include "srcb.h"
#include "bpt.h"
#include "framevie.h"
#include "bptdoc.h"
#include "srcsel.h"
#include "srcd.h"
#include "srcwin.h" 
#include "option.h"
#include "ginfodoc.h"
#include "infofram.h"
#include "mem.h"

//#include "logframe.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

HICON bpt_ok_icon;
HICON bpt_here_icon;
HICON bpt_here_disabled_icon;
//HICON pc_here_icon;

static	DWORD m_dwSplashTime;
static	class CSplashWnd m_splash;

CGlobalOptions global_options;
/////////////////////////////////////////////////////////////////////////////
// CGuiApp

BEGIN_MESSAGE_MAP(CGuiApp, CWinApp)
	//{{AFX_MSG_MAP(CGuiApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_REAL_CMD_BUTTON_REGISTER, OnRegister)


	ON_COMMAND(ID_REAL_CMD_BUTTON_NEW_BPTWIN, OnNewBptwin)
	ON_COMMAND(ID_REAL_CMD_BUTTON_NEW_SRCBROWSER_WIN, OnNewSrcBrowserWin)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_NEW_SRCBROWSER_WIN, OnUpdateNewSrcBrowserWin)
	ON_COMMAND(ID_REAL_CMD_BUTTON_NEW_SRC_WIN, OnNewSrcwin)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_NEW_SRC_WIN, OnUpdateNewSrcwin)

	ON_COMMAND(ID_REAL_CMD_BUTTON_NEW_LOCAL_WIN, OnNewLocalWin)
	ON_COMMAND(ID_REAL_CMD_BUTTON_NEW_EXPRESSION_WIN, OnNewExpressionwin)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_NEW_EXPRESSION_WIN, OnUpdateExpressionwin)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_NEW_LOCAL_WIN, OnUpdateNewLocalWin)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_NEW_BPTWIN, OnUpdateBptWin)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_NEW_REGWIN, OnUpdateNewRegwin)
	ON_COMMAND(ID_REAL_CMD_BUTTON_NEW_REGWIN, OnNewRegwin)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_NEW_SRCBROWSER_WIN, OnUpdateNeedExec)

	ON_COMMAND	    (ID_REAL_CMD_BUTTON_NEW_CMDWIN, OnNewCmdwin)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_NEW_CMDWIN, OnUpdateNewCmdwin)


	ON_COMMAND	    (ID_REAL_CMD_BUTTON_NEW_IO_WIN, OnNewIOLogWin)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_NEW_IO_WIN, OnUpdateNewIOLogWin)

#if 0
	ON_COMMAND	    (ID_REAL_CMD_BUTTON_NEW_MEMORY_WIN, OnNewMemwin)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_NEW_MEMORY_WIN, OnUpdateNewMemwin)
#endif


// from the most recently selected src window
// we have them here rather than in the srcwin thing 'cause
// it's a pain when they go grey when the window becomes
// deselected
#if 1
     ON_UPDATE_COMMAND_UI (ID_REAL_CMD_BUTTON_SRCWIN_SHOWASM, OnUpdateShowAsm) 
     ON_COMMAND	 	  (ID_REAL_CMD_BUTTON_SRCWIN_SHOWASM, OnShowAsm)
     ON_UPDATE_COMMAND_UI (ID_REAL_CMD_BUTTON_SRCWIN_SHOWSOURCE, OnUpdateShowSource) 
     ON_COMMAND		  (ID_REAL_CMD_BUTTON_SRCWIN_SHOWSOURCE, OnShowSource)
     ON_UPDATE_COMMAND_UI (ID_REAL_CMD_BUTTON_SRCWIN_SHOWLINE, OnUpdateShowLine) 
     ON_COMMAND           (ID_REAL_CMD_BUTTON_SRCWIN_SHOWLINE, OnShowLine)
     ON_UPDATE_COMMAND_UI (ID_REAL_CMD_BUTTON_SRCWIN_SHOWBPT, OnUpdateShowBpts) 
     ON_COMMAND		  (ID_REAL_CMD_BUTTON_SRCWIN_SHOWBPT, OnShowBpts)

#endif

	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_HELP_INDEX, OnHelpIndex)
	ON_COMMAND(ID_HELP, OnHelp)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
	// Standard print setup command
	ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGuiApp construction
CGuiApp theApp;
//CString z;
	  void mb(char*);
CGuiApp::CGuiApp()
{
 			AfxSetResourceHandle((HINSTANCE)1);
//	 AfxGetResourceHandle();
//AfxSetResourceHandle((HINSTANCE)0x1234578);	
 //bpt_ok_icon = LoadIcon(ID_1);
}


/////////////////////////////////////////////////////////////////////////////
// The one and only CGuiApp object

// FIXME! needed to make main and getopt happy
static char* wingdb_argv[2] = {"wingdb.exe", 0};
static int wingdb_argc=1;
void main(int,char**);
/////////////////////////////////////////////////////////////////////////////
// CGuiApp initialization
CWnd *top;
CWnd *bottom;

extern "C" {
extern char *target_name;

};

void mb(char *s)
{

  MessageBox(0,"HI",s, MB_OK);
  }
BOOL CGuiApp::InitInstance()
{
  // Standard initialization
  // If you are not using these features and wish to reduce the size
  //  of your final executable, you should remove from the following
  
#if defined(TARGET_SH)
  target_name = "sh";
#elif defined(TARGET_H8300)
  target_name = "h8300";
#elif defined(TARGET_M68K)
  target_name = "m68k";
#elif defined(TARGET_SPARCLITE)
  target_name = "sparclite";
#elif defined(TARGET_SPARCLET)
  target_name = "sparclet";
#elif defined(TARGET_MIPS)
  target_name = "mips";
#elif defined(TARGET_A29K)
  target_name = "a29k";
#elif defined(TARGET_I386)
  target_name = "i386";
#elif defined(TARGET_V850)
  target_name = "V850";
#else
  HELP ME
#endif
     
   
    //  bpt_here_icon = LoadIcon(ID_SYM_FRAME_BPTHEREMARK);
    //  bpt_here_disabled_icon = LoadIcon(ID_SYM_FRAME_BPTHEREDISABLEDMARK);
    // pc_here_icon = LoadIcon(ID_ICON_PC_HERE);
    Enable3dControls();
  LoadStdProfileSettings();	
  CBrowserList::Initialize();
  //  CFlash::Initialize();
  CSrcB::Initialize();  
  CFrameDialog::Initialize();
  CIOLogView::Initialize();
   CCmdLogView::Initialize();
  CRegView::Initialize();
//  CSrcBrowser::Initialize();
  CSrcSplit::Initialize();
  CGnuInfoFrame::Initialize();
  CExpView::Initialize();
  CSrcSel::Initialize();
  CGlobalOptions::Initialize();  
  CBpt::Initialize();  


//  CMem::Initialize();



  m_IOLogTemplate 
    = new CMultiDocTemplate(
			    ID_SYM_FRAME_IOLOGTYPE,
			    RUNTIME_CLASS(CRegDoc),
			    RUNTIME_CLASS(CMDIChildWnd),
			    RUNTIME_CLASS(CIOLogView));
  AddDocTemplate(m_IOLogTemplate);
  

  m_CmdLogTemplate 
    = new CMultiDocTemplate(
			    ID_SYM_FRAME_LOGTYPE,
			    RUNTIME_CLASS(CRegDoc),
			    RUNTIME_CLASS(CMDIChildWnd),
			    RUNTIME_CLASS(CCmdLogView));
  AddDocTemplate(m_CmdLogTemplate);
  
  
  m_srcTemplate 
    = new CMultiDocTemplate(ID_SYM_FRAME_SRCTYPE,
			    RUNTIME_CLASS(CSrcD),
			    RUNTIME_CLASS(CSrcSplit),
			    RUNTIME_CLASS(CSrcScroll1));
  
  AddDocTemplate(m_srcTemplate);
  
  
  
  m_srcbrowserTemplate  
    = new CMultiDocTemplate(ID_SYM_FRAME_SRCBROWSER,
			    RUNTIME_CLASS(CGdbDoc),
			    RUNTIME_CLASS(CMiniMDIChildWnd),
			    RUNTIME_CLASS(CSrcB));
  AddDocTemplate(m_srcbrowserTemplate);

	   #if 0
  m_memTemplate  
    = new CMultiDocTemplate(ID_SYM_FRAME_MEMTYPE,
			    RUNTIME_CLASS(CRegDoc),
			    RUNTIME_CLASS(CMemFrame),
			    RUNTIME_CLASS(CMem));
  AddDocTemplate(m_memTemplate);
  #endif
  
  m_localTemplate  
    = new CMultiDocTemplate(ID_SYM_FRAME_LOCALTYPE,
			    RUNTIME_CLASS(CRegDoc),
			    RUNTIME_CLASS(CMiniMDIChildWnd),
			    RUNTIME_CLASS(CFrameDialog));
  AddDocTemplate(m_localTemplate);
  
  
  
  m_infoTemplate  
    = new CMultiDocTemplate(ID_SYM_FRAME_INFOTYPE,
			    RUNTIME_CLASS(CGnuInfoDoc),
			    RUNTIME_CLASS(CGnuInfoFrame),
			    RUNTIME_CLASS(CGnuInfoSView));
  AddDocTemplate(m_infoTemplate);
  
  
  
  m_regTemplate
    = new CMultiDocTemplate(ID_SYM_FRAME_REGTYPE,
			    RUNTIME_CLASS(CRegDoc),
			    RUNTIME_CLASS(CMiniMDIChildWnd),	// standard MDI child frame
			    RUNTIME_CLASS(CRegView));
  
  AddDocTemplate(m_regTemplate);
  
  
  
  m_expTemplate
    = new CMultiDocTemplate(ID_SYM_FRAME_EXPTYPE,
			    RUNTIME_CLASS(CExpDoc),
			    RUNTIME_CLASS(CMDIChildWnd),	
			    RUNTIME_CLASS(CExpView));
  
  AddDocTemplate(m_expTemplate);
  
  
  m_bptTemplate
    = new CMultiDocTemplate(ID_SYM_FRAME_BPTTYPE,
			    RUNTIME_CLASS(CBptDoc),
			    RUNTIME_CLASS(CMiniMDIChildWnd),	
			    RUNTIME_CLASS(CBpt));
  
  AddDocTemplate(m_bptTemplate);
  
  // Register our clipboard format names
  m_uiMyListClipFormat = ::RegisterClipboardFormat("My Object List");
  
  // create main MDI Frame window
  
  CMainFrame* pMainFrame = new CMainFrame;
  if (!pMainFrame->LoadFrame(ID_SYM_FRAME_MAINFRAME))
    return FALSE;
  m_pMainWnd = pMainFrame;
  
  /* Splash window */
  int nCmdShow = m_nCmdShow;
  //	BOOL bRunEmbedded = RunEmbedded();
  BOOL bRunEmbedded = FALSE;
  // setup main window
  nCmdShow = !bRunEmbedded ? m_nCmdShow : SW_HIDE;
  nCmdShow |= SW_SHOWMAXIMIZED;
  m_nCmdShow = SW_HIDE | SW_SHOWMAXIMIZED;
  pMainFrame->ShowWindow(nCmdShow);
  
  if (!bRunEmbedded)
    {
      m_pMainWnd->UpdateWindow();
      
      if (!m_pMainWnd->IsIconic() && m_lpCmdLine[0] == 0 &&
	  m_splash.Create(m_pMainWnd))
	{
	  m_splash.ShowWindow(SW_SHOW);
	  m_splash.UpdateWindow();
	  m_splash.SetTimer(1, 500, NULL);
	}
      m_dwSplashTime = ::GetCurrentTime();
    }
  
  
  /* end of splah window */
  
  // Create a command log window
  
  //	m_logTemplate->OpenDocumentFile(NULL);
  OnNewSrcwin();
  OnNewCmdwin();
  
  
  if (m_lpCmdLine[0] != '\0')
    {
      // TODO: add command line processing here
    }
  
  // The main window has been initialized, so show and update it.
  m_nCmdShow = nCmdShow ;
  pMainFrame->ShowWindow(m_nCmdShow);
  pMainFrame->UpdateWindow();
  
  // FIXME!! was main(0,0);
  // but that causes crash due to main expecting argc & argv!!
  main(wingdb_argc,wingdb_argv);
  
  /* Run the global options after main, cause
     we override some of the things main does */
CWinApp::Enable3dControls();
  
  COptionsSheet::Initialize();
  return TRUE;
}


// App command to run the dialog
void CGuiApp::OnAppAbout()
{
  CAboutBox aboutDlg;
  aboutDlg.DoModal();
}

BOOL 
CGuiApp::ExitInstance()
{
//CMem::Terminate();
  CSrcB::Terminate();
  CBpt::Terminate();
  COptionsSheet::Terminate();
  CBrowserList::Terminate();
  //  CFlash::Terminate();
  CFrameDialog::Terminate();
  CIOLogView::Terminate();
  CCmdLogView::Terminate();
  CRegView::Terminate();
//  CSrcBrowser::Terminate();
  CSrcSplit::Terminate();
  CGnuInfoFrame::Terminate();	
  CExpView::Terminate();
  CSrcSel::Terminate();
  CGlobalOptions::Terminate();
  return CWinApp::ExitInstance();
}
/////////////////////////////////////////////////////////////////////////////
// CGuiApp commands

void CGuiApp::OnRegister() 
{
  // TODO: Add your command handler code here
  
  
}



BOOL CGuiApp::PreTranslateMessage(MSG* pMsg)
{
  BOOL bResult = CWinApp::PreTranslateMessage(pMsg);
  
  if (m_splash.m_hWnd != NULL &&
      (pMsg->message == WM_KEYDOWN ||
       pMsg->message == WM_SYSKEYDOWN ||
       pMsg->message == WM_LBUTTONDOWN ||
       pMsg->message == WM_RBUTTONDOWN ||
       pMsg->message == WM_MBUTTONDOWN ||
       pMsg->message == WM_NCLBUTTONDOWN ||
       pMsg->message == WM_NCRBUTTONDOWN ||
       pMsg->message == WM_NCMBUTTONDOWN))
    {
      m_splash.DestroyWindow();
      m_pMainWnd->UpdateWindow();
    }
  
  return bResult;
}


BOOL CGuiApp::OnIdle(LONG lCount)
{
  // call base class idle first
  BOOL bResult = CWinApp::OnIdle(lCount);
  
  // then do our work
  if (m_splash.m_hWnd != NULL)
    {
      if (::GetCurrentTime() - m_dwSplashTime > 2500)
	{
	  // timeout expired, destroy the splash window
	  m_splash.DestroyWindow();
	  m_pMainWnd->UpdateWindow();
	  
	  // NOTE: don't set bResult to FALSE,
	  //  CWinApp::OnIdle may have returned TRUE
	}
      else
	{
	  // check again later...
	  bResult = TRUE;
	}
    }
  
  if (iowinptr)
    iowinptr->doidle();
  if (cmdwinptr)
    cmdwinptr->doidle();
  return bResult;
}

void CGuiApp:: raisekids(CMultiDocTemplate *doc_temp)
{
  POSITION pos_doc_temp = doc_temp->GetFirstDocPosition();
  while (pos_doc_temp) {
    CDocument *doc = doc_temp->GetNextDoc(pos_doc_temp);
    POSITION p =  doc->GetFirstViewPosition();
    while (p)
      {
	CView *view = doc->GetNextView (p);
	//      view->GetParentFrame()->SetParent(crap);
	CFrameWnd *f = (CFrameWnd *)(view->GetParentFrame());
	f->ActivateFrame(SW_SHOWNORMAL);
      } 
  }
}

void CGuiApp::newwin(CMultiDocTemplate *p)
{
  POSITION pdoc = p->GetFirstDocPosition();
  if (pdoc) {
    CDocument *doc = p->GetNextDoc(pdoc);
    POSITION pos =  doc->GetFirstViewPosition();
    if (pos) {
      raisekids(p);
      return ;
    }
  }
  p->OpenDocumentFile(NULL);
  raisekids(p);
}

#if 0
void CGuiApp::OnNewMemwin() { // newwin (m_memTemplate);
}
void CGuiApp::OnUpdateNewMemwin(CCmdUI* pCmdUI) 
{
  //  pCmdUI->SetCheck(gotkid(m_memTemplate));
  // pCmdUI->Enable(togdb_target_has_execution());	
}
#endif


void CGuiApp::OnNewLocalWin() {newwin(m_localTemplate);}
void CGuiApp::OnNewCmdwin() { newwin (m_CmdLogTemplate);}
void CGuiApp::OnNewIOLogWin() { newwin (m_IOLogTemplate);}
void CGuiApp::OnNewRegwin() { newwin (m_regTemplate);}
void CGuiApp::OnNewBptwin() { newwin (m_bptTemplate);}
void CGuiApp::OnNewSrcBrowserWin() {  newwin(m_srcbrowserTemplate);}

void CGuiApp::OnUpdateNewSrcBrowserWin(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck(gotkid(m_srcbrowserTemplate));
  pCmdUI->Enable(togdb_target_has_execution());	
}

void CGuiApp::OnUpdateNewRegwin(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck(gotkid(m_regTemplate));
  pCmdUI->Enable(togdb_target_has_execution());	
}
void CGuiApp::OnUpdateBptWin(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck(gotkid(m_bptTemplate));
  pCmdUI->Enable(togdb_target_has_execution());	
}

void CGuiApp::OnUpdateNewCmdwin(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck(gotkid(m_CmdLogTemplate));
}


void CGuiApp::OnUpdateNewIOLogWin(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck(gotkid(m_IOLogTemplate));
}

void CGuiApp::OnNewSrcwin() {  newwin(m_srcTemplate);} 
void CGuiApp::OnUpdateNewSrcwin(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck(gotkid(m_srcTemplate));
  pCmdUI->Enable(togdb_target_has_execution());	
}


int CGuiApp::gotkid(CMultiDocTemplate *p)
{
  POSITION pdoc = p->GetFirstDocPosition();
  if (pdoc) {
    CDocument *doc = p->GetNextDoc(pdoc);
    POSITION p =  doc->GetFirstViewPosition();
    return p !=0;
  }
  return 0;
}

CSrcD *m_srcdoc;

void CGuiApp::OnNewExpressionwin()
{
  
  CExpView::open();
}

/* Execute a gui command from a script file 
   FIXME:
   syntax today is
   
   gui bw
   gui sw "filename"
   gui lw
   
   until I've got a better clue about how to do this
   (parts have to be in c++ too)
   */

void go() {}
extern CMainFrame *the_cover;
extern CWnd *top;
void CGuiApp::Command(const char *command)
{
#define BPT_WINDOW 0
#define SRC_WINDOW 1
#define REG_WINDOW 2
#define LOG_WINDOW 3
#define SB_WINDOW 4
#define LOCAL_WINDOW 5
#define FL_WINDOW 6
#define SPLIT_WINDOW 7
#define GO 9
  
  static struct cstruct 
    {
      const char *command;
      int todo;
    } c[] = {{"bw", BPT_WINDOW},
	     {"sw",SRC_WINDOW},
	     {"sp",SPLIT_WINDOW},
	     {"rw",REG_WINDOW},
	     {"lw", LOG_WINDOW},
	     {"lo", LOCAL_WINDOW},
	     {"go", GO},
	     {"fl", FL_WINDOW},
	     {0,0}};
  
  
  struct cstruct const *p;
  
  for (p = c; p->command; p++) 
    {
      int clen = strlen(p->command);
      if (strncmp(command, p->command, clen) == 0)
	{
	  const  char *arg1 = command + clen + 1;
	  if (arg1[0]==0)
	    arg1 = 0;
	  switch (p->todo)
	    {
	    case GO:
	      go();
	      break;
	      
	    case BPT_WINDOW:
	      m_bptTemplate->OpenDocumentFile(NULL);
	      break;
	    case LOCAL_WINDOW:
	      m_localTemplate->OpenDocumentFile(NULL);
	      break;
	    case REG_WINDOW:
	      m_regTemplate->OpenDocumentFile(NULL);
	      break;
	    case LOG_WINDOW:
	      m_CmdLogTemplate->OpenDocumentFile(NULL);
	      break;
	    case SRC_WINDOW:
	      if (!m_srcdoc) 
		m_srcdoc =(CSrcD *)( m_srcTemplate->OpenDocumentFile(arg1));
	      
	      break;
	    case SB_WINDOW:
	      m_srcbrowserTemplate->OpenDocumentFile(NULL);
	      break;
	    case FL_WINDOW:
	      theApp_show_file(arg1);
	      //	      CWatch::makeone(m_pMainWnd);
	      break;
	    }
	}
    }
}



void CGuiApp::InsertBreakpoint(struct symtab *tab, int line)
{
  togdb_set_breakpoint_sal (tab, line);
}


void CGuiApp::OnUpdateNewLocalWin(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck(gotkid(m_localTemplate));
  pCmdUI->Enable(togdb_target_has_execution());	
}


void CGuiApp::OnUpdateExpressionwin(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck(gotkid(m_expTemplate));
  pCmdUI->Enable(togdb_target_has_execution());	
  // pCmdUI->Enable(!CExpView::is_open());
}


void CGuiApp::OnUpdateNeedExec(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(togdb_target_has_execution());	
}



CWinApp *GetTheApp()
{
  return &theApp;
}

#if 1
void CGuiApp::OnHelpIndex() 
{
  
  CString ibuf = getenv("INFOPATH");
  ibuf += "\\dir.inf";
  m_infoTemplate->OpenDocumentFile(ibuf);      
}

void CGuiApp::OnHelp() 
{
  CString ibuf = getenv("INFOPATH");
  ibuf += "\\gdb.inf";
  m_infoTemplate->OpenDocumentFile(ibuf);      
}
#endif


void CGuiApp::sync_bpts()
{
  class CMultiDocTemplate *p = m_srcTemplate;
  POSITION pos = p->GetFirstDocPosition();
  while (pos)
    {
      CSrcD *doc = (CSrcD *)(p->GetNextDoc(pos));
      doc->sync_bpts();
    }
}

/* this is what the call stack looks like when we get
   here.
   theApp_sync_pc();
   gdbwin_update();
   update()
   run()
   catch_errors()
   togdb_command_from_tty();
   OnEditChange();


   Our pc has changed, call all the documents and tell
   them to:  (note that usually there is only one src document)

   doc->sync_pc();
   which will do an
   UpdateAllViews (NULL, pc)
   which will do a
   CtabView::OnUpdate (,..pc..);
   and then a
   CSrcScroll1::OnUpdate(,..pc..);

   for the CTabView:
   since pc looks valid, it will call it's parent, which
   is the split

   CSrcSplit::new_pc (pc)

   which will fetch the pc, look up the containing symbab and
   then call 

   CSrc_split:select_symbtab(thecurrentsymtab)

   if the srcfile we need to show is not the current file,
   (we can tell since we ask doc->lookup_symtab to return the
   CSrcFile which has the symtab we just fetched) we 
   set the CSrcSplit::visible to be the srcfile associated
   with the symtab, and then call each pane of the CSrcSplit
   to workout it's source. This set's the visible buffer
   to point to the right area of memory, which should contain
   the source of the file we want to show.

   that calls SetCurSel so the correct tab is shown.


*/
   

extern CSrcScroll1 *thesrcpane;
void theApp_sync_pc()
{
  class CMultiDocTemplate *p = theApp.m_srcTemplate;
  POSITION pos = p->GetFirstDocPosition();
  while (pos)
    {
      CSrcD *doc = (CSrcD *)(p->GetNextDoc(pos));
      doc->sync_pc();
    }


  /* And tell the locals window too. */
  {
    class CMultiDocTemplate *p = theApp.m_localTemplate;
    POSITION pos = p->GetFirstDocPosition();
    while (pos)
      {
	CSrcD *doc = (CSrcD *)(p->GetNextDoc(pos));
	doc->UpdateAllViews(0);
      }
  }
}

void theApp_show_at (CORE_ADDR pc)
{
  class CMultiDocTemplate *p = theApp.m_srcTemplate;
  POSITION pos = p->GetFirstDocPosition();
  while (pos)
    {
      CSrcD *doc = (CSrcD *)(p->GetNextDoc(pos));
      doc->show_at(pc);
    }
}



void theApp_sync_watch()
{
  class CMultiDocTemplate *p = theApp.m_expTemplate;
  POSITION pos = p->GetFirstDocPosition();
  while (pos)
    {
      CExpDoc *doc = (CExpDoc *)(p->GetNextDoc(pos));
      doc->UpdateAllViews(0);
    }
}

void theApp_add_watch(const char *name)
{
  class CMultiDocTemplate *p = theApp.m_expTemplate;
  POSITION pos = p->GetFirstDocPosition();
  if (!pos)
    {
      theApp.m_expTemplate->OpenDocumentFile(NULL);
      pos = p->GetFirstDocPosition();
    }
  while (pos)
    {
      CExpDoc *doc = (CExpDoc *)(p->GetNextDoc(pos));
      doc->add(name);
      doc->UpdateAllViews(0);
    }
}

CSrcD *getdoc()
{
  class CMultiDocTemplate *p = theApp.m_srcTemplate;
  POSITION pos = p->GetFirstDocPosition();
  if (!pos)
    {
      theApp.m_srcTemplate->OpenDocumentFile("");
    }
  return  (CSrcD *)(p->GetNextDoc(pos));
}

void theApp_show_file(const char *path)
{
  CSrcD *doc = getdoc();
  doc->read_src_by_filename(path);
  doc->UpdateAllViews(0);
}

void theApp_show_with_symtab(CSymtab *st)
{
  CSrcD *doc = getdoc();
  doc->read_src_by_symtab(st);
  doc->UpdateAllViews(0);		   
}

void theApp_show_function(const char *path, 
			  CORE_ADDR low,
			  CORE_ADDR high)
{
  CSrcD *doc = getdoc();
  doc->func(path, low, high);
  doc->UpdateAllViews(0);
}


void CGlobalOptions::Terminate()
{
  theApp.WriteProfileInt("Global","always_create", global_options.always_create);
}
void CGlobalOptions::Initialize()
{
  global_options.always_create =theApp.GetProfileInt("Global","always_create",1);
}



void redraw_allwins(CMultiDocTemplate *p)
{
  
  POSITION pos = p->GetFirstDocPosition();
  
  while (pos)
    {
      CSrcD *doc = (CSrcD *)(p->GetNextDoc(pos));
      POSITION p = doc->GetFirstViewPosition();
      while (p)
	{
	  CView *v = doc->GetNextView(p);
	  /* Read and reset the font, so the window resizes if necessary */
	  CFont *font = v->GetFont();
	  v->SetFont(font, TRUE);
	}
    }
}


void CGuiApp::SyncRegs()
{
  POSITION pos = theApp.m_regTemplate->GetFirstDocPosition();
  
  while (pos)
    {
      CRegDoc *doc = (CRegDoc *)(theApp.m_regTemplate->GetNextDoc(pos));
      doc->Sync();
#if 0
      POSITION p = doc->GetFirstViewPosition();
      while (p)
	{
	  CView *v = doc->GetNextView(p);
	  v->GetParentFrame()->Invalidate();
	}
#endif
    }
}	




void CGuiApp:: OnShowAsm()
{
  if (thesrcpane)
    thesrcpane->OnShowAsm();
}

void CGuiApp:: OnUpdateShowAsm(CCmdUI* pCmdUI) 
{
  if (thesrcpane)
    thesrcpane->OnUpdateShowAsm(pCmdUI);
}


void CGuiApp:: OnShowSource()
{
  if (thesrcpane)
    thesrcpane->OnShowSource();
}

void CGuiApp:: OnUpdateShowSource(CCmdUI* pCmdUI) 
{
  if (thesrcpane)
    thesrcpane->OnUpdateShowSource(pCmdUI);
}

void CGuiApp:: OnShowBpts()
{
  if (thesrcpane)
    thesrcpane->OnShowBpts();
}

void CGuiApp:: OnUpdateShowBpts(CCmdUI* pCmdUI) 
{
  if (thesrcpane)
    thesrcpane->OnUpdateShowBpts(pCmdUI);
}


void CGuiApp:: OnShowLine()
{
  if (thesrcpane)
    thesrcpane->OnShowLine();
}

void CGuiApp:: OnUpdateShowLine(CCmdUI* pCmdUI) 
{
  if (thesrcpane)
    thesrcpane->OnUpdateShowLine(pCmdUI);
}



static TCHAR BASED_CODE szWindowPos[] = _T("WindowPos"); 
static TCHAR szFormat[] = _T("%u,%u,%d,%d,%d,%d,%d,%d,%d,%d"); 

void  load_where (CFrameWnd *frame, const char *name)
{
  CString strBuffer = AfxGetApp()->GetProfileString(name, szWindowPos); 
  if (strBuffer.IsEmpty()) 
    return ;
  
  WINDOWPLACEMENT wp; 
  int nRead = _stscanf(strBuffer, szFormat, 
		       &wp.flags, &wp.showCmd, 
		       &wp.ptMinPosition.x, &wp.ptMinPosition.y, 
		       &wp.ptMaxPosition.x, &wp.ptMaxPosition.y, 
		       &wp.rcNormalPosition.left, &wp.rcNormalPosition.top, 
		       &wp.rcNormalPosition.right, &wp.rcNormalPosition.bottom); 
  
  if (nRead != 10) 
    return;
  
  wp.length = sizeof wp; 
  frame->SetWindowPlacement(&wp);
  
}

void save_where(CFrameWnd *frame, const char *name)
     // write a window placement to settings section of app's ini file 
{
  WINDOWPLACEMENT wp; 
  wp.length = sizeof wp;
  frame->GetWindowPlacement(&wp);
  TCHAR szBuffer[sizeof("-32767")*8 + sizeof("65535")*2]; 
  
  wsprintf(szBuffer, szFormat, 
	   wp.flags, wp.showCmd, 
	   wp.ptMinPosition.x, wp.ptMinPosition.y, 
	   wp.ptMaxPosition.x, wp.ptMaxPosition.y, 
	   wp.rcNormalPosition.left, wp.rcNormalPosition.top, 
	   wp.rcNormalPosition.right, wp.rcNormalPosition.bottom); 
  AfxGetApp()->WriteProfileString(name, szWindowPos, szBuffer); 
} 


extern "C" {
  
  
  int win32pollquit(void)
    {
      MSG msg;
      if (PeekMessage(&msg, NULL, 0,0,PM_REMOVE)) {
	if (msg.message == WM_KEYDOWN) {
	  return 1;
	}
      }
      return 0;
    }					   
}

extern "C" 
{
  int mswin_query (const char *name, va_list args)
    {
      char b[200];
      vsprintf (b, name, args);
      return MessageBox (0, b, "GDB says", MB_ICONQUESTION|MB_YESNO)==IDYES;
    }

  unsigned long __cdecl
  _beginthreadex (void *a, unsigned b, unsigned (__stdcall *c)(void *), void *d, unsigned e, unsigned *f)
    {
      return 0;
    }

  void __cdecl
  _endthreadex (unsigned xxx)
    {
    }
}
