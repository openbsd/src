// transbmp.h : interface of the CTransBitmap class
//
/////////////////////////////////////////////////////////////////////////////

class CTransBmp : public CBitmap
{
public:
    CTransBmp();
    ~CTransBmp();
    void Draw(HDC hDC, int x, int y);
    void Draw(CDC* pDC, int x, int y);
    void DrawTrans(HDC hDC, int x, int y);
    void DrawTrans(CDC* pDC, int x, int y);
    int GetWidth();
    int GetHeight();

private:
    int m_iWidth;
    int m_iHeight;
    HBITMAP m_hbmMask;    // handle to mask bitmap

    void GetMetrics();
    void CreateMask(HDC hDC);
};

/////////////////////////////////////////////////////////////////////////////
