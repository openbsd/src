// browserf.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "browserf.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBrowserFilter

CBrowserFilter::CBrowserFilter()
{
}

CBrowserFilter::~CBrowserFilter()
{
}


BEGIN_MESSAGE_MAP(CBrowserFilter, CComboBox)
	//{{AFX_MSG_MAP(CBrowserFilter)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CBrowserFilter message handlers

void CBrowserFilter::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct) 
{
	// TODO: Add your code to determine the size of specified item
	
}
