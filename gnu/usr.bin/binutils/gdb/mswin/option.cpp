// option.cpp : implementation file
//

#include "stdafx.h"
#include "option.h"
#include "colinfo.h"
#include "xm.h"
#include "dirpkr.h"
#include "srcd.h"
#include "srcwin.h"
#ifdef _DEBUG

#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COptionsSheet

IMPLEMENT_DYNAMIC(COptionsSheet, CPropertySheet)

COptionsSheet::COptionsSheet() : CPropertySheet("Properties",0,0)
{
  AddPage(&ser);
  AddPage(&fonts);
  AddPage(&dirs);
  AddPage(&colors);
}


COptionsSheet::~COptionsSheet()
{
}


BEGIN_MESSAGE_MAP(COptionsSheet, CPropertySheet)
	//{{AFX_MSG_MAP(COptionsSheet)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// COptionsSheet message handlers
/////////////////////////////////////////////////////////////////////////////
// COptionsColor property page

IMPLEMENT_DYNCREATE(COptionsColor, CPropertyPage)

struct maps {
	int n;
	char *name;
	CColorInfo *what;
};
extern CColorInfo colinfo_s;
extern CColorInfo colinfo_a;
extern CColorInfo colinfo_b;

static struct maps map[] = 
{
{	0,"Source Text",&colinfo_s},
{	1,"Assembly Text",&colinfo_a},
{	2,"Source Background",&colinfo_b},
	0,0,0};

#define NV 20
COLORREF col;
static COLORREF cvals[NV] =
{
  RGB(0,0,0),
  RGB(128,0,0),
  RGB(0,128,0),
  RGB(128,128,0),
  RGB(0,0,128),
  RGB(128,0,128),
  RGB(0,128,128),
  RGB(192,192,192),
  RGB(192,220,192),
  RGB(166,202,240),
  RGB(255,251,240),
  RGB(160,160,164),
  RGB(128,128,128),
  RGB(255,0,0),
  RGB(0,255,0),
  RGB(255,255,0),
  RGB(0,0,255),
  RGB(255,0,255),
  RGB(0,255,255),
  RGB(255,255,255)
};

COptionsColor::COptionsColor() : CPropertyPage(COptionsColor::IDD)
{
  int i;
  //{{AFX_DATA_INIT(COptionsColor)
  //}}AFX_DATA_INIT
  for (i = 0; i < NC; i++) {
    bs[i].LoadBitmaps(ID_SYM_BITMAP_CUP, ID_SYM_BITMAP_CDOWN, ID_SYM_BITMAP_CFOCUS);
    bs[i].set_color(cvals[i]);
  }
  sel = -1;
}
COptionsColor::~COptionsColor()
{
}

void COptionsColor::DoDataExchange(CDataExchange* pDX)
{
  CPropertyPage::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(COptionsColor)
  DDX_Control(pDX, ID_CMD_BUTTON_WINDS, m_list);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionsColor, CPropertyPage)
     //{{AFX_MSG_MAP(COptionsColor)
     ON_BN_CLICKED(ID_CMD_BUTTON_C0, OnC0)
     ON_BN_CLICKED(ID_CMD_BUTTON_C1, OnC1)
     ON_BN_CLICKED(ID_CMD_BUTTON_C2, OnC2)
     ON_BN_CLICKED(ID_CMD_BUTTON_C3, OnC3)
     ON_BN_CLICKED(ID_CMD_BUTTON_C4, OnC4)
     ON_BN_CLICKED(ID_CMD_BUTTON_C5, OnC5)
     ON_BN_CLICKED(ID_CMD_BUTTON_C6, OnC6)
     ON_BN_CLICKED(ID_CMD_BUTTON_C7, OnC7)
     ON_BN_CLICKED(ID_CMD_BUTTON_C8, OnC8)
     ON_BN_CLICKED(ID_CMD_BUTTON_C9, OnC9)
     ON_BN_CLICKED(ID_CMD_BUTTON_C10, OnC10)
     ON_BN_CLICKED(ID_CMD_BUTTON_C11, OnC11)
     ON_BN_CLICKED(ID_CMD_BUTTON_C12, OnC12)
     ON_BN_CLICKED(ID_CMD_BUTTON_C13, OnC13)
     ON_BN_CLICKED(ID_CMD_BUTTON_C14, OnC14)
     ON_BN_CLICKED(ID_CMD_BUTTON_C15, OnC15)
     ON_BN_CLICKED(ID_CMD_BUTTON_C16, OnC16)
     ON_BN_CLICKED(ID_CMD_BUTTON_C17, OnC17)
     ON_BN_CLICKED(ID_CMD_BUTTON_C18, OnC18)
     ON_BN_CLICKED(ID_CMD_BUTTON_C19, OnC19)
     ON_LBN_DBLCLK(ID_CMD_BUTTON_WINDS, OnDblclkWinds)
     ON_LBN_SETFOCUS(ID_CMD_BUTTON_WINDS, OnSetfocusWinds)
     ON_LBN_SELCHANGE(ID_CMD_BUTTON_WINDS, OnSelchangeWinds)
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     
     /////////////////////////////////////////////////////////////////////////////
     // COptionsColor message handlers
     
     
