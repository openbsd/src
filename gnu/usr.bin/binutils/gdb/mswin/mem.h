#if 0
// mem.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CMem view

class CMem : public CView
{
protected:
	CMem();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CMem)

// Attributes
public:
CEdit address;
CListBox types;
//CCheckBox update;
CButton now;
// Operations
public:

static void Initialize();
static void Terminate();
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMem)
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CMem();

	// Generated message map functions
protected:
	//{{AFX_MSG(CMem)
	afx_msg void OnChangeEdit();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CMemMini frame

class CMemMini : public CMiniFrameWnd
{
public:
  DECLARE_DYNCREATE(CMemMini)

  CMemMini();			// protected constructor used by dynamic creation
  
  // Attributes
public:
  
  // Operations
public:
  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CMemMini)
  //}}AFX_VIRTUAL
  
  // Implementation

  virtual ~CMemMini();
  
  // Generated message map functions
  //{{AFX_MSG(CMemMini)
  // NOTE - the ClassWizard will add and remove member functions here.
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CMemFrame frame

class CMemFrame : public CMiniMDIChildWnd
{
  DECLARE_DYNCREATE(CMemFrame)
  protected:
  CMemFrame();			// protected constructor used by dynamic creation
  
// CMemMini mini;
  CDialogBar bar;

  // Attributes
public:
  
  // Operations
public:
  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CMemFrame)
public:
  virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
protected:
  virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
  virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
  //}}AFX_VIRTUAL
  
  // Implementation
protected:
  virtual ~CMemFrame();
  
  // Generated message map functions
  //{{AFX_MSG(CMemFrame)
  // NOTE - the ClassWizard will add and remove member functions here.
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };


#endif
