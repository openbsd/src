// bpt.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CBpt view

class CBpt : public CView
{
  int inchanged;
 protected:
  CBpt();			// protected constructor used by dynamic creation
    DECLARE_DYNCREATE(CBpt)


      // Attributes
      public:
#if 1
#define B_SETBUTTON 0
#define B_CLEAR 1
#define B_DISABLE 2
#define B_CLEARALL 3
#endif

  CButton buttons[4];
  CListBox list;
  CEdit edit;
   
  // Operations
  public:
  static void Initialize();
  static void Terminate();
   
  // Overrides
    // ClassWizard generated virtual function overrides
      //{{AFX_VIRTUAL(CBpt)
	  public:
	    virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	    virtual BOOL DestroyWindow();
	    virtual void OnInitialUpdate();
	  protected:
	    virtual void OnDraw(CDC* pDC);	// overridden to draw this view
	      virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	    //}}AFX_VIRTUAL
			   
	      // Implementation
	      protected:
  void DoUpdateList();
  void DoUpdateNewBpt();
  virtual ~CBpt();
     
  // Generated message map functions
  protected:
  void update_buttons();
  void update_list();
  //{{AFX_MSG(CBpt)
	afx_msg void on_list_changed();
	afx_msg void on_edit_changed();
	afx_msg void OnDisable();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSetBreak();

	afx_msg void OnUpdateSetBreak(CCmdUI* pCmdUI);
	afx_msg void onselchangebreakpointlist();
	afx_msg void OnDblclkBreakpointList();
	afx_msg void OnClearAll();
	afx_msg void OnClearBreakpoint();
	//}}AFX_MSG
	  DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