#define CY_SHADOW   2
#define CX_SHADOW   2
     
     void border(CDC *pDC, CRect &rect)
{
  
  CPen pen;
  int cxClient = rect.Width();
  int cyClient = rect.Height();
  
  pen.CreateStockObject(BLACK_PEN);
  CPen* pPenOld = pDC->SelectObject(&pen);
  pDC->Rectangle(0, 0, cxClient-CX_SHADOW, cyClient-CY_SHADOW);
  if (pPenOld)
    pDC->SelectObject(pPenOld);
  
  // draw shadows around icon
  CBrush br;
  br.CreateStockObject(DKGRAY_BRUSH);
  rect.SetRect(cxClient-CX_SHADOW, CY_SHADOW, cxClient, cyClient);
  pDC->FillRect(rect, &br);
  rect.SetRect(CX_SHADOW, cyClient-CY_SHADOW, cxClient, cyClient);
  pDC->FillRect(rect, &br);
  
  
}

void COptionsColor::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT pDI)
{
#if 0
  static int c;
  CRect rect = pDI->rcItem;
  CDC *pDC =CDC::FromHandle( pDI->hDC);
  int n = nIDCtl - ID_CMD_BUTTON_C0;
  if (n < 0 || n >= NV)
    n = 0;
  
  //  border(pDC, rect);
  //  rect.InflateRect(-2,-2);
  CPen pen(PS_SOLID, pDI->itemState & ODS_SELECTED ? 3: 0, RGB(0,0,0));
#if 0
  if (pDI->itemState & ODS_FOCUS) {
    ::DrawFocusRect(pDI->hDC, &(pDI->rcItem));
  }
#endif
  
  CBrush br(cvals[n]);
  CPen  *oldpen = pDC->SelectObject(&pen);
  CBrush *oldbrush = pDC->SelectObject(&br);
  //  pDC->FillRect(rect,&br);
  //  rect.InflateRect(2,2);
  pDC->RoundRect(rect, CPoint(2,2));
  pDC->SelectObject(oldpen);
  pDC->SelectObject(oldbrush);
#endif
}
void nprops()
{
  COptionsSheet o;
  
  
if (o.DoModal()) {
	 o.ser.to_gdb();
//	o.dirs.remember(o.dirs .forwhat.GetCurSel());
}
}
void COptionsColor::OnC0() {c(0); }
void COptionsColor::OnC1() {c(1); }
void COptionsColor::OnC2() {c(2); }
void COptionsColor::OnC3() {c(3); }
void COptionsColor::OnC4() {c(4); }
void COptionsColor::OnC5() {c(5); }
void COptionsColor::OnC6() {c(6); }
void COptionsColor::OnC7() {c(7); }
void COptionsColor::OnC8() {c(8); }
void COptionsColor::OnC9() {c(9); }

void COptionsColor::OnC10() {c(10); }
void COptionsColor::OnC11() {c(11); }
void COptionsColor::OnC12() {c(12); }
void COptionsColor::OnC13() {c(13); }
void COptionsColor::OnC14() {c(14); }
void COptionsColor::OnC15() {c(15); }
void COptionsColor::OnC16() {c(16); }
void COptionsColor::OnC17() {c(17); }
void COptionsColor::OnC18() {c(18); }
void COptionsColor::OnC19() {c(19); }

