/****** Comm support ******************************************************/
#define NOPARITY            0
#define ODDPARITY           1
#define EVENPARITY          2
#define MARKPARITY          3
#define SPACEPARITY         4

#define ONESTOPBIT          0
#define ONE5STOPBITS        1
#define TWOSTOPBITS         2

#define IGNORE              0
#define INFINITE16          0xFFFF

/* Error Flags */
#define CE_RXOVER           0x0001
#define CE_OVERRUN          0x0002
#define CE_RXPARITY         0x0004
#define CE_FRAME            0x0008
#define CE_BREAK            0x0010
#define CE_CTSTO            0x0020
#define CE_DSRTO            0x0040
#define CE_RLSDTO           0x0080
#define CE_TXFULL           0x0100
#define CE_PTO              0x0200
#define CE_IOE              0x0400
#define CE_DNS              0x0800
#define CE_OOP              0x1000
#define CE_MODE             0x8000

#define IE_BADID            (-1)
#define IE_OPEN             (-2)
#define IE_NOPEN            (-3)
#define IE_MEMORY           (-4)
#define IE_DEFAULT          (-5)
#define IE_HARDWARE         (-10)
#define IE_BYTESIZE         (-11)
#define IE_BAUDRATE         (-12)

/* Events */
#define EV_RXCHAR           0x0001
#define EV_RXFLAG           0x0002
#define EV_TXEMPTY          0x0004
#define EV_CTS              0x0008
#define EV_DSR              0x0010
#define EV_RLSD             0x0020
#define EV_BREAK            0x0040
#define EV_ERR              0x0080
#define EV_RING             0x0100
#define EV_PERR             0x0200
#define EV_CTSS             0x0400
#define EV_DSRS             0x0800
#define EV_RLSDS            0x1000
#define EV_RingTe           0x2000
#define EV_RINGTE           EV_RingTe

/* Escape Functions */
#define SETXOFF             1
#define SETXON              2
#define SETRTS              3
#define CLRRTS              4
#define SETDTR              5
#define CLRDTR              6
#define RESETDEV            7

#define LPTx                0x80

#if (WINVER >= 0x030a)

/* new escape functions */
#define GETMAXLPT           8
#define GETMAXCOM           9
#define GETBASEIRQ          10

/* Comm Baud Rate indices */
#define CBR16_110      0xFF10
#define CBR16_300      0xFF11
#define CBR16_600      0xFF12
#define CBR16_1200     0xFF13
#define CBR16_2400     0xFF14
#define CBR16_4800     0xFF15
#define CBR16_9600     0xFF16
#define CBR16_14400    0xFF17
#define CBR16_19200    0xFF18
#define CBR16_38400    0xFF1B
#define CBR16_56000    0xFF1F
#define CBR16_128000   0xFF23
#define CBR16_256000   0xFF27

/* notifications passed in low word of lParam on WM_COMMNOTIFY messages */
#define CN_RECEIVE  0x0001
#define CN_TRANSMIT 0x0002
#define CN_EVENT    0x0004

#endif  /* WINVER >= 0x030a */

typedef struct tagDCB16
{
    BYTE Id;
    USHORT BaudRate;
    BYTE ByteSize;
    BYTE Parity;
    BYTE StopBits;
    USHORT RlsTimeout;
    USHORT CtsTimeout;
    USHORT DsrTimeout;

    USHORT fBinary        :1;
    USHORT fRtsDisable    :1;
    USHORT fParity        :1;
    USHORT fOutxCtsFlow   :1;
    USHORT fOutxDsrFlow   :1;
    USHORT fDummy         :2;
    USHORT fDtrDisable    :1;

    USHORT fOutX          :1;
    USHORT fInX           :1;
    USHORT fPeChar        :1;
    USHORT fNull          :1;
    USHORT fChEvt         :1;
    USHORT fDtrflow       :1;
    USHORT fRtsflow       :1;
    USHORT fDummy2        :1;

    char XonChar;
    char XoffChar;
    USHORT XonLim;
    USHORT XoffLim;
    char PeChar;
    char EofChar;
    char EvtChar;
    USHORT TxDelay;
} DCB16;
typedef DCB16 *LPDCB16;

#if (defined(STRICT) | (WINVER >= 0x030a))

typedef struct tagCOMSTAT16
{
    BYTE status;
    USHORT cbInQue;
    USHORT cbOutQue;
} COMSTAT16;

#define CSTF_CTSHOLD    0x01
#define CSTF_DSRHOLD    0x02
#define CSTF_RLSDHOLD   0x04
#define CSTF_XOFFHOLD   0x08
#define CSTF_XOFFSENT   0x10
#define CSTF_EOF        0x20
#define CSTF_TXIM       0x40

#else   /* (STRICT | WINVER >= 0x030a) */

/* NOTE: This structure declaration is not ANSI compatible! */
typedef struct tagCOMSTAT16
{
    BYTE fCtsHold  :1;
    BYTE fDsrHold  :1;
    BYTE fRlsdHold :1;
    BYTE fXoffHold :1;
    BYTE fXoffSent :1;
    BYTE fEof      :1;
    BYTE fTxim     :1;
    USHORT cbInQue;
    USHORT cbOutQue;
} COMSTAT16;

#endif  /* !(STRICT | WINVER >= 0x030a */

#define WM_COMMNOTIFY           0x0044

/*** Function Prototypes ****/

int APIENTRY BuildCommDCB16(LPCSTR lpszDef, DCB16 *lpdcb);
int APIENTRY OpenComm16(LPCSTR lpszDevControl, USHORT cbInQueue, USHORT cbOutQueue, LPCSTR dcb);
int APIENTRY CloseComm16(INT idComDev);
int APIENTRY ReadComm16(INT idComDev, LPVOID lpBuf, INT cbRead);
int APIENTRY WriteComm16(INT idComDev, LPCSTR lpBuf, INT cbWrite);
int APIENTRY FlushComm16(INT idComDev, INT fnQueue);
int APIENTRY TransmitCommChar16(INT idComDev, char chTransmit);
int APIENTRY SetCommState16(DCB16 *lpdcb); 
int APIENTRY GetCommState16(INT idComDev,DCB16 *lpdcb); 
int APIENTRY GetCommError16(INT idComDev,COMSTAT16 *lpcommstat);
  int APIENTRY GetCommReady16(INT idComDev, char *);
int APIENTRY SetCommBreak16(INT idComDev);
int APIENTRY ClearCommBreak16(INT idComDev);
 

#if 0
	  __declspec (dllexport)        int APIENTRY serial_isr_open();
	      __declspec (dllexport)    int APIENTRY serial_isr_close();
	      __declspec (dllexport)    int APIENTRY serial_isr_putchar(int);
	     __declspec (dllexport)     int APIENTRY serial_isr_getchar();

#endif
