// clipvw.cpp : implementation of the CExpView class
//

#include "stdafx.h"
#include "expwin.h"
#include "afxpriv.h" // for CSharedFile
#include "transbmp.h" // for CTransBmp

#define MAX(a,b) (((a) > (b)) ? (a) : (b))


#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

static CTransBmp on;
static CTransBmp off;
extern CGuiApp theApp;
static CExpView *openview;

/////////////////////////////////////////////////////////////////////////////
// CExpView

IMPLEMENT_DYNCREATE(CExpView, CView)

BEGIN_MESSAGE_MAP(CExpView, CView)
    //{{AFX_MSG_MAP(CExpView)
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_DRAWITEM()
    ON_WM_MEASUREITEM()
    ON_WM_SIZE()
    ON_WM_SETFOCUS()
    ON_CONTROL(LBN_DBLCLK, ID_CMD_BUTTON_MYLIST, OnListBoxDblClick)
    ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
    ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
    ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
    ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
    ON_UPDATE_COMMAND_UI(ID_EDIT_CLEAR, OnUpdateEditDel)
    ON_COMMAND(ID_EDIT_CLEAR, OnEditDel)
    ON_COMMAND(ID_REAL_CMD_BUTTON_EDIT_ADD, OnEditAdd)
    ON_COMMAND(ID_REAL_CMD_BUTTON_EDIT_DEL, OnEditDelItem)
    ON_EN_MAXTEXT(ID_CMD_BUTTON_EDIT, OnEditMaxtext)
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CExpView construction/destruction

void CExpView::Initialize()
{
   on.LoadBitmap(ID_SYM_BITMAP_ON);
   off.LoadBitmap(ID_SYM_BITMAP_OFF);

}
void CExpView::Terminate()
{
}

CExpView::CExpView()
{
    // Load the font we want to use
    m_font.CreateStockObject(ANSI_FIXED_FONT);
    // Get the metrics of the font
    CDC dc;
    dc.CreateCompatibleDC(NULL);
    CFont* pfntOld = (CFont*) dc.SelectObject(&m_font);
    TEXTMETRIC tm;
    dc.GetTextMetrics(&tm);
    dc.SelectObject(pfntOld);
    m_iFontHeight = tm.tmHeight;
    m_iFontWidth = tm.tmMaxCharWidth;
    // Load the bitmap we want
sel = 0;
openview = this;
}

CExpView::~CExpView()
{
openview = 0;
}

/////////////////////////////////////////////////////////////////////////////
// CExpView drawing

void CExpView::OnDraw(CDC* pDC)
{
    // Nothing to do here - all done by the listbox
}



/////////////////////////////////////////////////////////////////////////////
// CExpView message handlers

int CExpView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
  lpCreateStruct->style |= WS_EX_TOPMOST;
  if (CView::OnCreate(lpCreateStruct) == -1)
    return -1;
    
  CRect rc;
  GetClientRect(&rc);
  // adjust the client are to make the list box look better
  rc.bottom -= 2;
  
  edit.Create(WS_CHILD|WS_VISIBLE|WS_BORDER,	rc, this, ID_CMD_BUTTON_EDIT);
  m_wndList.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL
		   |WS_BORDER
		   | LBS_DISABLENOSCROLL | LBS_OWNERDRAWFIXED
		   | LBS_EXTENDEDSEL | LBS_NOINTEGRALHEIGHT
		   | LBS_NOTIFY,
		   rc,
		   this,
		   ID_CMD_BUTTON_MYLIST);
  
  update.Create("&set", 
		WS_CHILD|WS_VISIBLE|WS_BORDER, 
		rc,
		this,
		ID_REAL_CMD_BUTTON_EDIT_ADD);

  
  del.Create("&del", 
		WS_CHILD|WS_VISIBLE|WS_BORDER, 
		rc,
		this,
		ID_REAL_CMD_BUTTON_EDIT_DEL);
  
  GetParentFrame()->SetWindowPos(&wndTopMost, 0,0, 300,300,
				 SWP_NOMOVE);
  
  GetParentFrame()->SetWindowPos(0, 0,0, 300,300,  SWP_NOMOVE);
  GetParentFrame()->SetWindowText("Watch");
  return 0;
}

