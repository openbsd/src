#if 0
// thinking.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CThinking window

class CThinking : public CWnd
{
// Construction
public:
	CThinking();
void set_text(const char *);
void set_limit(int x);
void set_now (int x);
int val;
// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CThinking)
	public:
	virtual BOOL Create();
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CThinking();

	// Generated message map functions
protected:
	//{{AFX_MSG(CThinking)
	afx_msg void OnPaint();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
#endif
