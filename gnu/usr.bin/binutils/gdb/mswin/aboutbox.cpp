#include "stdafx.h"
#include "aboutbox.h"
#include <dos.h>
#include <direct.h>


/////////////////////////////////////////////////////////////////////////////
// CAboutBox dialog

BEGIN_MESSAGE_MAP(CAboutBox, CDialog)
	//{{AFX_MSG_MAP(CAboutBox)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CAboutBox::CAboutBox(CWnd* pParent /*=NULL*/)
	: CDialog(CAboutBox::IDD, pParent)
{
	//{{AFX_DATA_INIT(CAboutBox)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

void CAboutBox::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutBox)
	//}}AFX_DATA_MAP
}

/////////////////////////////////////////////////////////////////////////////
// CAboutBox message handlers

BOOL CAboutBox::OnInitDialog()
{
	CDialog::OnInitDialog();

	// initialize the big icon control
	m_icon.SubclassDlgItem(ID_CMD_BUTTON_BIGICON, this);
	m_icon.SizeToContent();
	m_myface.AutoLoad(ID_CMD_BUTTON_MYFACE, this);
	m_myface.LoadBitmaps(ID_SYM_BITMAP_SAC);
	m_myface.SizeToContent();

	return TRUE;  // return TRUE  unless you set the focus to a control
}

/////////////////////////////////////////////////////////////////////////////
// CSplashWnd dialog

BEGIN_MESSAGE_MAP(CSplashWnd, CDialog)
	//{{AFX_MSG_MAP(CSplashWnd)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CSplashWnd::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSplashWnd)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BOOL CSplashWnd::Create(CWnd* pParent)
{
	if (!CDialog::Create(CSplashWnd::IDD, pParent))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL CSplashWnd::OnInitDialog()
{
	CDialog::OnInitDialog();
	CenterWindow();

	// initialize the big icon control
	m_icon.SubclassDlgItem(ID_CMD_BUTTON_BIGICON, this);
	m_icon.SizeToContent();

	return TRUE;  // return TRUE  unless you set the focus to a control
}

/////////////////////////////////////////////////////////////////////////////
// CSplashWnd message handlers

/////////////////////////////////////////////////////////////////////////////
// CBigIcon

BEGIN_MESSAGE_MAP(CBigIcon, CButton)
	//{{AFX_MSG_MAP(CBigIcon)
	ON_WM_DRAWITEM()
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBigIcon message handlers

#define CY_SHADOW   4
#define CX_SHADOW   4

void CBigIcon::SizeToContent()
{
	// get system icon size
	int cxIcon = ::GetSystemMetrics(SM_CXICON);
	int cyIcon = ::GetSystemMetrics(SM_CYICON);

	// a big icon should be twice the size of an icon + shadows
	SetWindowPos(NULL, 0, 0, cxIcon*2 + CX_SHADOW + 4, cyIcon*2 + CY_SHADOW + 4,
		SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER);
}

void CBigIcon::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);
	ASSERT(pDC != NULL);

	CRect rect;
	GetClientRect(rect);
	int cxClient = rect.Width();
	int cyClient = rect.Height();

	// load icon
	HICON hicon = AfxGetApp()->LoadIcon(ID_SYM_ICON_GDB);
	if (hicon == NULL)
		return;

	// draw icon into off-screen bitmap
	int cxIcon = ::GetSystemMetrics(SM_CXICON);
	int cyIcon = ::GetSystemMetrics(SM_CYICON);

	CBitmap bitmap;
	if (!bitmap.CreateCompatibleBitmap(pDC, cxIcon, cyIcon))
		return;
	CDC dcMem;
	if (!dcMem.CreateCompatibleDC(pDC))
		return;
	CBitmap* pBitmapOld = dcMem.SelectObject(&bitmap);
	if (pBitmapOld == NULL)
		return;

	// blt the bits already on the window onto the off-screen bitmap
	dcMem.StretchBlt(0, 0, cxIcon, cyIcon, pDC,
		2, 2, cxClient-CX_SHADOW-4, cyClient-CY_SHADOW-4, SRCCOPY);

	// draw the icon on the background
	dcMem.DrawIcon(0, 0, hicon);

	// draw border around icon
	CPen pen;
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

	// draw the icon contents
	pDC->StretchBlt(2, 2, cxClient-CX_SHADOW-4, cyClient-CY_SHADOW-4,
		&dcMem, 0, 0, cxIcon, cyIcon, SRCCOPY);
}

BOOL CBigIcon::OnEraseBkgnd(CDC*)
{
	return TRUE;    // we don't do any erasing...
}