void CExpView::OnInitialUpdate()
{
  load_where(GetParentFrame(),"EXPWIN");	
}

void CExpView::OnUpdate(CView* pView, LPARAM lHint, CObject* pHint)
{
  // Get a pointer to the list of objects in the doc
  CExpDoc* pDoc = GetDocument();
  CMyObList* pObList = pDoc->GetObList();
  ASSERT(pObList);
  // Reset the listbox
  m_wndList.ResetContent();
  // Fill the listbox from the object list
  POSITION pos = pObList->GetHeadPosition();
  while (pos) {
    CMyObj* pObj =  pObList->GetNext(pos);
    m_wndList.AddString((char*) pObj);
  }
}

void CExpView::OnDestroy()
{
  save_where(GetParentFrame(),"EXPWIN");	
  CView::OnDestroy();
  // Be sure to destroy the window we created
  m_wndList.DestroyWindow();
}
void showsymbol(CDC *p,int x, int y, CSymbol *symbol);     
void CExpView::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT pDI)
{
  CMyObj* pObj;
  HFONT hfntOld;
  CRect rcText;
  CDC *pDC=CDC::FromHandle(pDI->hDC);
  
  switch (pDI->itemAction) {
    
  case ODA_DRAWENTIRE:
    // Draw the whole line of information
    // Get a pointer to the object
    pObj = (CMyObj*) pDI->itemData;
    ASSERT(pObj);
    ASSERT(pObj->IsKindOf(RUNTIME_CLASS(CMyObj)));
    // Set up the font we want to use
    hfntOld = (HFONT) ::SelectObject(pDI->hDC, m_font.m_hObject);
    rcText = pDI->rcItem;
    // Erase the entire area
    ::ExtTextOut(pDI->hDC, 
		 rcText.left, rcText.top,
		 ETO_OPAQUE,
		 &rcText,
		 "", 0,
		 NULL);
#if 0
    // Draw the bitmap in place
    { 
      static int k;
      k++;
      if (k&1)
	on.Draw(pDI->hDC, rcText.left, rcText.top);
      else
	off.Draw(pDI->hDC, rcText.left, rcText.top);
    }
#endif
    
    
#if 0    
    sym = CSymbol::Lookup(pObj->GetText());
    if (sym)
      {
	showsymbol (pDC, rcText.left, rcText.top, sym);
      }
    else
#endif
      {
	CString a = pObj->GetText();
	CString b = togdb_eval_as_string ((const char *)pObj->GetText());
	CString c = a + "=" + b;
	pDC->TextOut(rcText.left, rcText.top, c);
      }
#if 0
    // Move the text over to just beyond the bitmap
    rcText.left = pDI->rcItem.left + on.GetWidth() + 2;
    ::DrawText(pDI->hDC,
	       pObj->GetText(),
	       -1,
	       &rcText,
	       DT_LEFT | DT_VCENTER);
#endif
    
    // Check if we need to show selection state
    if (pDI->itemState & ODS_SELECTED) {
      ::InvertRect(pDI->hDC, &(pDI->rcItem));
    }
    // Check if we need to show focus state
    if (pDI->itemState & ODS_FOCUS) {
      ::DrawFocusRect(pDI->hDC, &(pDI->rcItem));
    }
    ::SelectObject(pDI->hDC, hfntOld);
    break;
    
  case ODA_FOCUS:
    // Toggle the focus state
    ::DrawFocusRect(pDI->hDC, &(pDI->rcItem));
    break;
    
  case ODA_SELECT:
    // Toggle the selection state
    ::InvertRect(pDI->hDC, &(pDI->rcItem));
    break;
  default:
    break;
  }
}

