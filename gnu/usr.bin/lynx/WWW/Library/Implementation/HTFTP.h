/*                                                               FTP access module for libwww
                                   FTP ACCESS FUNCTIONS

   This isn't really  a valid protocol module -- it is lumped together with HTFile . That
   could be changed easily.

   Author: Tim Berners-Lee.  Public Domain.  Please mail changes to timbl@info.cern.ch

 */
#ifndef HTFTP_H
#define HTFTP_H

#include <HTAnchor.h>
#include <HTStream.h>
#include <HTParse.h>

#define FILE_BY_NAME 0
#define FILE_BY_TYPE 1
#define FILE_BY_SIZE 2
#define FILE_BY_DATE 3
extern int HTfileSortMethod;  /* specifies the method of sorting */


/* PUBLIC						 HTVMS_name()
**		CONVERTS WWW name into a VMS name
** ON ENTRY:
**	nn		Node Name (optional)
**	fn		WWW file name
**
** ON EXIT:
**	returns		vms file specification
**
** Bug:	Returns pointer to static -- non-reentrant
*/
PUBLIC char * HTVMS_name PARAMS((
	CONST char * nn,
	CONST char * fn));


/*

Retrieve File from Server

  ON EXIT,

  returns                 Socket number for file if good.<0 if bad.

 */
extern int HTFTPLoad PARAMS
((
  CONST char *          name,
  HTParentAnchor *      anchor,
  HTFormat              format_out,
  HTStream*             sink
));

/*
**  This function frees any user entered password, so that
**  it must be entered again for a future request. - FM
*/
extern void HTClearFTPPassword NOPARAMS;

/*

Return Host Name

 */
extern CONST char * HTHostName NOPARAMS;

#endif
