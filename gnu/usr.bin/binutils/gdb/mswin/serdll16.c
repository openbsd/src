#ifndef APIENTRY
#define APIENTRY
#endif
#define W32SUT_16

#include "windefs.h"
#include <w32sut.h>
#include "serdll.h"

int FAR PASCAL
LibMain (HANDLE hLibInst, WORD wDataSeg,
	 WORD cbHeapSize, LPSTR lpszCmdLine)
{
  return (1);
}				// LibMain()

DWORD FAR PASCAL __export
UTInit (UT16CBPROC lpfnUT16CallBack, LPVOID lpBuf)
{
  return (1);			// Return Success

}				// UTInit()


#define DEBUG 0


DWORD FAR PASCAL __export
UTProc (DWORD * lpArgs, DWORD dwFunc)
{
  char DebugString[100];
  if (DEBUG)
    OutputDebugString ("YIBBLE\n");
  switch (dwFunc)
    {
    case BUILDCOMMDCB:
      if (DEBUG)
	{
	  wsprintf (DebugString,
		    "BuildCommDCB String=%s\n Buffer=%0lX ",
		    (LPSTR) lpArgs[0],
		    (LPVOID) lpArgs[1]);

	  OutputDebugString (DebugString);
	}


      return BuildCommDCB ((LPCSTR) lpArgs[0],
			   (LPVOID) lpArgs[1]);

    case OPENCOMM:
      {

	int i;
	DCB dcb;
	char *dev = (LPSTR) lpArgs[0];
	char *comm_string = (LPSTR) lpArgs[3];
	int j;
	memset (&dcb, 0, sizeof (dcb));
	if (DEBUG)
	  {
	    wsprintf (DebugString,
		      "NEW OpenComm String=%s Count1=%d Count2=%d %s\n",
		      (LPSTR) lpArgs[0],
		      (UINT) lpArgs[1],
		      (UINT) lpArgs[2],
		      (LPSTR) lpArgs[3]);

	    OutputDebugString (DebugString);
	  }

	i = OpenComm (dev, 512, 512);
	if (DEBUG)
	  {
	    wsprintf (DebugString,
		      "IRET %d %s\n", i, dev);

	    OutputDebugString (DebugString);
	  }
	if (i < 0)
	  return i;
#if 1
	BuildCommDCB (comm_string, &dcb);
	dcb.Id = i;
	dcb.fBinary = 1;
	dcb.fOutxCtsFlow = 0;
	dcb.fOutxDsrFlow = 0;
	dcb.fOutX = 0;
	dcb.fInX = 0;
	j = SetCommState (&dcb);
	if (DEBUG)
	  {
	    int j;
	    unsigned char *p = &dcb;
	    for (j = 0; j < sizeof (dcb); j++)
	      {
		wsprintf (DebugString, "%02x ", p[j]);
		OutputDebugString (DebugString);

	      }

	    OutputDebugString ("\n");
	  }
	if (j)
	  OutputDebugString ("SetCmmstate failed\n");

	if (DEBUG)
	  {
	    wsprintf (DebugString,
		      "IRET %d\n", i);

	    OutputDebugString (DebugString);
	  }
#endif
	return i;
      }


    case CLOSECOMM:
      if (DEBUG)
	{
	  wsprintf (DebugString,
		    "Close ComId=%d\n",
		    (int) lpArgs[0]);
	  OutputDebugString (DebugString);
	}
      return (CloseComm ((int) lpArgs[0]));


    case READCOMM:
      if (DEBUG)
	{
	  wsprintf (DebugString,
		    "ReadComm ComId=%d Buffer=%0lX Count=%d",
		    (int) lpArgs[0],
		    (LPSTR) lpArgs[1],
		    (int) lpArgs[2]);

	  OutputDebugString (DebugString);
	}
      return ReadComm ((int) lpArgs[0],
		       (LPSTR) lpArgs[1],
		       (int) lpArgs[2]);

    case WRITECOMM:
      if (DEBUG)
	{
	  wsprintf (DebugString,
		    "WriteComm ComId=%d Buffer=%0lX %c Count=%d\n",
		    (int) lpArgs[0],
		    (LPSTR) lpArgs[1], ((LPSTR) lpArgs[1])[0],
		    (int) lpArgs[2]);

	  OutputDebugString (DebugString);
	}
      return (WriteComm ((int) lpArgs[0],
			 (LPSTR) lpArgs[1],
			 (int) lpArgs[2]));



    case FLUSHCOMM:
      if (DEBUG)
	{
	  wsprintf (DebugString,
		    "FlushComm ComId=%d Queue=\n",
		    (int) lpArgs[0],
		    (int) lpArgs[1]);
	  OutputDebugString (DebugString);
	}
      return FlushComm ((int) lpArgs[0], (int) lpArgs[1]);

    case TRANSMITCOMMCHAR:
      if (DEBUG)
	{
	  wsprintf (DebugString,
		    "Transmit Comm Char ComId=%d Char=%c\n",
		    (int) lpArgs[0],
		    (char) lpArgs[1]);

	  OutputDebugString (DebugString);
	}
      return UngetCommChar ((int) lpArgs[0], (char) lpArgs[1]);

    case SETCOMMSTATE:
      if (DEBUG)
	{
	  wsprintf (DebugString,
		    "SetCommState Buffer=%0lX \n",
		    (LPVOID) lpArgs[0]);
	  OutputDebugString (DebugString);
	}
      return SetCommState ((LPVOID) lpArgs[0]);


    case GETCOMMSTATE:
      if (DEBUG)
	{
	  wsprintf (DebugString,
		    "GetCommState Comm Id=%d Buffer=%0lX \n",
		    (int) lpArgs[0],
		    (LPVOID) lpArgs[1]);
	  OutputDebugString (DebugString);
	}
      return (GetCommState ((int) lpArgs[0],
			    (LPVOID) lpArgs[1]));


    case GETCOMMERROR:
      {
	int x;
	COMSTAT *foo = (LPVOID *) lpArgs[1];
	COMSTAT local;
	if (DEBUG)
	  {
	    wsprintf (DebugString,
		      "GetCommError Comm Id=%d Buffer=%0lX \n",
		      (int) lpArgs[0],
		      (LPVOID) lpArgs[1]);
	    OutputDebugString (DebugString);
	  }
	x = GetCommError ((int) lpArgs[0], &local);
	if (DEBUG)
	  {
	    wsprintf (DebugString, "GC returns %x %d\n",
		      x, local.status, local.cbInQue);
	    OutputDebugString (DebugString);
	  }
	return x;
      }

    case GETCOMMREADY:
      {
	int x;
#define DEBUG 0
	COMSTAT local;
	int cid = lpArgs[0];
	if (DEBUG) {
	  wsprintf (DebugString,
		    "GC with %d", cid);
	      OutputDebugString (DebugString);
	}

	x = GetCommError (cid, &local);

	if (DEBUG)
	  {
	    if (local.status) {
	      wsprintf (DebugString,
			"GC returns %x %d\n", local.status, local.cbInQue);
	      OutputDebugString (DebugString);
	    }
	  }


	if (local.cbInQue)
	  {
	    char *buf = (char *) lpArgs[1];
	    char c;
	    int len = ReadComm (cid, buf, local.cbInQue);
	    if (len < 0)
	      {
		int err;
		len = -len;
		err = GetCommError (cid, NULL);
		GetCommEventMask (cid, 0xffff);
		if (DEBUG)
		  {
		    wsprintf (DebugString, "error %x\n", err);
		    OutputDebugString (DebugString);
		  }
	      }
	    buf[len] = 0;
	    if (DEBUG)
	      {
		wsprintf (DebugString, "FP %d$%d %s\n",
			  len, local.cbInQue, buf);
		OutputDebugString (DebugString);
	      }
	    return len;
	  }
	if (DEBUG)
	  OutputDebugString ("ECR\n");

	return 0;
#define DEBUG 0
      }


    }

  return ((DWORD) - 1L);
}
