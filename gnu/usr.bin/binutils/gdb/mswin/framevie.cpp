// framevie.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"

#include "framevie.h"

CFontInfo frame_fontinfo  ("FrameWindow");
	    
/////////////////////////////////////////////////////////////////////////////
// CFrameDialog

IMPLEMENT_DYNCREATE(CFrameDialog, CFormView)

CFrameDialog::CFrameDialog()
	: CFormView(CFrameDialog::IDD)
{
  //{{AFX_DATA_INIT(CFrameDialog)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  remembered  = 0;
  
  
  m_font.CreateStockObject(ANSI_FIXED_FONT);
  CDC dc;
  dc.CreateCompatibleDC(NULL);
  CFont* pfntOld = (CFont*) dc.SelectObject(&m_font);
  TEXTMETRIC tm;
  dc.GetTextMetrics(&tm);
  dc.SelectObject(pfntOld);
  m_iFontHeight = tm.tmHeight;
  m_iFontWidth = tm.tmMaxCharWidth;
}

CFrameDialog::~CFrameDialog()
{
}

void CFrameDialog::DoDataExchange(CDataExchange* pDX)
{
  CFormView::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CFrameDialog)
  DDX_Control(pDX, IDC_FRAME_NAME, name);
  DDX_Control(pDX, IDC_FRAME_DEPTH, depth);
  DDX_Control(pDX, IDC_FRAME_DATA, data);
  DDX_Control(pDX, IDC_FRAME_GOTO, gbutton);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CFrameDialog, CFormView)
     //{{AFX_MSG_MAP(CFrameDialog)
     ON_LBN_SELCHANGE(IDC_FRAME_NAME, OnSelchangeFrameName)
     ON_WM_DRAWITEM()
     ON_LBN_SELCHANGE(IDC_FRAME_DEPTH, OnSelchangeFrameDepth)
     ON_BN_CLICKED(IDC_FRAME_GOTO, OnFrameGoto)
     ON_CONTROL(LBN_DBLCLK, IDC_FRAME_NAME, OnFrameGoto)
     ON_WM_SIZE()
     ON_WM_SHOWWINDOW()
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     
     
     
     /////////////////////////////////////////////////////////////////////////////
     // CFrameDialog message handlers
     
     void CFrameDialog::OnInitialUpdate() 
{
  CFormView::OnInitialUpdate();
  load_where(GetParentFrame(),"FRAME");
  depth.AddString("1");
  depth.AddString("2");
  depth.AddString("3");
  depth.AddString("4");
  depth.AddString("5");
  depth.SetCurSel(3);
  
  ResizeParentToFit();
  rethink();
}

void CFrameDialog::filldata(CFrameInfo *frame)
{
  data.ResetContent();
  if (frame) {
  CBlock *block = frame->GetFrameBlock();
  
  while (block != 0)
    {
      dobuild_locals (block, frame);
      
      /* After handling the function's top-level block, stop.
	 Don't continue to its superblock, the block of
	 per-file symbols.  */
      if (block->GetFunction())
	break;
      block = block->GetSuperBlock();
    }	
	}
}

void CFrameDialog::dobuild_locals(CBlock *b,class CFrameInfo * frame)
{
  int nsyms;
  register int i;
  CSymbol *sym;
  
  nsyms = b->GetNSyms();
  
  for (i = 0; i < nsyms; i++)
    {
      sym = b->GetBlockSym (i);
      switch (sym->GetSymbolClass())
	{
	case LOC_LOCAL:
	case LOC_REGISTER:
	case LOC_STATIC:
	case LOC_BASEREG:
	  
	case LOC_REGPARM:
	case LOC_ARG:
	  
	  data.AddString((char *)sym);
	  break;
	  
	default:
	  /* Ignore symbols which are not locals.  */
	  break;
	}
    }
}

