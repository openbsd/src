// browserf.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CBrowserFilter window

class CBrowserFilter : public CComboBox
{
// Construction
public:
	CBrowserFilter();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBrowserFilter)
	public:
	virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CBrowserFilter();

	// Generated message map functions
protected:
	//{{AFX_MSG(CBrowserFilter)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
