/*	$OpenBSD: pcmcia_cntrl.c,v 1.2 1996/06/23 14:32:00 deraadt Exp $	*/

/*
 * Copyright (c) 1993, 1994 Stefan Grefen.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following dipclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Stefan Grefen.
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
 *
 */

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/device.h>

#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmcia_ioctl.h>

#define HAS_POWER(a) (!!(a&(PCMCIA_POWER)))

void
usage(void) {
    fprintf(stderr,"usage: pcmcia_cntrl [-f fd] <slotid> on|off|unmap|unconf|probe\n");
}

main(int argc,char **argv) {
    int fd = -1;
    char namebuf[64];
    int sockid;
    struct pcmcia_status stbuf;
    struct pcmcia_info inbuf;
    int onoff=-1;
    int unmap=-1;
    int unconf=-1;
    int probe=-1;
    int force=-1;

    if (argv[1] && strcmp(argv[1], "-f") == 0 &&
	argv[2]) {
	fd = atoi(argv[2]);
	argv += 2;
	argc -= 2;
    }

    if(argc!=3 || !isdigit(argv[1][0])) {
	usage();
	exit(1);
    }
    sockid=atoi(argv[1]);


    if(!strcmp(argv[2],"on")) {
	onoff=1;
    } else if(!strcmp(argv[2],"off")) {
	onoff=0;
    } else if(!strcmp(argv[2],"unconf")) {
	unmap=1;
	unconf=1;
    } else if(!strcmp(argv[2],"unconfforce")) {
	unmap=1;
	unconf=1;
	force=1;
    } else if(!strcmp(argv[2],"unmap")) {
	unmap=1;
    } else if(!strcmp(argv[2],"probe")) {
	probe=1;
    } else {
	usage();
	exit(1);
    }

    sprintf(namebuf,"/dev/pcmcia/slot%d",sockid);

    if (fd == -1 && (fd=open(namebuf,O_RDWR))<0) {
	perror("open");
	exit(1);
    }
    if(ioctl(fd,PCMCIAIO_GET_STATUS,&stbuf)<0) {
	perror("ioctl PCMCIAIO_GET_STATUS");
	exit(1);
    }
    if(!(stbuf.status&PCMCIA_CARD_PRESENT) && force < 0) {
	fprintf(stderr,"No card in slot %d\n",stbuf.slot);
	exit(1);
    }
    if(onoff>=0) {
	if(stbuf.status&PCMCIA_CARD_IS_MAPPED) {
	    fprintf(stderr,"Card in slot %d is mapped, can't turn it %s\n",stbuf.slot,onoff?"on":"off");
	    exit(1);
	}
	if((HAS_POWER(stbuf.status&PCMCIA_POWER))^(!!onoff)) {
	    int pw=onoff?PCMCIASIO_POWER_5V:PCMCIASIO_POWER_OFF;
	    printf("Card in slot %d power %s\n",stbuf.slot,onoff?"on":"off" );
	    if(ioctl(fd,PCMCIAIO_SET_POWER,&pw)<0) {
		printf("errno %d\n",errno);
		perror("ioctl PCMCIAIO_SET_POWER");
		exit(1);
	    }
	    /*exit(1);/**/
	}
    }
    if (unconf >= 0) {
	    int o;
	    if(!(stbuf.status&PCMCIA_CARD_IS_MAPPED)) {
		fprintf(stderr,"Card in slot %d is not mapped\n",stbuf.slot);
		exit(1);
	    }
	    if(ioctl(fd,PCMCIAIO_UNCONFIGURE,0)<0) {
		perror("ioctl PCMCIAIO_UNCONFIGURE");
		exit(1);
	    }
    }
    if (unmap >= 0) {
	if (!unconf && (stbuf.status&PCMCIA_CARD_INUSE)) {
	    fprintf(stderr, "Card in slot %d is configured--unconfigure before unmapping.\n", stbuf.slot);
	    exit(1);
	}
	    if(!(stbuf.status&PCMCIA_CARD_IS_MAPPED)) {
		fprintf(stderr,"Card in slot %d is not mapped\n",stbuf.slot);
		exit(1);
	    }
	    if(ioctl(fd,PCMCIAIO_UNMAP,0)<0) {
		perror("ioctl PCMCIAIO_UNMAP");
		exit(1);
	    }
    }
    if(probe>=0) {
	struct pcmcia_conf pc_cf;
	if(stbuf.status&PCMCIA_CARD_IS_MAPPED) {
	    fprintf(stderr,"Card in slot %d is mapped, can't probe it\n",
		stbuf.slot);
	    exit(1);
	}
	bzero(pc_cf,sizeof(pc_cf)); 
	if(ioctl(fd,PCMCIAIO_CONFIGURE,&pc_cf)<0) {
	    perror("ioctl PCMCIAIO_CONFIGURE");
	    exit(1);
	}
    }
    exit(0);
}

