// gui.h : main header file for the GUI application
//

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CGuiApp:
// See gui.cpp for the implementation of this class
//

class CGuiApp : public CWinApp
{
 private:
  virtual BOOL InitInstance();
  virtual BOOL ExitInstance();
  virtual BOOL PreTranslateMessage(MSG* pMsg);


 public:
  UINT m_uiMyListClipFormat;


  void raisekids(CMultiDocTemplate *);
  void newwin(CMultiDocTemplate*);

 public:
  static void SyncRegs();
  void sync_bpts();
  class CMultiDocTemplate* m_CmdLogTemplate;
  class CMultiDocTemplate* m_srcTemplate;
  class CMultiDocTemplate* m_expTemplate;
  class CMultiDocTemplate* m_infoTemplate;
  class CMultiDocTemplate* m_cmdTemplate;
  class CMultiDocTemplate* m_IOLogTemplate;
  class CMultiDocTemplate* m_regTemplate;
  class CMultiDocTemplate* m_srcsTemplate;
  class CMultiDocTemplate* m_bptTemplate;
  class CMultiDocTemplate* m_asmTemplate;	
  class CMultiDocTemplate* m_localTemplate;	
  class CMultiDocTemplate* m_srcbrowserTemplate;	
  class CMultiDocTemplate* m_watchTemplate;	

//  class CMultiDocTemplate *m_memTemplate;



  int gotkid(CMultiDocTemplate *p);
  CGuiApp();
  void Command(const char *command);

  void SetInterestingLine(int line);
  void SetInterestingAddr(  CORE_ADDR addr);
  void InsertBreakpoint(struct symtab *tab, int line);
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CGuiApp)
public:
  virtual BOOL OnIdle(LONG lCount);
  //}}AFX_VIRTUAL
  
  // Implementation
  
  //{{AFX_MSG(CGuiApp)

  afx_msg void OnUpdateShowAsm(CCmdUI* pCmdUI);
  afx_msg void OnShowAsm();

  afx_msg void OnUpdateShowSource(CCmdUI* pCmdUI);
  afx_msg void OnShowSource();

  afx_msg void OnUpdateShowBpts(CCmdUI* pCmdUI);
  afx_msg void OnShowBpts();

  afx_msg void OnUpdateShowLine(CCmdUI* pCmdUI);
  afx_msg void OnShowLine();

//  afx_msg void OnNewMemwin();
//  afx_msg void OnUpdateNewMemwin(CCmdUI* pCmdUI);


  afx_msg void OnAppAbout();
  afx_msg void OnRegister();
  afx_msg void OnNewCmdwin();
  afx_msg void OnUpdateNewCmdwin(CCmdUI* pCmdUI);
  afx_msg void OnNewIOLogWin();
  afx_msg void OnUpdateNewIOLogWin(CCmdUI* pCmdUI);
  afx_msg void OnUpdateBptWin(CCmdUI* pCmdUI);
  afx_msg void OnNewExpressionwin();
  afx_msg void OnNewSrcwin();
  afx_msg void OnNewRegwin();
  afx_msg void OnUpdateNewRegwin(CCmdUI* pCmdUI);
  afx_msg void OnNewBptwin();
  afx_msg void OnNewSrcBrowserWin();
  afx_msg void OnNewLocalWin();
  afx_msg void OnUpdateNewLocalWin(CCmdUI* pCmdUI);
  afx_msg void OnUpdateExpressionwin(CCmdUI* pCmdUI);
  afx_msg void OnUpdateNeedExec(CCmdUI* pCmdUI);
  
  afx_msg void OnUpdateNewSrcBrowserWin(CCmdUI* pCmdUI);
  afx_msg void OnUpdateNewSrcwin(CCmdUI* pCmdUI);
  afx_msg void OnHelpIndex();
  afx_msg void OnHelp();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };


/////////////////////////////////////////////////////////////////////////////
void theApp_sync_pc();
void theApp_show_at(CORE_ADDR pc);
void theApp_show_file(const char *);
void theApp_show_with_symtab(class CSymtab *);
void theApp_show_function(const char *, CORE_ADDR, CORE_ADDR);

class CGlobalOptions {
public:
  int always_create;
  static void Initialize();
  static void Terminate();
};

extern CGlobalOptions global_options;



class CSrcState 
{
public:
  BOOL	addresses ;
  BOOL 	breakpoint_ok;
  BOOL	disassembly;
  BOOL	instruction_data;
  BOOL	linenumbers ;
  BOOL 	source ;
public:
  CSrcState();
};
void props();

void redraw_allwins(CMultiDocTemplate *p);


void load_where(CFrameWnd *p, const char *);
void save_where(CFrameWnd *p, const char *);

void string_command (const CString &x);

extern "C" {
int mswin_query (const char*, va_list);
}
