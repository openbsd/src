static char *rcsid = "$Id: cmdtab.c,v 1.1 2001/05/22 15:35:58 rees Exp $";

#ifdef __palmos__
#include <Common.h>
#include <System/SysAll.h>
#include <System/Unix/unix_stdio.h>
#include <System/Unix/unix_stdlib.h>
#include <System/Unix/sys_types.h>
#include <string.h>
#undef sprintf
#undef vsprintf
#else
#include <stdio.h>
#endif

struct cmd {
    int ins, inout;
    char *name;
} cmdtab[] = {
    /* 7816-4 */
    0x0e, 0, "erase binary",
    0x20, 0, "verify",
    0x70, 0, "manage channel",
    0x82, 0, "ext auth",
    0x84, 1, "get challenge",
    0x88, 0, "int auth",
    0xa4, 0, "select",
    0xb0, 1, "read binary",
    0xb2, 1, "read record",
    0xc0, 1, "get response",
    0xc2, 0, "envelope",
    0xca, 0, "get data",
    0xd0, 0, "write binary",
    0xd2, 0, "write record",
    0xd6, 0, "update binary",
    0xda, 0, "put data",
    0xdc, 0, "update record",
    0xe2, 0, "append record",
    /* Webcard */
    0xfe, 0, "ip7816",
    /* Cyberflex Access */
    0x04, 0, "invalidate",
    0x08, 0, "manage instance",
    0x0a, 0, "manage program",
    0x0c, 0, "execute method",
    0x22, 0, "logout all",
    0x24, 0, "change PIN",
    0x2a, 0, "verify key",
    0x2c, 0, "unblock",
    0x44, 0, "rehabilitate",
    0xa8, 1, "directory",
    0xe0, 0, "create",
    0xe4, 0, "delete",
    0xfa, 0, "change java atr",
    0xfc, 0, "change acl",
/*    0xfe, 1, "get acl",*/
    /* GSM */
    0x26, 0, "disable PIN",
    0x28, 0, "enable PIN",
    0x30, 0, "decrease",
    0x32, 0, "increase",
    0xf2, 1, "get status",
    /* Visa cash / open platform */
    0x50, 0, "init update",
    0x80, 0, "install default app",
#ifdef PAYFLEX
    /* Payflex */
    0x52, 0, "credit",
    0x54, 0, "debit",
    0x56, 0, "replace debit",
    0x58, 0, "token debit",
    0x5a, 0, "token purchase",
    0x5c, 0, "update currency",
    0x8a, 0, "cert credit",
    0x8c, 0, "cert debit",
    0x8e, 0, "generate diversified key",
    0xd8, 0, "load key",
    0xde, 0, "update max amount",
    0xf4, 0, "load exe",
#endif /* PAYFLEX */
    0, 0, NULL
};

struct cmd *
lookup_cmd(int ins)
{
    struct cmd *p;
    static struct cmd dummy;
    static char name[32];

    for (p = &cmdtab[0]; p->name; p++)
	if (p->ins == ins)
	    break;

    if (!p->name) {
	dummy.ins = ins;
	dummy.inout = 2;
	sprintf(name, "unknown ins %02x", ins);
	dummy.name = name;
	p = &dummy;
    }

    return p;
}

char *
lookup_cmdname(int ins)
{
    return lookup_cmd(ins)->name;
}

/*
copyright 1999
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
