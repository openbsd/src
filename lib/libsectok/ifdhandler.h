/* $Id: ifdhandler.h,v 1.3 2001/07/02 20:07:08 rees Exp $ */

/*
copyright 2001
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

/*
 * pcsc cruft
 */

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

/* Extra tags for things they forgot to put in the ifd interface */
#define SCTAG_IFD_ATRLEN 0x6601
#define SCTAG_IFD_CARDPRESENT 0x301
#define SCTAG_OPEN_FLAGS  0x800
#define SCTAG_RESET_FLAGS 0x801

struct SCARD_IO_HEADER {
    u_long Protocol, Length;
};

/* IFD interface */
u_long IO_Create_Channel(u_long ChannelId);
u_long IO_Close_Channel();
u_long IFD_Get_Capabilities(u_long Tag, u_char Value[]);
u_long IFD_Set_Capabilities(u_long Tag, u_char Value[]);
u_long IFD_Set_Protocol_Parameters(u_long ProtocolType, u_char SelectionFlags, u_char PTS1, u_char PTS2, u_char PTS3);
u_long IFD_Power_ICC(u_long a);
u_long IFD_Swallow_ICC();
u_long IFD_Eject_ICC();
u_long IFD_Confiscate_ICC();
u_long IFD_Transmit_to_ICC(struct SCARD_IO_HEADER SendPci,
		    u_char ibuf[], u_long ilen, u_char obuf[], u_long *olen,
		    struct SCARD_IO_HEADER *RecvPci);
u_long IFD_Is_ICC_Present();
