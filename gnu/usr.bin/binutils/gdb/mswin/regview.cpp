// regview.cpp : implementation file
//

#include "stdafx.h"
#include "regview.h"
#include "regdoc.h"
#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CRegView

IMPLEMENT_DYNCREATE(CRegView, CScrollView)
extern CGuiApp theApp;
void redraw_allregwins()
{ 
redraw_allwins(theApp.m_regTemplate);
}

 CFontInfo reg_fontinfo  ("RegFont", redraw_allregwins);

CRegView::CRegView()
{
  m_base = 16;
  reg_fontinfo.MakeFont();

   
}

CRegView::~CRegView()
{
}


BEGIN_MESSAGE_MAP(CRegView, CScrollView)
	//{{AFX_MSG_MAP(CRegView)
	ON_WM_CHAR()
	ON_COMMAND(ID_REAL_CMD_BUTTON_REG_BINARY, OnRegBinary)
	ON_COMMAND(ID_REAL_CMD_BUTTON_REG_DECIMAL, OnRegDecimal)
	ON_COMMAND(ID_REAL_CMD_BUTTON_REG_EVERYTHING, OnRegEverything)
	ON_COMMAND(ID_REAL_CMD_BUTTON_REG_HEX, OnRegHex)
	ON_COMMAND(ID_REAL_CMD_BUTTON_REG_OCTAL, OnRegOctal)
	ON_WM_SIZE()
	ON_COMMAND(ID_REAL_CMD_BUTTON_SET_FONT, OnSetFont)
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CRegView drawing

#define REG_SIZE (BFD_ARCH_SIZE/8)
#define VAL_SIZE (BFD_ARCH_SIZE/8)
#define NUM_COLS 4
static   const int gap = 20;
#define WIDTHINPIX ((m_regsize.cx + gap + m_valuesize.cx * NUM_COLS))
#define DEPTHINPIX ((togdb_maxregs() / NUM_COLS + 2) * reg_fontinfo.dunits.cy)

int CRegView::format(char *buf, CORE_ADDR value, int which)
{
  int i;
  switch (which == 0 ? m_base:which) 
    {
    case 2:
      for (i = 0; i < BFD_ARCH_SIZE; i++) 
	{
	  buf[i] = (value >> ((BFD_ARCH_SIZE-1) - i ))&1 ? '1':'0';
	}
      break;
    case 8:
      i = sprintf(buf,"%11o", value); 
      break;
    case 10:
      i = sprintf(buf,"%10d", value);
      break;
    case 16:
      i =  sprintf(buf,"%08X", value);
      break;

    default:
      i = format(buf, value,2);
      i += format(buf+i, value,8);
      i += format(buf+i, value,10);
      i += format(buf+i, value,16);
      break;
    }

  buf[i++] = ' ';
  buf[i] = 0;
  return  i;
}


