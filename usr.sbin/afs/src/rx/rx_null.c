/*	$OpenBSD: rx_null.c,v 1.1.1.1 1998/09/14 21:53:16 art Exp $	*/
/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

#include "rx_locl.h"

RCSID("$KTH: rx_null.c,v 1.3 1998/02/22 19:48:47 joda Exp $");

/* The null security object.  No authentication, no nothing. */

static struct rx_securityOps null_ops;
static struct rx_securityClass null_object;

struct rx_securityClass *
rxnull_NewServerSecurityObject()
{
    null_object.ops = &null_ops;
    return &null_object;
}

struct rx_securityClass *
rxnull_NewClientSecurityObject()
{
    null_object.ops = &null_ops;
    return &null_object;
}
