/* $Id: todos_scrw.h,v 1.4 2001/06/08 15:04:05 rees Exp $ */

/*
copyright 1997, 2001
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

extern unsigned char todos_scinvert[];

/* forward declarations */

int todos_scopen(int ttyn, int flags, int *ep);
int todos_scsetflags(int ttyn, int flags, int mask);
int todos_scsetspeed(int ttyn, int speed);
int todos_scrw(int ttyn, int cla, int ins, int p1, int p2, int ilen, unsigned char *ibuf, int olen, unsigned char *obuf, int *sw1p, int *sw2p);
int todos_sccardpresent(int ttyn);
int todos_scdsr(int ttyn);
int todos_scclose(int ttyn);
int todos_scxreset(int ttyn, int flags, unsigned char *atr, int *ep);
int todos_scdtr(int ttyn, int cmd);
void todos_scdrain(int ttyn);
int todos_get_atr(int ttyn, int flags, unsigned char *atr, struct scparam *param);
