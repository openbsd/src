
// browserl.cpp : implementation file
//

#include "stdafx.h"
#include "browserl.h"
#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif


extern CFontInfo srcbfontinfo;
/////////////////////////////////////////////////////////////////////////////
// CBrowserList

CBrowserList::CBrowserList()
{
  srcbfontinfo.MakeFont();
  m_show_address = 0;
  m_show_lines = 0;
}

CBrowserList::~CBrowserList()
{
}


BEGIN_MESSAGE_MAP(CBrowserList, CListBox)
     //{{AFX_MSG_MAP(CBrowserList)
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     
     /////////////////////////////////////////////////////////////////////////////
     // CBrowserList message handlers
     
int CBrowserList::CompareItem(LPCOMPAREITEMSTRUCT lp) 
{
#if 0
	p = (union gui_symtab *)GetItemDataPtr(lp->itemID);
switch (p->type) {
case GUI_FILE:
	return 0;
case GUI_ITEM:
	if (p->
  // TODO: Add your code to determine the sorting order of the specified items
  // return -1 = item 1 sorts before item 2
  // return 0 = item 1 and item 2 sort the same
  // return 1 = item 1 sorts after item 2
#endif
  return 0;
}

void CBrowserList::DrawItem(LPDRAWITEMSTRUCT lp) 
{
  switch (lp->itemAction) 
    {
    case ODA_DRAWENTIRE:
      {

	char bu[200];	
	union gui_symtab *p;
	  CDC* pDC = CDC::FromHandle(lp->hDC);	
	pDC->SelectObject(&srcbfontinfo.m_font);
	p = (union gui_symtab *)GetItemDataPtr(lp->itemID);
	int x = lp->rcItem.left;
	int y = lp->rcItem.top;
#if 0	
	p->ExtTextOut(lp->hDC, 
		     lp->rcItem.left,
		     lp->rcItem.top,
		     ETO_OPAQUE,
		     &(lp->rcItem),
		     "", 0,
		     NULL);
#endif
	pDC->ExtTextOut(x,y, ETO_OPAQUE, &(lp->rcItem),"",0, NULL);
//pDC->TextOut(x,y,"HI STEVE");
#if 1
	switch (p->type) 
	  {
	  case GUI_FILE:
	    sprintf(bu," %s%s", p->as_file.tab->get_filename(),
				p->as_file.opened ? "" : "...");
	    pDC->TextOut(x, y ,bu);
	    break;
	  case GUI_ITEM:	
	    sprintf(bu,"     %-4d %08x %s", 
		    p->as_item.sym->GetLine(),
 		    p->as_item.sym->GetValue(),
		    p->as_item.sym->GetName());
	    pDC->TextOut(x, y ,bu);
	  }
#endif
      }
    
      break;
    
    case ODA_FOCUS:
      // Toggle the focus state
      
      //CListBox::DrawItem( lp) 	;
      ::DrawFocusRect(lp->hDC, &(lp->rcItem));
      break;
      
    case ODA_SELECT:
      //CListBox::DrawItem(  lp) 	 ;
      // Toggle the selection state
      //        ::InvertRect(lp->hDC, &(lp->rcItem));
      break;
    default:
      break;
    }
  
}


void CBrowserList::Initialize()
{
}

void CBrowserList::Terminate()
{

}



void CBrowserList::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct) 
{
	// TODO: Add your code to determine the size of specified item
	
}
