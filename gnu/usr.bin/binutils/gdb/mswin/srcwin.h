// source.h : header file
//


/////////////////////////////////////////////////////////////////////////////

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif

class CSrcSplit : public CMDIChildWnd
{
  DECLARE_DYNCREATE(CSrcSplit)
 protected:
  CSrcSplit();			// protected constructor used by dynamic creation
  
public:
#define ASMISH 0
#define SRCISH 1
#define ON 0
#define OFF 1
#define TOGGLE 2
  void set_type(int type, int what, int which);
  
  
  class CSrcScroll1 *panes[2];
  
  // Attributes
protected:
public:
  class CTabView *sel;
  CStatic fname;
//  class CSrcInfoView *info;
  CSplitterWnd split;
  int active;
  
public:
  
  class CSrcFile *visible_file;	// pointer to file involved with the window
  static void Initialize();
  static void Terminate();
  // Operations
public:
  void select_title(const char *);
  void select_symtab(class CSymtab *);
  void select_function (const char *, CORE_ADDR low, CORE_ADDR high);
  void new_pc(CORE_ADDR pc);
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CSrcSplit)
protected:
  virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
  virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
  //}}AFX_VIRTUAL
  
  // Implementation
public:
  virtual ~CSrcSplit();
  
  void hit( int *);
  // Generated message map functions
  //{{AFX_MSG(CSrcSplit)
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnFileOpen();
  afx_msg void OnTabClose();
	afx_msg void OnDestroy();
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CSrcScroll view




/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
// CSrcScroll1 view




class CSrcScroll1 : public CScrollView
{
public:
  int init;
  int winindex;
  int show_assembly() ;
  int show_source() ;
  void toggle(int *p);
  int srcb;  /* 1 to show src */
  int asmb;  /* 1 to show asm, 2 when showing asm because src not available */ 
  int lineb;
  int bptb;
  class  CSrcD * getdoc() { return (CSrcD*)(GetDocument());}
  void workout_source();
private:
  class CSrcFile *get_visible_file() 
    { return parent->visible_file; }
  class CSrcFrom *visible_buffer;
  
  void show_file(const char *n);
  void set_invert(int );
  int get_scroll_y();
  void scroll_to_show_line (int i);

  
  int width;
  int splatx;
  int srcx;
  enum { BPT, SRC }  zone; 
  int srcline_index;
  int depth;
  int get_top_line();
  int line_with_pc;
  int line_with_show_line;
  int line_from_0basedy(int y);
  int addr_in_line (int line, CORE_ADDR pc, int sp);
  int find_line_of_pc (CORE_ADDR);
  void calc_visible_lines(int*first_visible_line,
			  int *last_visible_line);
  void splat(CDC *pDC, int red, int y)  ;
  void redraw_line(int y, int off);
  
  
  
protected:
  CSrcScroll1();		// protected constructor used by dynamic creation
  DECLARE_DYNCREATE(CSrcScroll1)
    
    // Attributes
  public:
  class CSrcSplit *parent;
  // Operations
public:
  int want_to_show_line ;  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CSrcScroll1)
  virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
protected:
  virtual void OnDraw(CDC* pDC); // overridden to draw this view
  virtual void OnInitialUpdate(); // first time after construct

  //}}AFX_VIRTUAL
  
  // Implementation
protected:
  virtual ~CSrcScroll1();
public:  
  // Generated message map functions
  //{{AFX_MSG(CSrcScroll1)
  afx_msg void OnS();
  afx_msg void OnN();
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
  afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
  afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
  afx_msg void OnMouseMove(UINT nFlags, CPoint point);
  afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
  afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
  afx_msg void OnUpdateShowAsm(CCmdUI* pCmdUI);
  afx_msg void OnUpdateShowSource(CCmdUI* pCmdUI);
  afx_msg void OnUpdateShowLine(CCmdUI* pCmdUI);
  afx_msg void OnUpdateShowBpts(CCmdUI* pCmdUI);
  afx_msg void OnShowAsm();
  afx_msg void OnShowSource();
  afx_msg void OnWatch();
  afx_msg void OnShowLine();
  afx_msg void OnShowBpts();
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };

/////////////////////////////////////////////////////////////////////////////



void      redraw_allsrcwins();
/////////////////////////////////////////////////////////////////////////////
