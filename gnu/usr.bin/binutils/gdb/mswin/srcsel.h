// srcsel.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CSrcSel window
#ifdef __MFC4__			/* FIXME */
    #define CTabControl CTabCtrl /* FIXME: new MFC version uses CTabCtrl */
    // CTabCtrl has GetCurSel... 
    // CListCtrl has ???
    //#define CTabControl CListCtrl /* FIXME: new MFC version uses CTabCtrl */
    				/* FIXME: to include afxctl, define _AFXDLL */
    //DrawScrollers, CanScroll, AddTab, RemoveTab
    //   were part of CTabControl... now what???
    //#include <afxctl.h>	/* FIXME: don't think we need this one... */
    #include <afxdb.h>
    #include <afxcmn.h>
    typedef struct * REFIID;	// from wtypes.h
    #define TCN_TABCHANGED TCN_SELCHANGE
    #define TCN_TABCHANGING TCN_SELCHANGING
#endif

class CSrcSel : public CTabControl
{
  // Construction
public:
  static void Initialize();
  static void Terminate();
  CSrcSel();
  const char *gettext(int n);
  
  // Attributes
public:
  
  // Operations
public:
  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CSrcSel)
public:
  //}}AFX_VIRTUAL
  
  // Implementation
public:
  virtual ~CSrcSel();

#ifdef __MFC4__
    // maybe CTabControl is now CPropertySheet's GetTabControl???
    // was (CTabItem* GetTabItem())->m_strCaption...
    // PropertySheet does have a m_strCaption
    HFONT m_hBoldFont;
    HFONT m_hThinFont;
    void AddTab(const char *x) { } //FIXME: AddControlBar(CControlBar *)???
    void RemoveTab(int i) { } //FIXME: RemoveControlBar(CControlBar *)???
    void DrawScrollers(CDC* pDC) { } //FIXME??
    BOOL CanScroll() { return 0; } //FIXME??
#endif
  
  // Generated message map functions
protected:
  //{{AFX_MSG(CSrcSel)
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnPaint();
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };


/////////////////////////////////////////////////////////////////////////////
// CSrcSelWrap window

class CSrcSelWrap : public CWnd
{
public:
  CSrcSelWrap();
  CSrcSel sel;
  class CSrcSplit *parent;
  void AddTab(const char *x);
public:
  // Operations
public:
const char *gettext(int n);  
  // Overrides
  
  //{{AFX_VIRTUAL(CSrcSelWrap)
public:
  virtual BOOL Create(int, CRect &, CWnd *, int);
  
  //}}AFX_VIRTUAL
  
  // Implementation
public:
  virtual ~CSrcSelWrap();
  
  // Generated message map functions
protected:
  //{{AFX_MSG(CSrcSelWrap)
  afx_msg void OnPaint();
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);

  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CTabView view

class CTabView : public CView
{
 public:
	class CSrcSplit *parent;
	void deletecur();
	DECLARE_DYNCREATE(CTabView)
protected:
	CTabView();           // protected constructor used by dynamic creation
	virtual ~CTabView();

// Attributes
public:
  CSrcSel tabs;
// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTabView)
	public:
	virtual void OnInitialUpdate();
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view

	//}}AFX_VIRTUAL

// Implementation
protected:


	// Generated message map functions
protected:
	//{{AFX_MSG(CTabView)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnTabChanged(NMHDR*, LRESULT*); 
//	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
