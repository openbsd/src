/* $Id: sc.h,v 1.10 2001/10/02 16:22:40 rees Exp $ */

/*
 * Smartcard commander.
 * Written by Jim Rees and others at University of Michigan.
 */

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

extern int port, fd, cla, aut0_vfyd;
extern FILE *cmdf;

extern struct dispatchtable {
    char *cmd, *help;
    int (*action) (int ac, char *av[]);
} dispatch_table[];

int dispatch(int ac, char *av[]);
int help(int ac, char *av[]);
int reset(int ac, char *av[]);
int dclose(int ac, char *av[]);
int quit(int ac, char *av[]);
int apdu(int ac, char *av[]);
int selfid(int ac, char *av[]);
int isearch(int ac, char *av[]);
int csearch(int ac, char *av[]);
int class(int ac, char *av[]);
int dread(int ac, char *av[]);
int dwrite(int ac, char *av[]);
int challenge(int ac, char *av[]);
int ls(int ac, char *av[]);
int acl(int ac, char *av[]);
int jcreate(int ac, char *av[]);
int jdelete(int ac, char *av[]);
int jdefault(int ac, char *av[]);
int jatr(int ac, char *av[]);
int jdata(int ac, char *av[]);
int jlogin(int ac, char *av[]);
int jaut(int ac, char *av[]);
int jload(int ac, char *av[]);
int junload(int ac, char *av[]);
int jsetpass(int ac, char *av[]);
