/////////////////////////////////////////////////////////////////////////////

class CExpView : public CView
{public:
static void open();
static int is_open();

 protected:			// create from serialization only
  CExpView();
  DECLARE_DYNCREATE(CExpView)
    
    // Attributes
  public:
  class CExpDoc* GetDocument();
  
  // Operations
public:
  static void Initialize();
  static void Terminate();
  // Implementation
public:
  virtual ~CExpView();
  virtual void OnDraw(CDC* pDC); // overridden to draw this view
  virtual void OnUpdate(CView* pView, LPARAM lHint, CObject* pHint);
  virtual void OnInitialUpdate();
  
  
private:
  CListBox m_wndList;
  CEdit edit;
  CFont m_font;
  CButton update;
  CButton del;
  class CMyObj *sel;
  int m_iFontHeight;
  int m_iFontWidth;
  //    CBitmap m_bmSmile;
  
  // Generated message map functions
protected:
  //{{AFX_MSG(CExpView)
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDestroy();
  afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
  afx_msg void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct);
  afx_msg void OnSize(UINT nType, int cx, int cy);
  afx_msg void OnSetFocus(CWnd* pOldWnd);
  afx_msg void OnListBoxDblClick();
  afx_msg void OnUpdateEditCopy(CCmdUI* pCmdUI);
  afx_msg void OnEditCopy();
  afx_msg void OnUpdateEditPaste(CCmdUI* pCmdUI);
  afx_msg void OnEditPaste();
  afx_msg void OnUpdateEditDel(CCmdUI* pCmdUI);
  afx_msg void OnEditAdd();
  afx_msg void OnEditDelItem();
  afx_msg void OnEditDel();
  afx_msg void OnEditMaxtext();
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };


inline CExpDoc* CExpView::GetDocument()
{ return (CExpDoc*) m_pDocument; }


/////////////////////////////////////////////////////////////////////////////


// myobj.h : interface of the CMyObject class
//
/////////////////////////////////////////////////////////////////////////////

class CMyObj : public CObject
{
public:
  DECLARE_SERIAL(CMyObj)
    CMyObj();
  ~CMyObj();
  virtual void Serialize(CArchive& ar); 
  
  const CString& GetText()
    {return m_strText;}
  void SetText(CString& str)
    {m_strText = str;}
  int DoEditDialog();
  
private:
  CString m_strText;
};

/////////////////////////////////////////////////////////////////////////////

// myoblist.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CMyObList object

class CMyObList : public CObList
{
  DECLARE_SERIAL(CMyObList)
  public:
  CMyObList(); 
  ~CMyObList();
  void DeleteAll();
  CMyObj* RemoveHead()
    {return (CMyObj*) CObList::RemoveHead();}
  CMyObj* GetNext(POSITION& rPos)
    {return (CMyObj*) CObList::GetNext(rPos);}
  void Append(CMyObj* pMyObj);
  BOOL Remove(CMyObj* pMyObj);                       
  virtual void Serialize(CArchive& ar); 
};


// ExpDoc.h : interface of the CExpDoc class
//
/////////////////////////////////////////////////////////////////////////////

class CExpDoc : public CDocument
{
protected:			// create from serialization only
  CExpDoc();
  DECLARE_DYNCREATE(CExpDoc)
    
    // Attributes
  public:
  
  // Operations
public:
  void add(const char *name);
  CMyObList* GetObList()
    {return &m_MyObList;}
  // Implementation
public:
  virtual ~CExpDoc();
  virtual void Serialize(CArchive& ar);	// overridden for document i/o
protected:
  virtual BOOL    OnNewDocument();
  
private:
  CMyObList m_MyObList;
  
  // Generated message map functions
protected:
  //{{AFX_MSG(CExpDoc)
  // NOTE - the ClassWizard will add and remove member functions here.
  //    DO NOT EDIT what you see in these blocks of generated code !
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };

/////////////////////////////////////////////////////////////////////////////

