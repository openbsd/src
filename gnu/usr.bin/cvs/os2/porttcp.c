/****************************************************************
**
**	PORTTCP.C	- Support for portable TCP/IP
**
****************************************************************/

#define TCPIP_IBM_NOHIDE
#include	<stdio.h>
#include	"tcpip.h"

/*
 * Common unknown error buffer
 */
static char ErrUnknownBuf[36];

#ifndef SockStrError

/****************************************************************
 *	Routine: SockStrError
 *	Returns: Pointer to static buffer
 *	Action : Convert SOCK_ERRNO into error text
 ****************************************************************/

const char *
SockStrError(int SockErrno)
{
#if defined (TCPIP_IBM)  && defined (IBM_CPP)
  switch (SockErrno)
    {
    case SOCEPERM:		return "Not owner";
    case SOCESRCH:		return "No such process";
    case SOCEINTR:		return "Interrupted system call";
    case SOCENXIO:		return "No such device or address";
    case SOCEBADF:		return "Bad file number";
    case SOCEACCES:		return "Permission denied";
    case SOCEFAULT:		return "Bad address";
    case SOCEINVAL:		return "Invalid argument";
    case SOCEMFILE:		return "Too many open files";
    case SOCEPIPE:		return "Broken pipe";
    case SOCEOS2ERR:		return "OS/2 Error";
    case SOCEWOULDBLOCK:	return "Operation would block";
    case SOCEINPROGRESS:	return "Operation now in progress";
    case SOCEALREADY:		return "Operation already in progress";
    case SOCENOTSOCK:		return "Socket operation on non-socket";
    case SOCEDESTADDRREQ:	return "Destination address required";
    case SOCEMSGSIZE:		return "Message too long";
    case SOCEPROTOTYPE:		return "Protocol wrong type for socket";
    case SOCENOPROTOOPT:	return "Protocol not available";
    case SOCEPROTONOSUPPORT:    return "Protocol not supported";
    case SOCESOCKTNOSUPPORT:    return "Socket type not supported";
    case SOCEOPNOTSUPP:		return "Operation not supported on socket";
    case SOCEPFNOSUPPORT:	return "Protocol family not supported";
    case SOCEAFNOSUPPORT:
      return "Address family not supported by protocol family";
    case SOCEADDRINUSE:		return "Address already in use";
    case SOCEADDRNOTAVAIL:	return "Can't assign requested address";
    case SOCENETDOWN:		return "Network is down";
    case SOCENETUNREACH:	return "Network is unreachable";
    case SOCENETRESET:		return "Network dropped connection on reset";
    case SOCECONNABORTED:	return "Software caused connection abort";
    case SOCECONNRESET:		return "Connection reset by peer";
    case SOCENOBUFS:		return "No buffer space available";
    case SOCEISCONN:		return "Socket is already connected";
    case SOCENOTCONN:		return "Socket is not connected";
    case SOCESHUTDOWN:		return "Can't send after socket shutdown";
    case SOCETOOMANYREFS:	return "Too many references: can't splice";
    case SOCETIMEDOUT:		return "Connection timed out";
    case SOCECONNREFUSED:	return "Connection refused";
    case SOCELOOP:		return "Too many levels of symbolic links";
    case SOCENAMETOOLONG:	return "File name too long";
    case SOCEHOSTDOWN:		return "Host is down";
    case SOCEHOSTUNREACH:	return "No route to host";
    case SOCENOTEMPTY:		return "Directory not empty";

    default:
      sprintf( ErrUnknownBuf, "SockStrErrno( %d ) unknown", SockErrno );
      return ErrUnknownBuf;
    }
#else
#error	SockStrError not supported for this OS
#endif
}

#endif /* SockStrError */


/****************************************************************
 *	Routine: HostStrError
 *	Returns: Pointer to static buffer
 *	Action : Convert HOST_ERRNO into error text
 ****************************************************************/

