// srcbrows.h : header file
//


#ifndef __AFXEXT_H__
#include <afxext.h>
#endif

class CSrcBrowser : public CFormView
{
public:
  static void Initialize();
  static void Terminate();

protected:
  CSrcBrowser(); 
  DECLARE_DYNCREATE(CSrcBrowser)
    void Rethink();

public:
  //{{AFX_DATA(CSrcBrowser)
  enum { IDD = ID_SYM_DIALOG_SRCBROWSER };
  CButton	m_explode;
  CBrowserList	m_listbox;
  
  CString	m_filter;
  //}}AFX_DATA
  
public:
  
  
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CSrcBrowser)
public:
  virtual void OnInitialUpdate();
protected:
  virtual void DoDataExchange(CDataExchange* pDX); 
  virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
  //}}AFX_VIRTUAL
  
  
protected:
  virtual ~CSrcBrowser();
  
  // Generated message map functions
  //{{AFX_MSG(CSrcBrowser)
  afx_msg void OnDblclkBrowseList();
  afx_msg void OnGoto();
  afx_msg void OnBreakpoint();
  afx_msg void OnSetFont();
  afx_msg void OnExplode();
  afx_msg void OnChangeFilter();
  afx_msg void OnDestroy();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };


