/*                               /Net/dxcern/userd/timbl/hypertext/WWW/Library/src/HTTCP.html
                               GENERIC TCP/IP COMMUNICATION

   This module has the common code for handling TCP/IP connections etc.

 */
#ifndef HTTCP_H
#define HTTCP_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif
 
/*      Produce a string for an internet address
**      ---------------------------------------
**
** On exit:
**           returns a pointer to a static string which must be copied if
**                it is to be kept.
*/
#ifndef _WINDOWS
extern CONST char * HTInetString PARAMS((struct sockaddr_in* mysin));
#endif


/*      Encode INET status (as in sys/errno.h)                    inet_status()
**      ------------------
**
** On entry:
**              where gives a description of what caused the error
**      global errno gives the error number in the unix way.
**
** On return:
**      returns a negative status in the unix way.
*/
#ifdef __STDC__
        extern int HTInetStatus(char *where);
#else
        extern int HTInetStatus();
#endif

/*      Publicly accessible variables
*/
/* extern struct sockaddr_in HTHostAddress; */
                        /* The internet address of the host */
                        /* Valid after call to HTHostName() */


/*      Parse a cardinal value                                 parse_cardinal()
**      ----------------------
**
** On entry:
**      *pp points to first character to be interpreted, terminated by
**      non 0..9 character.
**      *pstatus points to status already valid,
**      maxvalue gives the largest allowable value.
**
** On exit:
**      *pp points to first unread character,
**      *pstatus points to status updated iff bad
*/

extern unsigned int HTCardinal PARAMS((int *pstatus,
                char            **pp,
                unsigned int    max_value));

/*	Check whether string is a valid Internet hostname
**	-------------------------------------------------
*/

extern BOOL valid_hostname PARAMS((CONST char * name));

/*	Resolve an internet hostname, like gethostbyname
**	------------------------------------------------
**
**  On entry,
**	str	points to the given host name, not numeric address,
**		without colon or port number.
**
**  On exit,
**	returns a pointer to a struct hostent in static storage,
**	or NULL in case of error or user interruption.
**
**  The interface is intended to be the same as for gethostbyname(),
**  but additional status is returned in lynx_nsl_status.
*/
extern int lynx_nsl_status;

#ifndef DJGPP
extern struct hostent * LYGetHostByName PARAMS((
	CONST char *	str));
#endif /* DJGPP */


/*      Parse an internet node address and port
**      ---------------------------------------
**
** On entry:
**               str points to a string with a node name or number,
**               with optional trailing colon and port number.
**               sin points to the binary internet or decnet address field.
**
** On exit:
**               *sin is filled in.  If no port is specified in str, that
**               field is left unchanged in *sin.
*/
#ifdef __STDC__
        extern int HTParseInet(struct sockaddr_in * mysin, CONST char * str);
        /*!! had to change this to get it to compile. CTB */
#else
        extern int HTParseInet();
#endif

/*      Get Name of This Machine
**      ------------------------
**
*/

extern CONST char * HTHostName NOPARAMS;

extern int HTDoConnect PARAMS((
	CONST char *	url,
	char *		protocol,
	int		default_port,
	int *		s));

extern int HTDoRead PARAMS((
	int 		fildes,
	void *		buf,
	unsigned 	nbyte));

#endif   /* HTTCP_H */
