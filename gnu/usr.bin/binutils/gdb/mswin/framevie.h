// framevie.h : header file
//
/////////////////////////////////////////////////////////////////////////////
// CFrameData window

class CFrameData : public CListBox
{
  // Construction
public:
  CFrameData();
  
  // Attributes
public:
  void type_print(class CSymbol *, char *, class CValue *);

  // Operations
public:
  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CFrameData)
public:
  virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
  virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
  //}}AFX_VIRTUAL
  
  // Implementation
public:
  virtual ~CFrameData();
  
  // Generated message map functions
protected:
  //{{AFX_MSG(CFrameData)
  // NOTE - the ClassWizard will add and remove member functions here.
  //}}AFX_MSG
  
  DECLARE_MESSAGE_MAP()
  };

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CFrameName window

class CFrameName : public CListBox
{
  // Construction
public:
  CFrameName();
  
  // Attributes
public:
  
  // Operations
public:
  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CFrameName)
public:
  virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
  virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
  virtual BOOL OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pLResult);
  //}}AFX_VIRTUAL
  
  // Implementation
public:
  virtual ~CFrameName();
  
  // Generated message map functions
protected:
  //{{AFX_MSG(CFrameName)
  //}}AFX_MSG
  
  DECLARE_MESSAGE_MAP()
  };

/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// CFrameView view
#if 0
class CFrameView : public CView
{
 protected:
  CFrameView();			// protected constructor used by dynamic creation
  DECLARE_DYNCREATE(CFrameView)
    
    // Attributes
  public:
CListBox /*
  CFrameName*/ m_name_listbox;
  CFrameData m_data_listbox;
CListBox maxdepth;
CStatic text;
  CByteArray m_expanded;	// 1 if moving in 
  int m_init ;
  void RecalcLayout();
  // Operations
public:
  static void Initialize();
  static void Terminate();

  void rethink();

  void filldata(class CFrameInfo *pt);

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CFrameView)
	public:

	protected:
  virtual void OnDraw(CDC* pDC); // overridden to draw this view
  virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	//}}AFX_VIRTUAL
  
  // Implementation
protected:
  virtual ~CFrameView();
  // Generated message map functions
protected:
  //{{AFX_MSG(CFrameView)
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void on_sel_change_name();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnDestroy();
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };
#endif
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CFrameFrameWnd frame
#if 0
class CFrameFrameWnd : public CMDIChildWnd
{
  DECLARE_DYNCREATE(CFrameFrameWnd)
  protected:
  CFrameFrameWnd();		// protected constructor used by dynamic creation
  int m_init;
  
  // Attributes
public:
  // Operations
public:
  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CFrameFrameWnd)
protected:
  virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
  //}}AFX_VIRTUAL
  
  // Implementation
protected:
  virtual ~CFrameFrameWnd();

  // Generated message map functions
  //{{AFX_MSG(CFrameFrameWnd)
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };

/////////////////////////////////////////////////////////////////////////////
#endif


// frameview.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CFrameDialog form view


class CFrameDialog : public CFormView
{
 protected:
  CFrameDialog();		// protected constructor used by dynamic creation
  DECLARE_DYNCREATE(CFrameDialog)
    
    // Form Data
  public:
  CFont m_font;
  int m_iFontHeight;
  int m_iFontWidth;

  static void Initialize();
  static void Terminate();
  
  void rethink();
  void protected_rethink();
  int remembered ;
  CPoint databox_indent;
  
  void filldata(class CFrameInfo *pt);
  void dobuild_locals(class CBlock *b,class CFrameInfo * frame);
  //{{AFX_DATA(CFrameDialog)
  enum { IDD = IDD_FRAME_DIALOG };
  CListBox	name;
  CListBox	depth;
  CListBox	data;
  CButton gbutton;
  //}}AFX_DATA
  
  // Attributes
public:
  void DrawItemWorker(CDC*, int, int, class CSymbol *);
  
  // Operations
public:
  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CFrameDialog)
public:
  virtual void OnInitialUpdate();  void   protected_OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);  
protected:
  virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support
  virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
  //}}AFX_VIRTUAL
  
  // Implementation
protected:
  virtual ~CFrameDialog();


 
  protected:
  // Generated message map functions
  //{{AFX_MSG(CFrameDialog)
  afx_msg void OnSelchangeFrameName();
  afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);

  afx_msg void OnSelchangeFrameDepth();
  afx_msg void OnFrameGoto();
  afx_msg void OnSize(UINT nType, int cx, int cy);
  
  afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };

/////////////////////////////////////////////////////////////////////////////
