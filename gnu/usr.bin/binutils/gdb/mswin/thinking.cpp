#if 0
// thinking.cpp : implementation file
//

#include "stdafx.h"
#include "thinking.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CThinking

CThinking::CThinking()
{
}

CThinking::~CThinking()
{
}


BEGIN_MESSAGE_MAP(CThinking, CWnd)
	//{{AFX_MSG_MAP(CThinking)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CThinking message handlers

extern CWnd  *theframe;
BOOL CThinking::Create()
{
CRect rect(0,0,200,20);
return CControlBar::Create(this,NULL,  WS_CHILD|WS_VISIBLE|CBRS_TOP|CBRS_TOOLTIPS|CBRS_FLYBY,
		rect, theframe.0);
}

void CThinking::OnPaint() 
{
CPaintDC dc(this); // device context for painting
char b[200];
sprintf(b,"HI %d", val);
dc.TextOut(0,0,b);
}

void CThinking::set_text(const char *) {}
void CThinking::set_limit(int x) {}
void CThinking::set_now(int x)
{
 val = x; 
SendMessage(WM_PAINT);
}
#endif