/*
 * IFD handler for CITI sc7816 dumb reader driver
 *
 * Jim Rees, University of Michigan CITI, August 2000
 */
static char *rcsid = "$Id: ifdhandler.c,v 1.1 2001/05/22 15:35:57 rees Exp $";

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <string.h>

#include "todos_scrw.h"

/* capability table */
struct cap_entry {
    u_long tag;
    int flags; 
} cap_table[] = {
  {TAG_OPEN_FLAGS, 0}, /* open flags */
  {TAG_RESET_FLAGS, 0}, /* reset flags */
  {0, 0} /* terminator */
};

/* pcsc cruft */
#define MAX_ATR_SIZE 33
#define IFD_POWER_UP 500
#define IFD_POWER_DOWN 501
#define IFD_RESET 502
#define IFD_ERROR_NOT_SUPPORTED 606
#define IFD_ERROR_POWER_ACTION 608
#define IFD_COMMUNICATION_ERROR 612
#define IFD_NOT_SUPPORTED 614
#define IFD_ICC_PRESENT 615
#define IFD_ICC_NOT_PRESENT 616
#define TAG_IFD_ATR 0x303

struct SCARD_IO_HEADER {
    u_long Protocol, Length;
};

static int CT_ttyn;
static int CT_atrlen;
static unsigned char CT_atr[MAX_ATR_SIZE];

static short silly_channels[] = {
    0x3F8, 0x2F8, 0x3E8, 0x2E8, -1
};

u_long
IO_Create_Channel(u_long ChannelId)
{
    int i, ttyn;

#ifdef DEBUG
    fprintf (stderr, "IO_Create_Channel: ChannelId == %06x\n", ChannelId);
#endif /* DEBUG */

    if ((ChannelId & 0xffff0000) != 0x10000)
	return IFD_NOT_SUPPORTED;

    ttyn = ChannelId & 0xffff;
    for (i = 0; silly_channels[i] > 0; i++) {
	if (silly_channels[i] == ttyn) {
	    ttyn = i;
	    break;
	}
    }

#ifdef DEBUG
    fprintf (stderr, "IO_Create_Channel: ttyn == %d, flags == %d\n",
	     ttyn, cap_table[0].flags);
#endif /* DEBUG */

    CT_ttyn = todos_scopen(ttyn, cap_table[0].flags, NULL);

#ifdef DEBUG
    fprintf (stderr, "IO_Create_Channel: todos_scopen() returns %d\n", CT_ttyn);
#endif /* DEBUG */

    return (CT_ttyn < 0) ? IFD_COMMUNICATION_ERROR : 0;
}

u_long
IO_Close_Channel()
{
    todos_scclose(CT_ttyn);
    CT_ttyn = -1;
    CT_atrlen = 0;
    return 0;
}

u_long
IFD_Get_Capabilities(u_long Tag, u_char Value[])
{
    u_long r = IFD_ERROR_NOT_SUPPORTED;

    switch (Tag) {
    case 0x201:
	r = 0;
	break;
    case SCTAG_IFD_CARDPRESENT:
	*(u_long *) Value = todos_sccardpresent(CT_ttyn) ? IFD_ICC_PRESENT : IFD_ICC_NOT_PRESENT;
	r = 0;
	break;
    case TAG_IFD_ATR:
	memcpy(Value, CT_atr, CT_atrlen);
	r = 0;
	break;
    case SCTAG_IFD_ATRLEN:
	*(int *) Value = CT_atrlen;
	r = 0;
	break;
    case TAG_OPEN_FLAGS:
	*(int *) Value = cap_table[0].flags;
	r = 0;
	break;
    case TAG_RESET_FLAGS:
	*(int *) Value = cap_table[1].flags;
	r = 0;
	break;
    }

    return r;
}

/* NI:
   set capabilities.
   I take only one of two tags, that is, 0x800 (open flags) and
   0x801 (reset flags).

   input:  tag and the value.
   output: if the tag is one of the two, set it, and return 0.
           otherwise return NOT_SUPPORTED.
*/