const char *
HostStrError(int HostErrno)
{
  switch (HostErrno) 
    {
    case HOST_NOT_FOUND:
      return "Host not found";
    case TRY_AGAIN:
      return "Host not found (suggest try again)";
    case NO_RECOVERY:
      return "Non-recoverable error: FORMERR, REFUSED, NOTIMP";
    case NO_DATA:
      return "No Data (valid name, but no record of requested type)";

    default:
      sprintf( ErrUnknownBuf, "HostStrErrno( %d ) unknown", HostErrno );
      return ErrUnknownBuf;
    }
}


#if defined( TCPIP_IBM )
/****************************************************************
 * Routine: IbmSockSend
 * Returns: same as send
 * Action : Do the right thing for IBM TCP/IP which includes
 *	    the following two stupidities:
 *		1) Never try to send more than 32K
 *		2) Never pass a buffer that crosses a 64K boundary
 *     	 If Flags is non-zero, this function only attempts
 *	 to deal with condition (1) above.
 ****************************************************************/

int
IbmSockSend (int Socket, const void *Buffer, int Len, int Flags)
{
	int Sent, ToSend, TotalSent = 0;

	const char *Tmp = Buffer;

    /*
     * If Flags have been passed in, the 64K boundary optimization
     * can not be performed.  For example, MSG_PEEK would not work
     * correctly.
     */
	if (Flags)
          return send (Socket, (char *) Buffer, min (0x7FFF, Len), Flags);

	do
        {
          /* Never send across a 64K boundary */
          ToSend = min (Len, (int) (0x10000 -  (0xFFFF & (long) Tmp)));

          /* Never send more than 32K */
          if (ToSend > 0x7FFF) 
            ToSend = 0x7FFF;

          Sent = send (Socket, (char *) Tmp, ToSend, 0);
          if (Sent < 0)
            {
              if ((TotalSent > 0) && (SOCK_ERRNO == EWOULDBLOCK))
                return TotalSent;
              if (SOCK_ERRNO == EINTR)
                continue;
              return Sent;
            }
          if (Sent < ToSend)
            return TotalSent + Sent;
          
          Tmp += Sent;
          TotalSent += Sent;
          Len -= Sent;
	} while (Len > 0);
        
	return TotalSent;
}



/****************************************************************
 * Routine: IbmSockRecv
 * Returns: same as recv
 * Action : Do the right thing for IBM TCP/IP which includes
 *          the following two stupidities:
 *			1) Never try to recv more than 32K
 *			2) Never pass a buffer that crosses a 64K boundary
 *		 If Flags is non-zero, this function only attempts
 *		 to deal with condition (1) above.
 ****************************************************************/

int
IbmSockRecv (int Socket, const void *Buffer, int Len, int Flags)
{
  int Recvd, ToRecv, TotalRecvd = 0;

  char *Tmp = Buffer;

  /* If Flags have been passed in, the 64K boundary optimization
     probably can not be performed. */

	if (Flags)
          return recv (Socket, Buffer, min (0x7FFF, Len), Flags);

	do
	{
          /* Never send across a 64K boundary */
          ToRecv = min( Len, (int)( 0x10000 - ( 0xFFFF & (long)Tmp )));
          
          /* Never send more than 32K */
          if( ToRecv > 0x7FFF )
            ToRecv = 0x7FFF;

          Recvd = recv (Socket, Tmp, ToRecv, 0);
          if (Recvd <= 0)
            {
              if ((TotalRecvd > 0)
                  && (Recvd == 0 || (SOCK_ERRNO == EWOULDBLOCK )))
                return TotalRecvd;
              if (SOCK_ERRNO == EINTR)
                continue;
              
              return Recvd;
            }
          if (Recvd < ToRecv)
            return TotalRecvd + Recvd;
          
          Tmp += Recvd;
          TotalRecvd += Recvd;
          Len -= Recvd;
	} while (Len > 0);
        
	return TotalRecvd;
}
#endif /* defined( TCPIP_IBM ) */

