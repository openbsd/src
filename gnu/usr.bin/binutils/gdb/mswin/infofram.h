// infofram.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CGnuInfoFrame frame

class CGnuInfoHist 
{
 public:
  class  CGnuInfoNode *node;
  CGnuInfoHist *prev;
  CGnuInfoHist *next;
  CGnuInfoHist (CGnuInfoNode *v, CGnuInfoHist *prev);
};



/////////////////////////////////////////////////////////////////////////////

// infoframe.h : header file
//

/////////////////////////////////////////////////////////////////////////////

class CMySplitterWnd : public CSplitterWnd
{
	DECLARE_DYNCREATE(CMySplitterWnd)
	public:
	CMySplitterWnd();           // protected constructor used by dynamic creation

// Attributes
protected:

public:
int inited;
// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMySplitterWnd)
	public:

	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMySplitterWnd();

	// Generated message map functions
	//{{AFX_MSG(CMySplitterWnd)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

class CGnuInfoFrame : public CMDIChildWnd
{
  DECLARE_DYNCREATE(CGnuInfoFrame)
 protected:
  CGnuInfoFrame();			// protected constructor used by dynamic creation
  
  // Attributes
public:
  
  int OkNode(class CGnuInfoNode *);
  CToolBar toolbar;
  CToolBar toolbar1;
  class CGnuInfoDoc *doc();
  class CGnuInfoNode *GetCurrentNode();
  class CGnuInfoNode *current;
  class CMySplitterWnd  text_view;
  class CGnuInfoSView *scroll_view;
  class CGnuInfoList *list_view;
  class CGnuInfoHist *history_head;  
  class CGnuInfoHist *history_root;  
  class CGnuInfoHist *history_ptr;  
  int okpage(int);
  void wantpage(int);
  // Operations
public:
  void GotoNode(CString name);
  static void Initialize();
  static void Terminate();
  void SetCurrent (CGnuInfoNode *c, int remember);  
  void Prev();
  void splitdo(int x);
  void Next();
  void Up();
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CGnuInfoFrame)
public:
  virtual void RecalcLayout(BOOL bNotify = TRUE);
protected:
  virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
  //}}AFX_VIRTUAL
  
  // Implementation
protected:
  virtual ~CGnuInfoFrame();
  
  
  
  void RememberPage(CGnuInfoNode *);
  // Generated message map functions
  //{{AFX_MSG(CGnuInfoFrame)
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnActionsBackward();
  afx_msg void OnActionsForward();
  afx_msg void OnActionsNext();
  afx_msg void OnActionsPrev();
  afx_msg void OnActionsUp();
  afx_msg void OnUpdateBackward(CCmdUI* pCmdUI);
  afx_msg void OnUpdateForward(CCmdUI* pCmdUI);
  afx_msg void OnUpdateNext(CCmdUI* pCmdUI);
  afx_msg void OnUpdatePrev(CCmdUI* pCmdUI);
  afx_msg void OnUpdateUp(CCmdUI* pCmdUI);
  afx_msg void OnViewSetFont();  
  afx_msg void OnActionsViewBoth();
  afx_msg void OnActionsViewPage();
  afx_msg void OnActionsViewContents();
  afx_msg void OnActionsNextPage();
  afx_msg void OnUpdateActionsNextPage(CCmdUI* pCmdUI);
  afx_msg void OnActionsPrevPage();
  afx_msg void OnUpdateActionsPrevPage(CCmdUI* pCmdUI);

  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };

/////////////////////////////////////////////////////////////////////////////







// infosvie.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CGnuInfoSView view

class CHotSpots
{
 public:
  short int line;
  CString name;
};

class CGnuInfoSView : public CScrollView
{
 protected:
  CGnuInfoSView();
  DECLARE_DYNCREATE(CGnuInfoSView)
    
    // Attributes
  public:
int maxcx;
int maxcy;
void ParseNode (const char *,int,  char *,char *, char *,int *);
  class CGnuInfoFrame *frame;
  class CGnuInfoNode *intext_node;
  class CGnuInfoNode *GetCurrentNode() ;
  CString FindSpot (int line);
  class CGnuInfoDoc *doc() 
    { return (CGnuInfoDoc *)GetDocument();}
#define MAXHOTS 100
  int n_hotspots;
  CHotSpots hotspots[MAXHOTS];
  void AddSpot(int, const char *);
  void PrintAHotSpot(int li, CDC *p, int x, int y, const CString &s);
  
  CStringArray text;
  int last_page;
  // Operations
public:
  int  font_height;
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CGnuInfoSView)
protected:
  virtual void OnDraw(CDC* pDC); // overridden to draw this view
  virtual void OnInitialUpdate(); // first time after construct
  //}}AFX_VIRTUAL
  
  // Implementation
protected:
  virtual ~CGnuInfoSView();
  
  // Generated message map functions
  //{{AFX_MSG(CGnuInfoSView)
  afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
  
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };

/////////////////////////////////////////////////////////////////////////////

// infolist.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CGnuInfoList view

class CGnuInfoList : public CScrollView
{
 public:
  int font_height;
  class CGnuInfoFrame *frame;
void Sync();
 private:
  class CGnuInfoDoc *doc() { return (CGnuInfoDoc *)GetDocument();}
class CGnuInfoNode *GetCurrentNode();
protected:

	CGnuInfoList();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CGnuInfoList)

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGnuInfoList)
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	virtual void OnInitialUpdate();     // first time after construct
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CGnuInfoList();

	// Generated message map functions
	//{{AFX_MSG(CGnuInfoList)
		// NOTE - the ClassWizard will add and remove member functions here.
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
