#include "stdafx.h"


static char *szHeight = "Height";
static char *szWeight = "Weight";
static char *szItalic = "Italic";
static char *szUnderline = "Underline";
static char *szPitchAndFamily = "PitchAndFamily";
static char *szFaceName = "FaceName";
static char *szSystem = "System";




CFontInfo::CFontInfo(const char *f1, void (*f2)())
{
  windowname = f1;
  func = f2;
}

void CFontInfo::MakeFont()
{
  CalcHeight();
}
void CFontInfo::OnChooseFont()
{
  // TODO: Add your command handler code here
  // get current font description
  
  LOGFONT lf = m_lfDefFont;
  CFontDialog dlg(&lf, CF_SCREENFONTS|CF_INITTOLOGFONTSTRUCT|CF_FIXEDPITCHONLY);
  if (dlg.DoModal() == IDOK)
    {
      m_font.DeleteObject();
      m_font.CreateFontIndirect(&lf);
      m_lfDefFont = lf;
    }	
  CalcHeight();
}


static void GetProfileFont(LPCSTR szSec, LOGFONT* plf)
{
  CWinApp* pApp = AfxGetApp();
  plf->lfHeight = pApp->GetProfileInt(szSec, szHeight, 0);
  if (plf->lfHeight != 0)
    {
      plf->lfWeight = pApp->GetProfileInt(szSec, szWeight, 0);
      plf->lfItalic = (BYTE)pApp->GetProfileInt(szSec, szItalic, 0);
      plf->lfUnderline = (BYTE)pApp->GetProfileInt(szSec, szUnderline, 0);
      plf->lfPitchAndFamily = (BYTE)pApp->GetProfileInt(szSec, szPitchAndFamily, 0);
      CString strFont = pApp->GetProfileString(szSec, szFaceName, szSystem);
      strncpy((char*)plf->lfFaceName, strFont, sizeof plf->lfFaceName);
      plf->lfFaceName[sizeof plf->lfFaceName-1] = 0;
    }
  else {
    /* choose a sensible default */
    plf->lfHeight=-12;
    strcpy (  plf->lfFaceName,"FixedSys");
  }
}
static void WriteProfileFont(LPCSTR szSec, const LOGFONT* plf, LOGFONT* plfOld)
{
  CWinApp* pApp = AfxGetApp();
  
  if (plf->lfHeight != plfOld->lfHeight)
    pApp->WriteProfileInt(szSec, szHeight, plf->lfHeight);
  if (plf->lfHeight != 0)
    {
      if (plf->lfHeight != plfOld->lfHeight)
	pApp->WriteProfileInt(szSec, szHeight, plf->lfHeight);
      if (plf->lfWeight != plfOld->lfWeight)
	pApp->WriteProfileInt(szSec, szWeight, plf->lfWeight);
      if (plf->lfItalic != plfOld->lfItalic)
	pApp->WriteProfileInt(szSec, szItalic, plf->lfItalic);
      if (plf->lfUnderline != plfOld->lfUnderline)
	pApp->WriteProfileInt(szSec, szUnderline, plf->lfUnderline);
      if (plf->lfPitchAndFamily != plfOld->lfPitchAndFamily)
	pApp->WriteProfileInt(szSec, szPitchAndFamily, plf->lfPitchAndFamily);
      if (strcmp(plf->lfFaceName, plfOld->lfFaceName) != 0)
	pApp->WriteProfileString(szSec, szFaceName, (LPCSTR)plf->lfFaceName);
    }
  *plfOld = *plf;
}

static TCHAR szFormat[] = "%u,%u,%u,%u";

void CFontInfo::Initialize()
{
  CWinApp* pApp = AfxGetApp();
  CString buf;
  GetProfileFont (windowname, &m_lfDefFont);
  m_lfDefFontOld = m_lfDefFont;
  //  GetProfileFont (m_PrintFont, &m_lfDefPrintFont);
  //  m_lfDefPrintFontOld = m_lfDefPrintFont;
  
  m_font.CreateFontIndirect(&m_lfDefFont);
  
#if 0
  CString strBuffer = pApp->GetProfileString(windowname,"where");
  if (!strBuffer.IsEmpty())
    {
      int nRead = _stscanf(strBuffer, szFormat,
			   &where.left,    &where.top, &where.right, &where.bottom);
    }
#endif
  
  CalcHeight();
}


void CFontInfo::Terminate()
{
  CWinApp* pApp = AfxGetApp();
  WriteProfileFont (windowname, &m_lfDefFont, &m_lfDefFontOld);
#if 0
  TCHAR szBuffer[sizeof("-32767")*8 + sizeof("65535")*2];
  wsprintf(szBuffer, szFormat,  where.left,where.top, where.right, where.bottom);
  pApp->WriteProfileString(windowname, "where", szBuffer);
#endif
  
}	


void CFontInfo::CalcHeight()
{
  CClientDC dc(0);
  dc.SelectObject(&m_font);
  CSize one_char = dc.GetTextExtent("M",1);
  m_fontheight = one_char.cy;
  
  dunits = dc.GetTextExtent("abcdefghijklmopqrstuvwxyzABCDEFGHIJKLMOPQRSTUVWXYZ",52);
  dunits.cx /= 52;
}

void CFontInfo::SetUp(CREATESTRUCT &cs)
{
  abort();
  //cs.cx = w;
  // cs.cy = h;
}

void CFontInfo::ChangeFont(LOGFONT *lf)
{
  m_font.DeleteObject();
  m_font.CreateFontIndirect(lf);
  if (func)
    func();
  m_lfDefFont = *lf;
  CalcHeight();
}