static int mapc[] = { 
  ID_CMD_BUTTON_C0,
  ID_CMD_BUTTON_C1,
  ID_CMD_BUTTON_C2,
  ID_CMD_BUTTON_C3,
  ID_CMD_BUTTON_C4,
  ID_CMD_BUTTON_C5,
  ID_CMD_BUTTON_C6,
  ID_CMD_BUTTON_C7,
  ID_CMD_BUTTON_C8,
  ID_CMD_BUTTON_C9,
  ID_CMD_BUTTON_C10,
  ID_CMD_BUTTON_C11,
  ID_CMD_BUTTON_C12,
  ID_CMD_BUTTON_C13,
  ID_CMD_BUTTON_C14,
  ID_CMD_BUTTON_C15,
  ID_CMD_BUTTON_C16,
  ID_CMD_BUTTON_C17,
  ID_CMD_BUTTON_C18,
  ID_CMD_BUTTON_C19};
BOOL COptionsColor::OnInitDialog() 
{
  CPropertyPage::OnInitDialog();
  
  for (int i =0; i < NC; i++) {
    
    bs[i].SubclassDlgItem(mapc[i], this);
    bs[i].SizeToContent();
  }
  
  for (i = 0; map[i].name; i++) 
    {
      m_list.AddString(map[i].name);
    }
  
  //  thing.SubclassDlgItem(ID_CMD_BUTTON_THING, this);
  
  return TRUE; 
  
}


void CMyBitmapButton::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
  CDC *pDC =CDC::FromHandle( lpDIS->hDC);
  CBitmapButton::DrawItem(lpDIS);
  CRect r = &lpDIS->rcItem;
  r.InflateRect(-5,-5);
  CBrush br(col);
  CPen pen(PS_SOLID, lpDIS->itemState & ODS_SELECTED ? 5: 0, RGB(0,0,0));
  CPen  *oldpen = pDC->SelectObject(&pen);
  CBrush *oldbrush = pDC->SelectObject(&br);
  pDC->RoundRect(r, CPoint(2,2));
  pDC->SelectObject(oldpen);
  pDC->SelectObject(oldbrush);
}

void COptionsColor::highlight(int x)
{
  int y;
  
  for (y = 0; y < NV; y++)
    bs[y].SetState(0);	
  if (x>=0)
    bs[x].SetState(1);
}

int COptionsColor::findb(int sel)
{
  if (sel >= 0) {
    COLORREF x = map[sel].what->m_value;
    int j;
    for (j = 0; j < NV; j++)
      if (x == cvals[j])
	return j;
  }
  return -1;
  
}
void COptionsColor::OnDblclkWinds() 
{
  sel = m_list.GetCurSel();
  highlight(findb(sel));
  
}

void COptionsColor::OnSelchangeWinds() 
{
  sel = m_list.GetCurSel();
  highlight(findb(sel));
  
}


void COptionsColor::c(int x)
{
  if (sel >= 0)
    map[sel].what->change(cvals[x]);
  highlight(x);
}

void COptionsColor::OnSetfocusWinds() 
{
  sel = m_list.GetCurSel();
  highlight(findb(sel));
}


