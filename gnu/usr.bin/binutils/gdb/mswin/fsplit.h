#if 0
class CFSplit : public CMDIChildWnd
{
 public:
	CFSplit();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CFSplit)
protected:


// Attributes


public:
	CSplitterWnd    m_wndSplitter;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFSplit)
	protected:
	virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CFSplit();

	// Generated message map functions
	//{{AFX_MSG(CFSplit)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////



#endif
