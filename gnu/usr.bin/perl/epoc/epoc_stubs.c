/*
 *    Copyright (c) 1999 Olaf Flebbe o.flebbe@gmx.de
 *    
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

int setgid() {return -1;}
int setuid() {return -1;}

int execv() { return -1;}
int execvp() { return -1;}

void Perl_do_exec() {}