/////////////////////////////////////////////////////////////////////////////
  // COptionsDir property page
  
  IMPLEMENT_DYNCREATE(COptionsDir, CPropertyPage)
    
    COptionsDir::COptionsDir() : CPropertyPage(COptionsDir::IDD)
      {
       //{{AFX_DATA_INIT(COptionsDir)
	//}}AFX_DATA_INIT
prev_sel = -1;
     }
  
  COptionsDir::~COptionsDir()
    {
   }
  
  void COptionsDir::DoDataExchange(CDataExchange* pDX)
    {

     CPropertyPage::DoDataExchange(pDX);
     //{{AFX_DATA_MAP(COptionsDir)
	DDX_Control(pDX, IDC_PATH_FOR_WHAT, forwhat);
	DDX_Control(pDX, ID_REAL_CMD_BUTTON_UP, up);
	DDX_Control(pDX, ID_CMD_BUTTON_REMOVE, remove);
	DDX_Control(pDX, ID_CMD_BUTTON_DOWN, down);
	DDX_Control(pDX, ID_CMD_BUTTON_ADD, add);
	DDX_Control(pDX, ID_CMD_BUTTON_PATH, path_list);
	//}}AFX_DATA_MAP
   }
  
  
  BEGIN_MESSAGE_MAP(COptionsDir, CPropertyPage)
    //{{AFX_MSG_MAP(COptionsDir)
	ON_BN_CLICKED(ID_CMD_BUTTON_ADD, OnAdd)
	ON_BN_CLICKED(ID_CMD_BUTTON_DOWN, OnDown)
	ON_BN_CLICKED(ID_CMD_BUTTON_REMOVE, OnRemove)
	ON_BN_CLICKED(ID_REAL_CMD_BUTTON_UP, OnUp)
	ON_LBN_SELCHANGE(ID_CMD_BUTTON_PATH, OnSelchangePath)
	ON_LBN_SELCHANGE(IDC_PATH_FOR_WHAT, OnSelchangePathForWhat)
	//}}AFX_MSG_MAP
    END_MESSAGE_MAP()
      
      
      /////////////////////////////////////////////////////////////////////////////
	// COptionsDir message handlers

void COptionsDir::fillup       (const char *str, CListBox *box)
{
  int i;
  CString x = str;
  box->ResetContent();
  while ((i = x.Find(DIRNAME_SEPARATOR))>=0)
    {
      CString lhs = x.Left(i);
      box->AddString(lhs);
      x = x.Mid(i+1);
    }
  if (x.GetLength())
    {
      box->AddString(x);
    }
}
BOOL COptionsDir::OnInitDialog() 
{
    CPropertyPage::OnInitDialog();
 


#define SOURCE_FILE_PATH 1
#define INFO_FILE_PATH 0
/* Add them in opposite order */
  forwhat.AddString("Info file search");
  forwhat.AddString("Source file search");


forwhat.SetCurSel(0);
OnSelchangePathForWhat();
  
  return TRUE;			// return TRUE unless you set the focus to a control
  // EXCEPTION: OCX Property Pages should return FALSE
}

void COptionsColor::OnEditupdateWinds() 
{
	// TODO: Add your control notification handler code here


}

void COptionsDir::OnAdd() 
{
 COptionsAddPath x;
 if (x.DoModal() == IDOK)
   {
     path_list.AddString(x.path);
   }
}

void COptionsDir::OnRemove() 
{
  int mup = path_list.GetCurSel();
  path_list.DeleteString(mup); 
  path_list.SetCurSel(mup-1);
  OnSelchangePath();
}

void COptionsDir::move(int dir)
{
  CString str;
  int mup = path_list.GetCurSel();
  path_list.GetText(mup, str);
  path_list.DeleteString(mup);
  path_list.InsertString(mup+dir, str);
  path_list.SetCurSel(mup + dir);
  OnSelchangePath();
}
void COptionsDir::OnUp() 
{
  move(-1);
}
void COptionsDir::OnDown() 
{
 move(1);
}

void COptionsDir::OnSelchangePath() 
{
 int cidx = path_list.GetCurSel() ;
 int size = path_list.GetCount();

 if (cidx < 0) {
   up.EnableWindow(0);
   down.EnableWindow(0);
   remove.EnableWindow(0);
 }
 else {
   // TODO: Add your control notification handler code here
   up.EnableWindow(cidx > 0);
   down.EnableWindow(cidx < size-1);
   remove.EnableWindow(1);
 }
 UpdateData(0);
}
/////////////////////////////////////////////////////////////////////////////
// COptionsAddPath dialog


COptionsAddPath::COptionsAddPath(CWnd* pParent /*=NULL*/)
	: CDialog(COptionsAddPath::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptionsAddPath)
	path = _T("");
	//}}AFX_DATA_INIT
 
}


void COptionsAddPath::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	//{{AFX_DATA_MAP(COptionsAddPath)
	DDX_Text(pDX, ID_REAL_CMD_BUTTON_NEW_PATH, path);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionsAddPath, CDialog)
	//{{AFX_MSG_MAP(COptionsAddPath)
	ON_BN_CLICKED(ID_CMD_BUTTON_BROWSE, OnBrowse)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

 
