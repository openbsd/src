
class CFontInfo 
{
 public:
  void (*func)();
  CRect where;
  void SetUp(CREATESTRUCT &cs);
  CString windowname;
 private:


 public:
  LOGFONT m_lfDefFont;
  LOGFONT m_lfDefFontOld;
  CFont m_font;
  CFontInfo(const char *windowname, void (*func)()=0);
  void ChangeFont(LOGFONT *p);
  void Initialize();
  void Terminate();
  void OnChooseFont();
  void MakeFont();
  int m_fontheight;
  void CalcHeight();
  CSize dunits;
};
