// change.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CChange dialog

class CChange : public CDialog
{
// Construction

public:
void think();
static void change(const char *name);
	CChange(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CChange)
	enum { IDD = IDD_CHANGE_VALUE };
	CEdit	name;
	CEdit	value;
	//}}AFX_DATA
constchar *sname;


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChange)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CChange)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnWatch();
	afx_msg void OnSet();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
