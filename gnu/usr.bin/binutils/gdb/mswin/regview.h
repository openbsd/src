// regview.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CRegView view

class CRegView : public CScrollView
{

void PrintReg ( class CRegDoc *pDoc, class CDC *pDC,int rn_, int base_, int, int);
protected:
	CRegView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CRegView)
	int m_base;
	int mx;
	int my;
	CSize m_regsize;
	CSize m_valuesize;
	int m_ronline;
	int m_numlines;
	int format(char *, CORE_ADDR, int);
void rethink();
void BestShape();

// Attributes
public:
// Operations
public:
	static void Initialize();
	static void Terminate();




// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRegView)
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CRegView();

	// Generated message map functions
protected:
	//{{AFX_MSG(CRegView)
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnRegBinary();
	afx_msg void OnRegDecimal();
	afx_msg void OnRegEverything();
	afx_msg void OnRegHex();
	afx_msg void OnRegOctal();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFont();
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