void CRegView::BestShape()
{

#if 0
  char buf[100];
  CClientDC d(NULL);
  RECT wnd;
  GetClientRect(&wnd);
  d.SelectObject(&reg_fontinfo.m_font);


  /* Try our prefered shapes */
  
  int i;
  for (i = 0; px[i]; i++) 
    {
      m_ronline = px[i];
      m_numlines = py[i];
      
      mx = m_regsize.cx + (m_valuesize.cx + gap) * m_ronline ;
      my = m_regsize.cy * m_numlines;
      
      if (mx < wnd.right && my < wnd.bottom) 
	{
	  /* Use this and resize the area */
 	  rethink();
#if 1
	  CFrameWnd* pFrame = GetParentFrame();
	  pFrame->SetWindowPos(NULL, 0, 0, mx + 40, my+40 ,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
#endif
	  return ;
	}
    }
  /* mx and my are now the size needed to show all the regs, so set the
     scroll up */
  GetParentFrame()->SetWindowPos(NULL, 0, 0, mx + 40, my+40 ,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
  //  GetParentFrame()->MoveWindow(0,0,mx,my);
  rethink();
#endif
  
}


void CRegView::PrintReg(CRegDoc *pDoc, class CDC *pDC, int rn, int base, int cx, int cy)
{
  char buf[50*BFD_ARCH_SIZE];
  if(pDoc->m_regsinit[rn]) {
    format (buf, pDoc->m_regs[rn], base);
      
    if (pDoc->m_regchanged[rn]) 
      {
	pDC->SetTextColor(0x000000ff);
	pDC->TextOut(cx,cy, buf);
	pDC->SetTextColor(0x00000000);
      }
    else {
      pDC->TextOut(cx,cy, buf);
    }
  }
  else
    pDC->TextOut(cx,cy,".");
  
}

extern "C" {
  extern char *reg_names[];
  extern int reg_order[];
} ;

void CRegView::OnDraw(CDC* pDC)
{
  char buf[200];
  CRegDoc * pDoc = (CRegDoc *)GetDocument();      
  int i;
  
  int rn = 0;
  int ron = 0;
  int cx;
  int cy;
  int ri = 0;  
  cx = 0;
  cy = 0;
  pDoc->prepare();
  pDC->SelectObject(&reg_fontinfo.m_font);  
  
  				m_ronline = 4; /* FIXME! what's this?? */
  while (reg_order[ri] >= 0) {		
    rn = reg_order[ri];
    pDC->TextOut(cx, cy, reg_names[rn]);
    cx += m_regsize.cx + gap;
    for(i = 0; i < m_ronline && reg_order[ri] >= 0; i++) 
      {
	PrintReg(pDoc,pDC, reg_order[ri], 0, cx, cy);
	cx += m_valuesize.cx + gap;
	ri++;
      }
    cy += m_valuesize.cy;
    cx = 0;
  }
  /* New line for ccr and pc */
  
  pDC->TextOut(cx, cy,"PC ");
  
  PrintReg(pDoc,pDC, togdb_pcreg(),16, m_regsize.cx + gap, cy);
  
  /* And the current insn */
  if (pDoc->m_regsinit[togdb_pcreg()]) 
    {
      togdb_disassemble (pDoc->m_regs[togdb_pcreg()], buf);
      cx = (m_regsize.cx  + gap)*3;
      pDC->TabbedTextOut(cx, cy, buf, strlen(buf), 0, 0, cx);
    }
  
  cy += m_valuesize.cy;
  
  pDC->TextOut(0, cy,reg_names[togdb_ccrreg()]);
  PrintReg(pDoc, pDC, togdb_ccrreg(),16, m_regsize.cx + gap, cy);
  
}


/////////////////////////////////////////////////////////////////////////////
// CRegView message handlers
static int v;
void CRegView::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
  // TODO: Add your message handler code here and/or call default
  
  if (nChar == 'a')
    {
      CRegDoc * pDoc = (CRegDoc *)GetDocument();      
      pDoc->ChangeReg(0,999 *v++);
      pDoc->UpdateAllViews(0);
    }
  
}

void CRegView::OnRegBinary() 
{
  m_base = 2;
  Invalidate();	
}

void CRegView::OnRegDecimal() 
{
  m_base = 10;
  Invalidate();		
}

void CRegView::OnRegEverything() 
{
  m_base = -1;
  Invalidate();	
}

void CRegView::OnRegHex() 
{
  m_base = 16;
  Invalidate();	
}

void CRegView::OnRegOctal() 
{
  m_base = 8;
  Invalidate();	
}



void CRegView::Initialize()
{
  reg_fontinfo.Initialize();
}

void CRegView::Terminate()
{
  reg_fontinfo.Terminate();
}



void CRegView::rethink()
{
  char buf[50*BFD_ARCH_SIZE];
  CORE_ADDR value0 = 0;
  int which0 = 0;
  int len = format(buf, value0, which0);
  m_valuesize.cx = (len+2) * reg_fontinfo.dunits.cx ;
  m_valuesize.cy = reg_fontinfo.dunits.cy ;

  m_regsize.cx = reg_fontinfo.dunits.cx * REG_SIZE;
  m_regsize.cy = reg_fontinfo.dunits.cy;


  CSize sizeTotal;
  CSize sizePage;
  CSize sizeLine;
  sizeTotal.cx = WIDTHINPIX;
  sizeTotal.cy = DEPTHINPIX;
  
  sizePage.cx = sizeTotal.cx/5;
  sizePage.cy = sizeTotal.cy/5;
  
  sizeLine.cx = sizeTotal.cx/10;
  sizeLine.cy = sizeTotal.cy/10;
  
  SetScrollSizes(MM_TEXT, sizeTotal, sizePage, sizeLine);

}

void CRegView::OnSize(UINT nType, int cx, int cy) 
{
  CScrollView::OnSize(nType, cx, cy);
  SetWindowText("Registers");
	rethink();
  
}



void CRegView::OnSetFont()
{
  reg_fontinfo.OnChooseFont();
  Invalidate();
}

BOOL CRegView::PreCreateWindow(CREATESTRUCT& cs) 
{
  // TODO: Add your specialized code here and/or call the base class
  return CScrollView::PreCreateWindow(cs);
}

void CRegView::OnInitialUpdate() 
{
  // TODO: Add your specialized code here and/or call the base class
  CScrollView::OnInitialUpdate();
  rethink();
  GetParentFrame()->SetWindowPos(NULL, 0, 0, 
			WIDTHINPIX, DEPTHINPIX,
				 SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
  
  load_where(GetParentFrame(),"REG");
}

void CRegView::OnDestroy() 
{
  CScrollView::OnDestroy();
  save_where(GetParentFrame(),"REG");
  
}