void CExpView::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
  // Return the height of the font or the bitmap, 
  // whichever is greater
  lpMeasureItemStruct->itemHeight = MAX(m_iFontHeight, on.GetHeight());
}

void CExpView::OnSize(UINT nType, int cx, int cy)
{
  CView::OnSize(nType, cx, cy);
  // Resize the listbox the fit the top of the client area
  // put the edit box at the bottom 

  /* x1 text--x2 b1 x3- b2-x4*/

  int border = 10;
  int x1 = border;
  int x4 = cx - border;
  int x2 = x4 - ( m_iFontHeight * 8);
  int x3 = x4 - ( m_iFontHeight * 4);
  int endeditboxy = cy - border;
  int topeditboxy = endeditboxy - m_iFontHeight - 15;
  int endlistboxy = topeditboxy - border;
  int toplistboxy = border;
  
  
  m_wndList.SetWindowPos(NULL, x1, toplistboxy,
			 x4-x1 , endlistboxy - toplistboxy,
			 SWP_NOACTIVATE | SWP_NOZORDER);
  
  edit.SetWindowPos(NULL, x1, topeditboxy,
		    (x2 - x1) - border, endeditboxy - topeditboxy, 
		    SWP_NOACTIVATE |SWP_NOZORDER);
  
  update.SetWindowPos(NULL, x2,topeditboxy,
		      (x3 - x2) , endeditboxy - topeditboxy,
		      SWP_NOACTIVATE |SWP_NOZORDER);
  
  del.SetWindowPos(NULL, x3,topeditboxy,
		   (x4 - x3) , endeditboxy - topeditboxy,
		   SWP_NOACTIVATE |SWP_NOZORDER);
}

void CExpView::OnSetFocus(CWnd* pOldWnd)
{
  // Set focus to the listbox
  m_wndList.SetFocus();
}

void CExpView::OnListBoxDblClick()
{
  // Find what was clicked
  int i = m_wndList.GetSelCount();
  // Get the first selected item
  int iSel = LB_ERR;
  m_wndList.GetSelItems(1, &iSel);
  ASSERT(iSel != LB_ERR);
  CMyObj* pObj = (CMyObj*) m_wndList.GetItemData(iSel);
#if 0
  ASSERT(pObj);
  ASSERT(pObj->IsKindOf(RUNTIME_CLASS(CMyObj)));
  if (pObj->DoEditDialog() == IDOK) {
    CExpDoc* pDoc = GetDocument();
    pDoc->SetModifiedFlag();
    pDoc->UpdateAllViews(NULL);
  }
#endif
  sel = pObj;
  edit.SetActiveWindow();
  edit.EnableWindow();
  edit.SetFocus();
  edit.SetWindowText(pObj->GetText());
}

void CExpView::OnUpdateEditCopy(CCmdUI* pCmdUI)
{
  int i = m_wndList.GetSelCount();
  pCmdUI->Enable(i > 0 ? TRUE : FALSE);
}

void CExpView::OnEditCopy()
{
  // Get the number of selected items
  int iCount = m_wndList.GetSelCount();
  ASSERT(iCount > 0);
  // get the list of selection ids
  int* pItems = new int [iCount];
  m_wndList.GetSelItems(iCount, pItems);
  // Create a list
  CMyObList ObList;
  // Add all the items to the list
  int i;
  CMyObj* pObj;
  for (i=0; i<iCount; i++) {
    pObj = (CMyObj*) m_wndList.GetItemData(pItems[i]);
    ObList.Append(pObj);
  }
  // Done with the item list
  delete pItems;
  // Create a memory file based archive
  CSharedFile mf (GMEM_MOVEABLE|GMEM_DDESHARE|GMEM_ZEROINIT);
  CArchive ar(&mf, CArchive::store);  
  ObList.Serialize(ar);
  ar.Close();			// flush and close
  HGLOBAL hMem = mf.Detach();
  if (!hMem) return;
  // Nuke the list but not the objects
  ObList.RemoveAll();
  // Send the clipboard the data
  OpenClipboard();
  EmptyClipboard();
  SetClipboardData(theApp.m_uiMyListClipFormat, hMem);
  CloseClipboard();
}

