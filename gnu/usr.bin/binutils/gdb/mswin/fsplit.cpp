#if 0
#include "stdafx.h"
#include "fsplit.h"
#include "srcwin.h"
#include "guiframe.h"

 
/////////////////////////////////////////////////////////////////////////////
// CFSplit

IMPLEMENT_DYNCREATE(CFSplit, CMDIChildWnd)

CFSplit::CFSplit()
{
}

CFSplit::~CFSplit()
{
}

BOOL CFSplit::OnCreateClient(LPCREATESTRUCT /*lpcs*/, CCreateContext* pContext)
{
CRect x;
GetClientRect(x);
  m_wndSplitter.CreateStatic(this, 2, 1);
  m_wndSplitter.CreateView (0, 0, 
	RUNTIME_CLASS(CGuiFrameWrap), CSize(100,x.Height()*3/4), pContext);
  m_wndSplitter.CreateView (1, 0,
	 RUNTIME_CLASS(CGuiFrameWrap), CSize(100,x.Height()/4), pContext);

return TRUE;
}


BEGIN_MESSAGE_MAP(CFSplit, CMDIChildWnd)
	//{{AFX_MSG_MAP(CFSplit)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


#endif
