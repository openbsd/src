/*
 *    Copyright (c) 1999 Olaf Flebbe o.flebbe@gmx.de
 *    
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#include <string.h>

int getgid() {return 0;}
int getegid() {return 0;}
int geteuid() {return 0;}
int getuid() {return 0;}
int setgid() {return -1;}
int setuid() {return -1;}


int Perl_my_popen( int a, int b) {
	 return NULL;
}
int Perl_my_pclose( int a) {
	 return NULL;
}

int kill() {return -1;}
signal() { }

int execv() { return -1;}
int execvp() { return -1;}

void Perl_do_exec() {}

/*------------------------------------------------------------------*/
/* Two dummy functions implement getproto*                          */
/*------------------------------------------------------------------*/
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>


static struct protoent protos[2] = {
  {"tcp",  NULL, IPPROTO_TCP} ,
  {"udp",  NULL, IPPROTO_UDP}};

struct protoent *getprotobyname (const char *st) {
    
  if (!strcmp( st, "tcp")) { 
    return &protos[0];
  }
  if (!strcmp( st, "udp")) { 
    return &protos[1];
  }
  return NULL;
}

struct protoent *getprotobynumber ( int i) {
  if (i == IPPROTO_TCP) {
     return &protos[0];
  }
  if (i == IPPROTO_UDP) {
    return &protos[1];
  }
  return NULL;
}


