
class CColorInfo 
{
 private:
  char *m_name;    /* Name of area */
 public:
  COLORREF m_value; /* Value of color */
void (*func)();

  CColorInfo(char *wCol, long v, void (*func)()=0);
void change(COLORREF n);
  void Initialize();
  void Terminate();
  void OnChooseColor();
};