/////////////////////////////////////////////////////////////////////////////
// COptionsAddPath message handlers

void COptionsAddPath::OnBrowse() 
{
 // TODO: Add your control notification handler code here
 CString x;
 if (dirpick(x, this))
   {
     path = x;
     UpdateData(0);
   }
}

void COptionsDir::remember(int sel)
{
  if (sel >= 0) 
    {
      int lim = path_list.GetCount();
      CString all;
      for (int i = 0; i < lim; i++)
	{
	  CString t;
	  if (i != 0)
	    all += DIRNAME_SEPARATOR;
	  path_list.GetText(i, t);
	  all += t;
	}

      switch (sel) 
	{
	case SOURCE_FILE_PATH:
	  togdb_set_source_path((const char *)all);
	  CSrcD::path_changed ++;	
	  redraw_allsrcwins();
	  break;
	case INFO_FILE_PATH:
	  togdb_set_info_path((const char *)all);
	  break;
	}
    }
}
void COptionsDir::OnOK()
{
  remember(forwhat.GetCurSel());
}

extern CGuiApp theApp;
void COptionsDir::Initialize()
{
  CString t = theApp.GetProfileString("DIR","SRC");
  if (!t.IsEmpty())
    {
      togdb_set_source_path((const char *)t);
    }
  t = theApp.GetProfileString("DIR","INFOPATH");
  if (getenv("INFOPATH")) 
    {
      t += ";";
      t += getenv ("INFOPATH");
    }
  if (!t.IsEmpty())
    {
      togdb_set_info_path((const char *)t);
    }
  
}

void COptionsDir::Terminate()
{
  theApp.WriteProfileString("DIR","SRC", togdb_get_source_path());
  theApp.WriteProfileString("DIR","INFOPATH", togdb_get_info_path());
}

void COptionsSheet::Initialize()
{
  COptionsDir::Initialize();
  COptionsSerial::Initialize();
}
void COptionsSheet::Terminate()
{
  COptionsDir::Terminate();
  COptionsSerial::Terminate();
}




void COptionsDir::OnSelchangePathForWhat() 
{
  remember(prev_sel);
  int i = forwhat.GetCurSel();
  prev_sel = i;
  
  if (i >= 0) 
    {   
      const char *s;
      switch (i) 
	{
	case SOURCE_FILE_PATH:
	  // TODO: Add extra initialization here
	  s = togdb_get_source_path();
	  break;
	case INFO_FILE_PATH:
	  // TODO: Add extra initialization here
	  s = togdb_get_info_path();
	  break;
	}
      fillup (s, &path_list);
    }
}
//////////////////////////////////////////////////////////////////////
// Font stuff

/////////////////////////////////////////////////////////////////////////////
// COptionsFont property page

IMPLEMENT_DYNCREATE(COptionsFont, CPropertyPage)
     
     COptionsFont::COptionsFont() : CPropertyPage(COptionsFont::IDD)
{
  //{{AFX_DATA_INIT(COptionsFont)
  show_unfixed = FALSE;
  prev_font_name_index = -1;
  prev_font_window_index = -1;
  //}}AFX_DATA_INIT
}

COptionsFont::~COptionsFont()
{
}

void COptionsFont::DoDataExchange(CDataExchange* pDX)
{
  CPropertyPage::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(COptionsFont)
  DDX_Control(pDX, ID_CMD_BUTTON_FONT_STYLE, font_style);
  DDX_Control(pDX, ID_CMD_BUTTON_FONT_SIZE, font_size);
  DDX_Control(pDX, ID_CMD_BUTTON_FONT_NAME, font_name);
  DDX_Control(pDX, ID_CMD_BUTTON_FONT_WINDOW, font_window);
  DDX_Control(pDX, ID_CMD_BUTTON_FONT_DEMO, font_demo);
  DDX_Check(pDX, IDC_SHOW_UNFIXED, show_unfixed);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionsFont, CPropertyPage)
     //{{AFX_MSG_MAP(COptionsFont)
     ON_LBN_SELCHANGE(ID_CMD_BUTTON_FONT_NAME, OnSelchangeFontName)
     ON_LBN_SELCHANGE(ID_CMD_BUTTON_FONT_SIZE, OnSelchangeFontSize)
     ON_LBN_SELCHANGE(ID_CMD_BUTTON_FONT_STYLE, OnSelchangeFontStyle)
     ON_LBN_SELCHANGE(ID_CMD_BUTTON_FONT_WINDOW, OnSelchangeFontWindow)
     ON_BN_CLICKED(IDC_SHOW_UNFIXED, OnShowUnfixed)
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     
     /////////////////////////////////////////////////////////////////////////////
     // COptionsFont message handlers
     
