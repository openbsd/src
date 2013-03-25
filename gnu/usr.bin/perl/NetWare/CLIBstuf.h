
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	CLIBstuf.h
 * DESCRIPTION	:	The purpose of clibstuf is to make sure that Perl, cgi2perl and
 *                  all the perl extension nlm's (*.NLP) use the Novell Netware CLIB versions
 *                  of standard functions. This code loads up a whole bunch of function pointers
 *                  to point at the standard CLIB functions.
 * Author		:	HYAK
 * Date			:	January 2001.
 *
 */



#ifndef __CLIBstuf_H__
#define __CLIBstuf_H__


#ifdef __cplusplus
  extern "C"
  {
#endif

    void fnInitGpfGlobals(void);

#ifdef __cplusplus
  }
#endif


#endif	// __CLIBstuf_H__