void CExpView::OnUpdateEditPaste(CCmdUI* pCmdUI)
{
  // See if there is a list available
  OpenClipboard();
  UINT uiFmt = 0;
  while (uiFmt = EnumClipboardFormats(uiFmt)) {
    if (uiFmt == theApp.m_uiMyListClipFormat) {
      CloseClipboard();
      pCmdUI->Enable(TRUE);
      return;
    }
  }
  pCmdUI->Enable(FALSE);
  CloseClipboard();    
}

void CExpView::OnEditPaste()
{
  OpenClipboard();
  HGLOBAL hMem = ::GetClipboardData(theApp.m_uiMyListClipFormat);
  if (!hMem) {
    CloseClipboard();
    return;
  }
  // Create a mem file
  CSharedFile mf;
  mf.SetHandle(hMem);
  // Create the archive and get the data
  CArchive ar(&mf, CArchive::load);  
  CMyObList PasteList;
  PasteList.Serialize(ar);
  ar.Close();
  mf.Detach();
  CloseClipboard();
  
  // Add all the objects to the doc
  CExpDoc* pDoc = GetDocument();
  ASSERT(pDoc);
  CMyObList* pObList = pDoc->GetObList();
  ASSERT(pObList);
  ASSERT(pObList->IsKindOf(RUNTIME_CLASS(CMyObList)));
  POSITION pos = NULL;
  // Remove each of the CMyObj objects from the paste list
  // and append them to the list
  while (! PasteList.IsEmpty()) {
    CMyObj* pObj =  PasteList.RemoveHead();
    ASSERT(pObj);
    ASSERT(pObj->IsKindOf(RUNTIME_CLASS(CMyObj)));
    pObList->Append(pObj);
  }
  pDoc->UpdateAllViews(NULL);
}


void CExpView::OnUpdateEditDel(CCmdUI* pCmdUI)
{
  int i = m_wndList.GetSelCount();
  pCmdUI->Enable(i > 0 ? TRUE : FALSE);
}

void CExpView::OnEditDel()
{
  // Delete any currently selected items
  int iCount = m_wndList.GetSelCount();
  if((iCount != LB_ERR) && (iCount > 0)) {
    CExpDoc* pDoc = GetDocument();
    ASSERT(pDoc);
    CMyObList* pObList = pDoc->GetObList();
    ASSERT(pObList);
    ASSERT(pObList->IsKindOf(RUNTIME_CLASS(CMyObList)));
    // get the list of selection ids
    int* pItems = new int [iCount];
    m_wndList.GetSelItems(iCount, pItems);
    // Delete all the items
    for (int i=0; i<iCount; i++) {
      CMyObj* pObj 
	= (CMyObj*) m_wndList.GetItemData(pItems[i]);
      pObList->Remove(pObj);
      delete pObj;
    }
    delete pItems;
    pDoc->UpdateAllViews(NULL);
  }
}

void CExpView::OnEditMaxtext()
{
  OnEditAdd();
}


void CExpView::OnEditDelItem()
{
  OnEditDel();

}
void CExpView::OnEditAdd()
{
  CExpDoc* pDoc = GetDocument();
  CString foo;
  edit.GetWindowText(foo);
  if (foo.GetLength() > 0) 
    {
      if (sel)
	{
	  sel->SetText(foo);
	  sel = 0;
	}
      else {
	CMyObj* pOb = new CMyObj;
	pOb->SetText(foo);
	
	
	ASSERT(pDoc);
	CMyObList* pList = pDoc->GetObList();
	ASSERT(pList);
	
	
	pList->Append(pOb);
      }

      pDoc->UpdateAllViews(NULL);
      edit.SetWindowText("");
    }
}


