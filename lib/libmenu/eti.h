/*-----------------------------------------------------------------------------+
|   The ncurses Extended Terminal Interface (ETI) is Copyright (C) 1995-1997   |
|             by Juergen Pfeifer <Juergen.Pfeifer@T-Online.de>                 |
|                          All Rights Reserved.                                |
|                                                                              |
| Permission to use, copy, modify, and distribute this software and its        |
| documentation for any purpose and without fee is hereby granted, provided    |
| that the above copyright notice appear in all copies and that both that      |
| copyright notice and this permission notice appear in supporting             |
| documentation, and that the name of the above listed copyright holder(s) not |
| be used in advertising or publicity pertaining to distribution of the        |
| software without specific, written prior permission.                         | 
|                                                                              |
| THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD TO  |
| THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FIT-  |
| NESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR   |
| ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RE- |
| SULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, |
| NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH    |
| THE USE OR PERFORMANCE OF THIS SOFTWARE.                                     |
+-----------------------------------------------------------------------------*/

#ifndef _ETI_ERRNO_H_
#define _ETI_ERRNO_H_

#define	E_OK			(0)
#define	E_SYSTEM_ERROR	 	(-1)
#define	E_BAD_ARGUMENT	 	(-2)
#define	E_POSTED	 	(-3)
#define	E_CONNECTED	 	(-4)
#define	E_BAD_STATE	 	(-5)
#define	E_NO_ROOM	 	(-6)
#define	E_NOT_POSTED		(-7)
#define	E_UNKNOWN_COMMAND	(-8)
#define	E_NO_MATCH		(-9)
#define	E_NOT_SELECTABLE	(-10)
#define	E_NOT_CONNECTED	        (-11)
#define	E_REQUEST_DENIED	(-12)
#define	E_INVALID_FIELD	        (-13)
#define	E_CURRENT		(-14)

#endif
