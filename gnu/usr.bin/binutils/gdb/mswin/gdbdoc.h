// gdbdoc.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CGdbDoc document

class CGdbDoc : public CDocument
{
protected:
	CGdbDoc();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CGdbDoc)

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGdbDoc)
	protected:
	virtual BOOL OnNewDocument();
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CGdbDoc();
	virtual void Serialize(CArchive& ar);   // overridden for document i/o

	// Generated message map functions
protected:
	//{{AFX_MSG(CGdbDoc)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