u_long
IFD_Set_Capabilities(u_long Tag, u_char Value[])
{
    int i;

    for (i = 0 ; cap_table[i].tag != 0 ; i++ ) {
	if (Tag == cap_table[i].tag) {
	    /* found the tag.  set it. */
	    cap_table[i].flags = (int)*((int *)Value);
#ifdef DEBUG
	    fprintf (stderr, "cap_table[%x].flags = %d\n",
		     Tag, cap_table[i].flags);
#endif DEBUG

	    return 0; 
	}
    }
    
    return IFD_ERROR_NOT_SUPPORTED;
}

u_long
IFD_Set_Protocol_Parameters(u_long ProtocolType, u_char SelectionFlags, u_char PTS1, u_char PTS2, u_char PTS3)
{
    return IFD_ERROR_NOT_SUPPORTED;
}

u_long
IFD_Power_ICC(u_long a)
{
    u_long r;

    if (a == IFD_POWER_UP)
	r = (todos_scdtr(CT_ttyn, 1) < 0) ? IFD_ERROR_POWER_ACTION : 0;
    else if (a == IFD_POWER_DOWN)
	r = (todos_scdtr(CT_ttyn, 0) < 0) ? IFD_ERROR_POWER_ACTION : 0;
    else if (a == IFD_RESET) {
	CT_atrlen = todos_scxreset(CT_ttyn, cap_table[1].flags, CT_atr, NULL);
	r = (CT_atrlen <= 0) ? IFD_ERROR_POWER_ACTION : 0;
    } else
	r = IFD_NOT_SUPPORTED;

    return r;
}

u_long
IFD_Swallow_ICC()
{
    return IFD_ERROR_NOT_SUPPORTED;
}

u_long
IFD_Eject_ICC()
{
    return (todos_scdtr(CT_ttyn, 0) < 0) ? IFD_COMMUNICATION_ERROR : 0;
}

u_long
IFD_Confiscate_ICC()
{
    return IFD_ERROR_NOT_SUPPORTED;
}

u_long
IFD_Transmit_to_ICC(struct SCARD_IO_HEADER SendPci,
		    u_char ibuf[], u_long ilen, u_char obuf[], u_long *olen,
		    struct SCARD_IO_HEADER *RecvPci)
{
    unsigned char buf[255+2];
    int n, p3, sw1, sw2;

#ifdef DEBUG
    printf("p3 %x ilen %x *olen %x\n", ibuf[4], ilen, *olen);
#endif
    ilen -= 5;

    if (ilen > 0) {
	/* "in" data; stupid ifd interface tacks le on the end */
	if (ilen == ibuf[4] + 1) {
	    n = ibuf[5 + --ilen];
#ifdef DEBUG
	    printf("found trailing le %d\n", n);
#endif
	} else if (*olen > 2)
	    n = sizeof buf;
	else
	    n = 0;
    } else
	n = ibuf[4];

    n = todos_scrw(CT_ttyn, ibuf[0], ibuf[1], ibuf[2], ibuf[3], ilen, &ibuf[5], n, buf, &sw1, &sw2);
    if (n < 0)
	return IFD_COMMUNICATION_ERROR;

    if (n)
	memcpy(obuf, buf, n);

    obuf[n+0] = sw1;
    obuf[n+1] = sw2;
    *olen = n + 2;

    return 0;
}

u_long
IFD_Is_ICC_Present()
{
    return (todos_sccardpresent(CT_ttyn) ? IFD_ICC_PRESENT : IFD_ICC_NOT_PRESENT);
}

/*
copyright 2000
the regents of the university of michigan
all rights reserved

permission is granted to use, copy, create derivative works 
and redistribute this software and such derivative works 
for any purpose, so long as the name of the university of 
michigan is not used in any advertising or publicity 
pertaining to the use or distribution of this software 
without specific, written prior authorization.  if the 
above copyright notice or any other identification of the 
university of michigan is included in any copy of any 
portion of this software, then the disclaimer below must 
also be included.

this software is provided as is, without representation 
from the university of michigan as to its fitness for any 
purpose, and without warranty by the university of 
michigan of any kind, either express or implied, including 
without limitation the implied warranties of 
merchantability and fitness for a particular purpose. the 
regents of the university of michigan shall not be liable 
for any damages, including special, indirect, incidental, or 
consequential damages, with respect to any claim arising 
out of or in connection with the use of the software, even 
if it has been or is hereafter advised of the possibility of 
such damages.
*/
