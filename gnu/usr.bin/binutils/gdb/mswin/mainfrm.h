// mainfrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

class CMyToolBar : public CToolBar
{
// Construction
public:
	CMyToolBar();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMyToolBar)
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMyToolBar();

	// Generated message map functions
protected:
	//{{AFX_MSG(CMyToolBar)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class CMainFrame : public CMDIFrameWnd
{
  DECLARE_DYNAMIC(CMainFrame)
 public:
  CMainFrame();

  // Attributes
public:
  
  // Operations
public:
  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CMainFrame)
public:
  virtual void RecalcLayout(BOOL bNotify = TRUE);
 protected:
  virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
  //}}AFX_VIRTUAL

  // Implementation
public:

  //class CFSplit *split;
  virtual ~CMainFrame();
  void DockControlBarLeftOf(CControlBar* Bar,CControlBar* LeftOf)  ;
  void InitBar(CToolBar *p, int id, const unsigned int *buttons, int size);
  
protected:			// control bar embedded members
  CStatusBar  m_wndStatusBar;
  CMyToolBar    m_wndToolBar;
  CMyToolBar  m_wndGdbBar;
  CMyToolBar  m_wndWinBar;
  CMyToolBar src_win_bar;
//  CDialogBar doingbar;
  // Generated message map functions
protected:
  //{{AFX_MSG(CMainFrame)
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnRegister();
  afx_msg void OnSync();
  afx_msg void OnClose();

  afx_msg void OnTargetRemote();
  afx_msg void OnTargetSimulator();
  afx_msg void OnReadScript();

  afx_msg void OnS();
  afx_msg void OnFinish();
  afx_msg void OnUpdateS(CCmdUI* pCmdUI);
  afx_msg void OnUpdateSync(CCmdUI* pCmdUI);
  afx_msg void OnUpdateWhat(CCmdUI* pCmdUI);
  afx_msg void OnUpdateFinish(CCmdUI* pCmdUI);
  afx_msg void OnN();	afx_msg void OnUpdateN(CCmdUI* pCmdUI);
  afx_msg void OnCont();
  afx_msg void OnUpdateCont(CCmdUI* pCmdUI);
  afx_msg void OnUpdateRun(CCmdUI* pCmdUI);
  afx_msg void OnRun();
  afx_msg void OnSize(UINT nType, int cx, int cy);

  afx_msg void OnIn();	afx_msg void OnUpdateIn(CCmdUI* pCmdUI);
  afx_msg void OnOut();	afx_msg void OnUpdateOut(CCmdUI* pCmdUI);
  afx_msg void OnProperties();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CMyToolbar window


/////////////////////////////////////////////////////////////////////////////