void COptionsFont::OnSelchangeFontSize() 
{
  enable_okones();
}



struct fmaps {
  char *name;
  CFontInfo *what;
};

extern CFontInfo srcbfontinfo;
extern CFontInfo cmdlogview_fontinfo;
extern CFontInfo iologview_fontinfo;
extern CFontInfo srcfontinfo;
extern CFontInfo reg_fontinfo;
//extern CFontInfo mem_fontinfo;
extern CFontInfo bptfontinfo;
extern CFontInfo bptfontinfo;
extern CFontInfo buttonfontinfo;
extern CFontInfo infobrowser_fontinfo;
extern CFontInfo frame_fontinfo;
static fmaps fmap[] = {
  { "Source Window", &srcfontinfo},
  { "Command Log", &cmdlogview_fontinfo},
  { "I/O Log", &iologview_fontinfo},
  { "Register Window", &reg_fontinfo},
  { "Button Font", &buttonfontinfo},
  { "Locals Window", &frame_fontinfo},
  { "Info Browser", &infobrowser_fontinfo},
  { "Source Browser", &srcbfontinfo},
  { "Breakpoint Window", &bptfontinfo},0};

//  { "Memory Window", &mem_fontinfo},0};

static int CALLBACK callback_fontname(const LOGFONTA  *l, 
				      const TEXTMETRIC *tm, 
				      unsigned long ft,
				      LPARAM p)
{
  COptionsFont *pf = (COptionsFont *)p;
  pf->UpdateData(TRUE);  
  if ( pf->show_unfixed
      || ((tm->tmPitchAndFamily  & 1) == 0)	 )
    {
      if ((ft & TRUETYPE_FONTTYPE) 
	  || (ft & RASTER_FONTTYPE))
	{
	  pf->font_name.AddString(l->lfFaceName);
	}
    }
  return 1;
}
#define L_BI 0
#define L_B 1
#define L_I 2
#define L_R 3
static char *ftnames[4] = {"Bold Italic","Bold","Italic","Regular"};
static
int ft(const LOGFONT *l)
{
  if (l->lfWeight == FW_BOLD)
    {
      if (l->lfItalic)
	return L_BI;
      else
	return L_B;
    }
  else  if (l->lfItalic)
    return L_I;
  else 
    return L_R;
}

