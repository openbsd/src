#include "stdafx.h"
#include "colinfo.h"

 


CColorInfo::CColorInfo(char *f1, long v, void (*f)())
{
  m_name = f1;
  m_value = v;
  func =f ;
}

void CColorInfo::OnChooseColor()
{
  CColorDialog color (0,0,0);	
  if (color.DoModal() == IDOK)
    {
      m_value = color.GetColor();
    }

}

void CColorInfo::Initialize()
{
  long l;
  l  =   AfxGetApp()->GetProfileInt(m_name, "color", -1);
  if (l != -1)
    m_value = l;
}


void CColorInfo::Terminate()
{
  AfxGetApp()->WriteProfileInt(m_name, "color", m_value);
}


void CColorInfo::change(COLORREF x)
{
  m_value = x;
  if (func)
    func();
}