/////////////////////////////////////////////////////////////////////////////
// CMyObj

IMPLEMENT_SERIAL(CMyObj, CObject, 0)
     
     /////////////////////////////////////////////////////////////////////////////
     // CMyObj construction/destruction
     
     CMyObj::CMyObj()
{
  m_strText = "Some text.";
}

CMyObj::~CMyObj()
{
}

void CMyObj::Serialize(CArchive& ar)
{
  if (ar.IsStoring()) {
    ar << m_strText;
  } else {
    ar >> m_strText;
  }
}


/////////////////////////////////////////////////////////////////////////////
// CMyObList

IMPLEMENT_SERIAL(CMyObList, CObList, 0)
     
     CMyObList::CMyObList()
{
}

CMyObList::~CMyObList()
{
  DeleteAll();
}

/////////////////////////////////////////////////////////////////////////////
// CMyObjMode serialization

void CMyObList::Serialize(CArchive& ar)
{
  CObList::Serialize(ar);
}

/////////////////////////////////////////////////////////////////////////////
// CMyObList commands

// Delete al objects in the list
void CMyObList::DeleteAll()
{
  while(!IsEmpty()) {
    CMyObj* ptr = RemoveHead();
    ASSERT(ptr);
    delete ptr;
  }
}

// Add a new object to the end of the list
void CMyObList::Append(CMyObj* pMyObj)
{
  ASSERT(pMyObj);
  ASSERT(pMyObj->IsKindOf(RUNTIME_CLASS(CMyObj)));
  CObList::AddTail(pMyObj);
}

// Remove an object from the list but don't delete it
BOOL CMyObList::Remove(CMyObj* pMyObj)
{
  POSITION pos = Find(pMyObj);
  if (!pos) return FALSE;
  RemoveAt(pos);
  return TRUE;
}






/////////////////////////////////////////////////////////////////////////////
// CExpDoc

IMPLEMENT_DYNCREATE(CExpDoc, CDocument)
     
     BEGIN_MESSAGE_MAP(CExpDoc, CDocument)
     //{{AFX_MSG_MAP(CExpDoc)
     // NOTE - the ClassWizard will add and remove mapping macros here.
     //    DO NOT EDIT what you see in these blocks of generated code !
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     /////////////////////////////////////////////////////////////////////////////
     // CExpDoc construction/destruction
     
     CExpDoc::CExpDoc()
{
  // TODO: add one-time construction code here
}

CExpDoc::~CExpDoc()
{
  m_MyObList.DeleteAll();
}

BOOL CExpDoc::OnNewDocument()
{
  if (!CDocument::OnNewDocument())
    return FALSE;
  m_MyObList.DeleteAll();
  return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CExpDoc serialization

void CExpDoc::Serialize(CArchive& ar)
{
  m_MyObList.Serialize(ar);
}


/////////////////////////////////////////////////////////////////////////////
// CExpDoc commands





/////////////////////////////////////////////////////////////////////////////
// CMyObjDlg message handlers


BOOL CExpView::OnEraseBkgnd(CDC* pDC) 
{
  // TODO: Add your message handler code here and/or call default
  
  CBrush b (RGB(192,192,192));
  CBrush *old = pDC->SelectObject(&b);
  CRect rect;
  pDC->GetClipBox(&rect);
  pDC->PatBlt(rect.left, rect.top,rect.Width(), rect.Height(), PATCOPY);
  pDC->SelectObject(old);
  return TRUE;
}


void CExpDoc::add(const char *name)
{
  CMyObj *p = new CMyObj();
  CString x = strdup(name);
  p->SetText(x);
  m_MyObList.Append (p);
  UpdateAllViews(0);
}


void
CExpView::open()
{
  extern CGuiApp theApp;
  if (!openview)
    theApp.m_expTemplate->OpenDocumentFile(NULL);
  else
    openview->ShowWindow(SW_RESTORE);
}

int
CExpView::is_open()
{
  return openview != 0;
}

