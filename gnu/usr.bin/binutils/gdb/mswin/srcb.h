// srcb.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// SrcB view

class CSrcB : public CView
{
 int showall;
 protected:
 CSrcB();			// protected constructor used by dynamic creation
 DECLARE_DYNCREATE(CSrcB)
   void rethink();
 // Attributes
 public:
 static void Initialize();
 static void Terminate();
 CEdit edit;
 CButton view;		    
 CButton bpt;
 CButton explode;

 CBrowserList list;
 
 // Operations
 public:

 // Overrides
 // ClassWizard generated virtual function overrides
 //{{AFX_VIRTUAL(CSrcB)
 public:
 virtual void OnInitialUpdate();
 protected:
 virtual void OnDraw(CDC* pDC);	// overridden to draw this view
 virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
 //}}AFX_VIRTUAL
 
 // Implementation
 protected:
 virtual ~CSrcB();
 
 // Generated message map functions
 protected:
 //{{AFX_MSG(CSrcB)
 afx_msg void OnSize(UINT nType, int cx, int cy);
 afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
 afx_msg BOOL OnEraseBkgnd(CDC* pDC);
 afx_msg void on_change_filter();
 afx_msg void on_explode();
 afx_msg void on_goto();
 afx_msg void on_breakpoint();
afx_msg void on_dblclkbrowselist();

 //}}AFX_MSG
 DECLARE_MESSAGE_MAP()
 };

/////////////////////////////////////////////////////////////////////////////