static LOGFONT ds[40];
static int styles[10];
static int CALLBACK callback_fontstyle(const LOGFONT *l, 
				       const TEXTMETRIC *tm, 
				       unsigned long nFontType,
				       LPARAM p)
{
  ENUMLOGFONT *e = (ENUMLOGFONT *)l;
  NEWTEXTMETRIC *ntm =  (NEWTEXTMETRIC*)tm;
  COptionsFont *pf = (COptionsFont *)p;


  if (tm->tmPitchAndFamily & TMPF_TRUETYPE) 
    {
      /* True type font, called once for each style */
      char b[sizeof (e->elfStyle)+1];
      strncpy(b, (char *)(e->elfStyle), sizeof(b)-1);
      ds[pf->font_style.GetCount()] = *l;
      pf->font_style.AddString(b);
    }
  else  
    {
      /* Not a true type font, called once for each size .. 
	 but we'll just add the standard styles */
      {
	int f = ft(l);
	char b[20];
	int i;
	/* Only add the style if not already there */
	for (i = 0; i < pf->font_style.GetCount(); i++)
		if (styles[i] == f) return 1;
	styles[pf->font_style.GetCount()] = f;
	sprintf(b,"%s",ftnames[f]);
	pf->font_style.AddString(b);
      }
    }
  return 1;
}
static int sizes[] = {5,6,7,8,9,10,11,12,14,20,0};
static int CALLBACK callback_fontsizes(const LOGFONT *l, 
				       const TEXTMETRIC *tm, 
				       unsigned long nFontType,
				       LPARAM p)
{
  char b[20];
  CClientDC dc(0);
  NEWTEXTMETRIC *ntm =  (NEWTEXTMETRIC*)tm;
  
  COptionsFont *pf = (COptionsFont *)p;
  if (ntm->tmPitchAndFamily & TMPF_TRUETYPE)
    {
      if (pf->iteration == pf->font_style.GetCurSel())
	{
	  /* A truetype font has an infinite set of sizes, we'll just make
	     some up */
	  for (int j = 0; sizes[j]; j++)
	    {
	      sprintf(b,"%d",sizes[j]);
	      pf->font_size.AddString(b);
	      
	      ds[j] = *l;
	      ds[j].lfHeight = -MulDiv(sizes[j],
				       GetDeviceCaps(dc.m_hDC, LOGPIXELSY), 72);
	      ds[j].lfWidth = 0;
	    }
	  return 0;
	}
      pf->iteration++;
    }
  else 
    {
      /* Called once per possible entry, but only show sizes
	 of selected styles */
      if (styles[pf->font_style.GetCurSel()] == ft(l))
	{
	  int point =  MulDiv(l->lfHeight,72, GetDeviceCaps(dc.m_hDC, LOGPIXELSY));
	  sprintf(b,"%d", point);
	  ds[pf->font_size.GetCount()] = *l;
	  pf->font_size.AddString(b);
	}
    }
  return 1;
}

void COptionsFont::enumerate_sizes()
{
  CString fname;
  CClientDC dc(0);
  font_name.GetText(prev_font_name_index, fname);
  font_size.ResetContent();
  iteration = 0;
  
  EnumFontFamilies (dc.m_hDC, fname, callback_fontsizes, (LPARAM)this);      
}


void COptionsFont::enable_okones()
{
  CClientDC dc(0);
  UpdateData(TRUE);
  
  if (font_window.GetCurSel()<0) 
    {
      font_name.ResetContent();
    }
  else if (font_window.GetCurSel() != prev_font_window_index)
    {
      prev_font_window_index = font_window.GetCurSel();
      font_name.ResetContent();
      EnumFontFamilies (dc.m_hDC, NULL, callback_fontname, (LPARAM)this);      
      font_name.SetCurSel(-1);
    }
  
  if (font_name.GetCurSel() < 0) 
    {
      font_style.ResetContent();
    }
  else if (font_name.GetCurSel() != prev_font_name_index)
    {
      prev_font_name_index = font_name.GetCurSel();
      font_style.ResetContent();
      CString fname;
      font_name.GetText(prev_font_name_index, fname);
      EnumFontFamilies (dc.m_hDC, fname, callback_fontstyle, (LPARAM)this);      
      font_style.SetCurSel(-1);
    }
  if (font_style.GetCurSel() < 0) 
    {
      font_size.ResetContent();
    }
  else if (font_style.GetCurSel() != prev_font_style_index)
    {
      prev_font_style_index = font_style.GetCurSel();
	  prev_font_size_index = -1;
      enumerate_sizes();
    }
  if (font_size.GetCurSel() >= 0 && 
      font_size.GetCurSel() != prev_font_size_index)
    {
      prev_font_size_index = font_size.GetCurSel();
      
      curfont.DeleteObject();
      curfont.CreateFontIndirect(ds+prev_font_size_index);
      font_demo.SetFont(&curfont);
      font_demo.SetWindowText("Hello there this is a test");
      fmap[font_window.GetCurSel()].what->ChangeFont(ds + prev_font_size_index);
    }
  if (font_size.GetCurSel() < 0)
    {
      font_demo.SetWindowText("");
    }
}


BOOL COptionsFont::OnInitDialog() 
{
  CPropertyPage::OnInitDialog();
  
  for (int i = 0; fmap[i].name; i++)
    {
      font_window.AddString(fmap[i].name);
    }
  
  showup = 0;
  tfont = 0;
  enable_okones();
  return TRUE;
}


static int tt;


void COptionsFont::OnSelchangeFontName() 
{
  enable_okones();
	  prev_font_size_index = -1;
	  prev_font_style_index = -1;
}