char *tostring(int i,CFrameInfo *p)
{
  char bu[100];
  if (p) 
    {
      char *filename;
      int line;
      
      struct symtab *sy;
      struct frame_annotated_info fai;
      togdb_annotate_info (p , &fai);
      togdb_fetchloc (p->GetFramePC(), &filename, &line, &sy);
      sprintf(bu,"%2d %s %s:%d %s",
	      i,
	      preg(p->GetFramePC()), filename,line,
	      fai.funcname);
      return strdup(bu);
    }
  else 
    {
      return strdup("**undef**");
    }
}




void CFrameDialog::protected_rethink()
{
  name.ResetContent();
  CFrameInfo *fi;
  int de  ;
  int maxd =depth.GetCurSel()+1;
  for (de  = 0, fi = CFrameInfo::GetCurrentFrame();
       fi && de  < maxd;
       fi = fi->get_prev_frame())
    {	
      name.AddString( tostring(de, fi));
      de++;
    }
  
  filldata(CFrameInfo::GetCurrentFrame());
  UpdateData(1);
}

static CFrameDialog *theframe;
static int caller (char *)
{
  theframe->protected_rethink();
  return 1;
}

static int p1;
static LPDRAWITEMSTRUCT p2;
static int call_protected_OnDrawItem(char *)
{
  theframe->protected_OnDrawItem(p1,p2);
  return 1;
}
void CFrameDialog::rethink()
{
  theframe = this;
  catch_errors (caller, 0, "Error!", 2);
}
void CFrameDialog::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
  // TODO: Add your specialized code here and/or call the base class
  
  rethink();
}

void CFrameDialog::OnSelchangeFrameName() 
{
  int level = name.GetCurSel();
  if (level >= 0)
    {
      CFrameInfo *p;	 	
      p  = CFrameInfo::GetCurrentFrame();
      while (level >0)
	{
	  p = p->get_prev_frame();
	  level --;
	}
      filldata(p);
    }
  
}

void type_print(CSymbol *symbol, char *p, CValue *value)
{
  CType *type = symbol->GetType();
  if (type->code == TYPE_CODE_PTR
      || type->code == TYPE_CODE_REF)
    {
      
    }
  switch (type->code) {
  case TYPE_CODE_STRUCT:
    sprintf(p,"struct");
    break;
  case TYPE_CODE_INT:
    sprintf(p, "%d", value->GetInt());
    break;
  case TYPE_CODE_ENUM:
    sprintf(p, "%s (%d)", value->GetEnumName(), value->GetInt());
    break;
  case TYPE_CODE_PTR:
    sprintf(p,"<ptr> %s", paddr(value->GetInt()));
    break;
  default:
    sprintf(p,"** dunno **");
    break;
  }
}


void showsymbol(CDC *p,int x, int y, CSymbol *symbol)
{
  CValue *value = symbol->ReadVarValue (CFrameInfo::GetCurrentFrame());
  char where[100];
  char name[100];
  /* Describe where it is */
  if (value) {
    switch (value->GetLVal()) {
    case lval_memory:
      sprintf(where,"%s ", paddr(value->GetAddress()));
      break;
    case lval_register:
      sprintf(where,"r%d ", value->GetRegno());
      break;
    default:
      sprintf(where,"??");
      break;
    }
    //  sprintf(where1,"frame %x", value->GetFrame());
    sprintf(name,"%s = ", symbol->GetName());
    //  strcat(where, where1);
    strcat(where,  name);
    
    char b[200];
    type_print(symbol, b, value);	
    strcat(where, b);
    p->TextOut(x,y,where);
  }
}

void CFrameDialog::DrawItemWorker(CDC *p,int x, int y, CSymbol *symbol)
{
  showsymbol(p,x,y,symbol);
}

