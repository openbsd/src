/*
 *
 * Copyright (c) 1995 Charles D. Cranor and Seth Widoff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor
 *	and Seth Widoff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * main driver, plus machdep stuff
 */

#include "sboot.h"

void sboot() 

{
  char * mesg;
  char buf[128];
  buf[0] = '0';
  consinit();
  printf("\nsboot: serial line bootstrap program (&end = %x)\n\n", &end);
  if (reboot) {  /* global flag from AAstart.s */
    reboot = 0;
    printf("[rebooting...]\n");
    do_cmd("b");
  }
  while (1) {
    printf(">>> ");
    ngets(buf, sizeof(buf));
    do_cmd(buf);
  }
  /* not reached */
}

/*
 * exit to rom
 */

void callrom () 
{
  asm("trap #15; .word 0x0063");
}

/*
 * do_cmd: do a command
 */

void do_cmd(buf)

char *buf;

{
  switch (*buf) {
  case '\0':
    return;
  case 'a':
    if ( rev_arp() ) {
	printf ("My ip address is: %d.%d.%d.%d\n", myip[0],
		myip[1], myip[2], myip[3]);
	printf ("Server ip address is: %d.%d.%d.%d\n", servip[0],
		servip[1], servip[2], servip[3]);
    } else  {
	printf ("Failed.\n");
    } 
    return;
  case 'e':
    printf("exiting to ROM\n");
    callrom();
    return; 
  case 'f':
    if (do_get_file() == 1) {
      printf("Download Failed\n");
    } else {
      printf("Download was a success!\n");
    }
    return;
  case 'b':
    le_init();
    if ( rev_arp() ) {
	printf ("My ip address is: %d.%d.%d.%d\n", myip[0],
		myip[1], myip[2], myip[3]);
	printf ("Server ip address is: %d.%d.%d.%d\n", servip[0],
		servip[1], servip[2], servip[3]);
    } else  {
	printf ("REVARP: Failed.\n");
	return;
    }
    if (do_get_file() == 1) {
        printf("Download Failed\n");
        return;
    } else {
        printf("Download was a success!\n");
    }
    go(buf);
    return;
  case 'h':
  case '?':
    printf("valid commands\n");
    printf("a - send a RARP\n");
    printf("b - boot the system\n");
    printf("e - exit to ROM\n");
    printf("f - ftp the boot file\n");
    printf("g - execute the boot file\n");
    printf("h - help\n");
    printf("i - init LANCE enet chip\n");
    return;
  case 'i':
    le_init();
    return;
  case 'g':
    go(buf);
    return;
  default:
    printf("sboot: %s: Unknown command\n", buf);
  }
}

go(buf) 
char *buf;

{
  void (*entry)() =  (void (*)) LOAD_ADDR ;
  printf("Start @ 0x%x ... \n", entry);
  (*entry)(buf);
}