void COptionsFont::show_styles(const char *font)
{
  CClientDC dc(0);
  font_style.ResetContent();
  first_call = 1;
  EnumFontFamilies (dc.m_hDC, font, callback_fontstyle, (LPARAM)this);
}

/* Find the highlighted font and light up the rows */
void COptionsFont::highlight_font()
{
  int idx = font_name.GetCurSel();
  if (idx <0) 
    {
      font_name.SetCurSel(0);
      idx = 0;
    }
  CString fname;
  font_name.GetText (idx, fname);
  show_styles (fname);
}

void COptionsFont::OnSelchangeFontWindow() 
{
  enable_okones();
prev_font_name_index = -1;
	  prev_font_size_index = -1;
	  prev_font_style_index = -1;
}

void COptionsFont::OnShowUnfixed() 
{
  /* Force a redraw of the window names */
  prev_font_window_index = -2;
  enable_okones();
}


void COptionsFont::OnSelchangeFontStyle() 
{
  enable_okones();  
  prev_font_size_index = -1;
}


/////////////////////////////////////////////////////////////////////////////
// COptionsSerial property page



IMPLEMENT_DYNCREATE(COptionsSerial, CPropertyPage)
extern "C" { 
extern char serial_port[];
extern int baud_rate;
extern int serial_bits;
extern int serial_parity;
extern int serial_stop_bits;
};


static int blist[] = { 300,600,1200,2400,4800,9600,14400,19200,38400,57600,0};


void COptionsSerial::Terminate()
{
  if (serial_port)
    theApp.WriteProfileString ("Serial","Port",serial_port);
  theApp.WriteProfileInt ("Serial","Baud", baud_rate);
  theApp.WriteProfileInt ("Serial","Bits",serial_bits);
  theApp.WriteProfileInt ("Serial","StopBits", serial_stop_bits);
  theApp.WriteProfileInt ("Serial","Parity", serial_parity);
}

void COptionsSerial::Initialize()
{
 CString r;
 r  = theApp.GetProfileString ("Serial","Port");
  strncpy (serial_port,(const char *)r,5);
  baud_rate = theApp.GetProfileInt ("Serial","Baud",9600);
  serial_bits = theApp.GetProfileInt ("Serial","Bits",8);
  serial_stop_bits = theApp.GetProfileInt ("Serial","StopBits",1);
  serial_parity = theApp.GetProfileInt ("Serial","Parity",2);
}
void  COptionsSerial::from_gdb()
{
  int i;
  for (i = 0; blist[i]; i++) 
    {
      if (blist[i] == baud_rate)	{
	m_baud = i;

      break;
	  }
    }
  m_parity = serial_parity;
  if (serial_port
      && strlen(serial_port)>=4	
      && serial_port[3] >= '1'
      && serial_port[3] <= '4')
    m_com = serial_port[3] - '1';
  switch (serial_bits) {
  case 8:
    m_databits = 1;
    break;
  case 7:
    m_databits = 0;
    break;
  }
  m_stopbits = serial_stop_bits-1;
}
void COptionsSerial::to_gdb()
{
  baud_rate = blist[m_baud];
  serial_parity = m_parity;
  strcpy (serial_port,"COM1:");
  serial_port[3] = m_com + '1';
  serial_bits = m_databits ? 8 : 7;
  serial_stop_bits = m_stopbits + 1;
}

COptionsSerial::COptionsSerial() : CPropertyPage(COptionsSerial::IDD)
{
	//{{AFX_DATA_INIT(COptionsSerial)
	//}}AFX_DATA_INIT
  	from_gdb();
}

COptionsSerial::~COptionsSerial()
{

}

void COptionsSerial::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionsSerial)
	DDX_Radio(pDX, IDC_B300, m_baud);
	DDX_Radio(pDX, IDC_PODD, m_parity);
	DDX_Radio(pDX, IDC_COM1, m_com);
	DDX_Radio(pDX, IDC_D7, m_databits);
	DDX_Radio(pDX, IDC_S1, m_stopbits);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionsSerial, CPropertyPage)
	//{{AFX_MSG_MAP(COptionsSerial)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// COptionsSerial message handlers