void CFrameDialog::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lp ) 
{
  theframe = this;
  p1 = nIDCtl;
  p2 = lp;
      remembered = 1;
  catch_errors (call_protected_OnDrawItem, 0, "Error!", 2);
}
void CFrameDialog::protected_OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lp ) 
{
  switch (lp->itemAction) 
    {
    case ODA_DRAWENTIRE:
      {
	int x = lp->rcItem.left;
	int y = lp->rcItem.top;
	
	CDC* pDC = CDC::FromHandle(lp->hDC);	
	
	pDC->SelectObject(&frame_fontinfo.m_font);
	pDC->ExtTextOut(x,y, ETO_OPAQUE, &(lp->rcItem),"",0, NULL);
	if (lp->itemID!= ~0) 
	  {
	    CSymbol * p = (CSymbol *)data.GetItemData(lp->itemID);	
	    showsymbol(pDC, x,y,p);
	  }
      }
      
      break;
      
    case ODA_FOCUS:
      // Toggle the focus state
      
      //FTYPE::DrawItem( lp) 	;
      ::DrawFocusRect(lp->hDC, &(lp->rcItem));
      break;
      
    case ODA_SELECT:
      //FTYPE::DrawItem(  lp) 	 ;
      // Toggle the selection state
      //        ::InvertRect(lp->hDC, &(lp->rcItem));
      break;
    default:
      break;
    }
  
}

void CFrameDialog::OnSelchangeFrameDepth() 
{
  // TODO: Add your control notification handler code here
  rethink();	
}

void CFrameDialog::OnFrameGoto() 
{
  //name.ResetContent();
  CFrameInfo *fi;
  int de  ;
  int want = name.GetCurSel();
  int maxd = depth.GetCurSel()+1;
  for (de  = 0, fi = CFrameInfo::GetCurrentFrame();
       fi && de  < maxd;
       fi = fi->get_prev_frame())
    {	
      if (de == want)
	{
	  theApp_show_at (fi->pc);
	  return;
	}
      de++;
    }
}

void CFrameDialog::OnSize(UINT nType, int cx, int cy) 
{
  CFormView::OnSize(nType, cx, cy);
  
  int border = 10;
  int x1 = border;
  int x3 = cx - border;
  int x2 = x3  - ( m_iFontHeight * 8);
  int y1 = border;
  int y4 = cy - border;
  int y2 = y1 +  ( m_iFontHeight * 2);
  int y3 = (y1 + y4) * 1 / 5;
#if 0  
  if (remembered) {
    CRect databox;
    CRect original;
    
    /* 
       Make sure that the contents retain their original size and shape
       so we keep the bottom rhc of the data box the same distance from
       the rhbc of he container 
       */
    
    GetClientRect(original);
    data.GetClientRect(databox);
    data.SetWindowPos (0, 0,0,
		       original.Width() - databox_indent.x - databox.left,
		       original.Height() - databox_indent.y - databox.top,
		       SWP_NOMOVE|SWP_NOZORDER);	
    
  }
  
#endif
  if (remembered) {
  data.SetWindowPos (0 ,x1, y3 + border, x3-x1, y4-y3-border, SWP_NOACTIVATE | SWP_NOZORDER);
  depth.SetWindowPos(0, x2, y2, x3-x2, y3-y2, SWP_NOACTIVATE | SWP_NOZORDER);
  name.SetWindowPos (0, x1, y1, x2-x1-border, y3-y1, SWP_NOACTIVATE | SWP_NOZORDER);
  gbutton.SetWindowPos (0, x2, y1, x3-x2, y2-y1, SWP_NOACTIVATE | SWP_NOZORDER);
 }
  
}


void CFrameDialog::Initialize()
     
     
     
{
  frame_fontinfo.Initialize();
}


void CFrameDialog::Terminate()
{
  
  frame_fontinfo.Terminate();
  
  
}


void CFrameDialog::OnShowWindow(BOOL bShow, UINT nStatus) 
{

#if 0
  CRect databox;
  CRect original;
  if (!remembered)
    {

      
      
      
      data.GetWindowRect(databox);
      GetWindowRect(original);
      
      
      databox_indent.x = original.right - databox.right;
      databox_indent.y = original.bottom - databox.bottom;
    }
#endif
  CFormView::OnShowWindow(bShow, nStatus);
  
  // TODO: Add your message handler code here
  
}
