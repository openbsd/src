/*	$OpenBSD: config_slot.c,v 1.2 1996/06/23 14:30:02 deraadt Exp $	*/

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
 */
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/device.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmcia_ioctl.h>

void parse_device(u_char *, int,  int);
void parse_ver1(u_char *, int,  int);
void parse_config(u_char *, int,  int);
void read_extended_speed(u_char *, int *);
const char *tuple_name(int), *dtype_name(int), *dsize_name(int),
     *dspeed_name(int);
void parse_tuples(u_char *buf, int print_tuples);
int dumpcf(struct pcmcia_conf *pc_cf);
void usage(void);
int readiowin(char **argv,struct iowin *io);
void dcp(char **argv,char *target);
void parse_cmdline(int argc,char **argv,struct pcmcia_conf *pc_cf);

int iowins=0;

dumpcf(struct pcmcia_conf *pc_cf) {
    int i;
    static char *ios[]= {
    	"auto","8bit","16bit","illegal"
    };
    printf("Driver name '%s'\n",pc_cf->driver_name);
    printf("CFG offset %x\n",pc_cf->cfg_off );
    printf("IRQ type %s\n",pc_cf->irq_level?"Level":pc_cf->irq_pulse?"Pulse":"None");
    printf("CFG Entry %x %s\n",pc_cf->cfgtype,pc_cf->cfgtype&DOSRESET?"dosreset":"");
    printf("IRQ num %x\n",pc_cf->irq_num&0xf);
    printf("Cardtype %s\n",pc_cf->iocard?"IO":"MEM");
    for (i=0;i<pc_cf->iowin;i++) {
	printf("iowin  %x-%x %s\n",pc_cf->io[i].start,
    	    	pc_cf->io[i].start+pc_cf->io[i].len-1,
    	    	ios[(pc_cf->io[i].flags&(PCMCIA_MAP_8|PCMCIA_MAP_16))>>8]);
   }
    for (i=0;i<pc_cf->memwin;i++) {
	printf("memwin  (%x)%x-%x %x\n",
		pc_cf->mem[i].caddr,
		pc_cf->mem[i].start,
    	    	pc_cf->mem[i].start+pc_cf->mem[i].len-1,
    	    	pc_cf->mem[i].flags);
   }
}

void
usage(void) {
    fprintf(stderr,"usage: config_slot <slotid> [driver name][iocard]\\\n");
    fprintf(stderr,"                   [irq num][lirq][iowin start len width]\n");
    fprintf(stderr,"                   [** not yet memwin start offs len width]\n");
}


main(int argc,char **argv) {
    char namebuf[64];
    struct pcmcia_status stbuf;
    struct pcmcia_info inbuf;
    struct pcmcia_conf pc_cf;
    char manu[MAX_CIS_NAMELEN];
    char model[MAX_CIS_NAMELEN];
    char addinf1[MAX_CIS_NAMELEN];
    char addinf2[MAX_CIS_NAMELEN];
    int sockid;
    int fd,cfg;

    if(argc<2 || !isdigit(argv[1][0])) {
	usage();
    	exit(1);
    }

    sockid=atoi(argv[1]);

    bzero(pc_cf,sizeof(pc_cf));

    argc-=2;argv+=2;
    sprintf(namebuf,"/dev/pcmcia/slot%d",sockid);

    if((fd=open(namebuf,O_RDWR))<0) {
	printf("errno %d\n",errno);
	perror("open");
	exit(1);
    }
    if(ioctl(fd,PCMCIAIO_GET_STATUS,&stbuf)<0) {
	printf("errno %d\n",errno);
	perror("ioctl PCMCIAIO_GET_STATUS");
	exit(1);
    }
    if(!(stbuf.status&PCMCIA_CARD_PRESENT)) {
	printf("No card in slot %d\n",stbuf.slot);
	exit(1);
    }
    if(!(stbuf.status&PCMCIA_POWER)) {
	int pw=PCMCIA_POWER_5V;
	printf("Card in slot %d no power\n",stbuf.slot);
	if(ioctl(fd,PCMCIAIO_SET_POWER,&pw)<0) {
	    printf("errno %d\n",errno);
	    perror("ioctl PCMCIAIO_SET_POWER");
	    exit(1);
	}
	/*exit(1);/**/
    }
    if(ioctl(fd,PCMCIAIO_GET_STATUS,&stbuf)<0) {
	printf("errno %d\n",errno);
	perror("ioctl PCMCIAIO_GET_STATUS");
	exit(1);
    }
    printf("Status slot  %d %x\n",stbuf.slot,stbuf.status);
    if(!(stbuf.status&PCMCIA_READY)) {
	printf("Card in slot %d not ready\n",stbuf.slot);
	/*exit(1);/**/
    }
    if(ioctl(fd,PCMCIAIO_GET_INFO,&inbuf)<0) {
        printf("errno %d\n",errno);
        perror("ioctl PCMCIAIO_GET_INFO");
        exit(1);
    }
    /*dbuf(inbuf.cis_data,512);/**/
    bzero(&pc_cf,sizeof(pc_cf));
    /*parse_cmdline(argc,argv,&pc_cf);/**/
    printf("* %x\n",pc_cf.cfgtype);
    cfg=pc_cf.cfgtype&CFGENTRYMASK;
    if(pcmcia_get_cf(0, inbuf.cis_data,512,CFGENTRYMASK,&pc_cf)) {
        fprintf(stderr,"read_conf failed\n");
        exit(1);
    }
    if(pc_cf.cfgtype&CFGENTRYID) {
	if(pcmcia_get_cf(0, inbuf.cis_data,512,cfg,&pc_cf)) {
	    fprintf(stderr,"read_conf failed\n");
	    exit(1);
	}
    }
    parse_cmdline(argc,argv,&pc_cf);
    if(iowins && pc_cf.iowin!=iowins) {
	pc_cf.iowin=iowins;
    }
    dumpcf(&pc_cf);
    if (pcmcia_get_cisver1(0, (u_char *)&inbuf.cis_data, 512,
			   manu, model, addinf1, addinf2) == 0) {
	printf(" <%s, %s", manu, model);
	if (addinf1[0])
	    printf(", %s", addinf1);
	if (addinf2[0])
		printf(", %s", addinf2);
	printf(">\n");
    }
    printf("PCMCIAIO_CONFIGURE==%x\n",PCMCIAIO_CONFIGURE);
    if(ioctl(fd,PCMCIAIO_CONFIGURE,&pc_cf)<0) {
        printf("errno %d\n",errno);
        perror("ioctl PCMCIAIO_CONFIGURE");
        exit(1);
    }

    exit(0);

}
#define OPT_ARG(opt,arg,func) if(argc>1 && !strcmp(argv[0],opt)) { \
			    arg=func(argv[1]); argv+=2;argc-=2 ;continue;}
#define OPT(opt,arg) if(!strcmp(argv[0],opt)) { \
			    arg=1; argv++;argc-- ;continue;}
#define OPTOV_ARG(opt,op,func,arg) if(!strcmp(argv[0],opt)) { \
			    arg op func(argv[1]); argv+=2;argc-=2 ;continue;}
#define OPTOV(opt,op,val,arg) if(!strcmp(argv[0],opt)) { \
			    arg op val; argv++;argc-- ;continue;}
#define OPT_ARGN(opt,n,arg,func) if(argc>n && !strcmp(argv[0],opt)) { \
			    func(&argv[1],&(arg)); argv+=n+1;argc-=n+1 ;continue;}
readiowin(char **argv,struct iowin *io) {
    io->start=strtol(argv[0],NULL,0);
    io->len=strtol(argv[1],NULL,0);
    if(!strcmp(argv[2],"auto")) {
	io->flags=0;
    } else if(!strcmp(argv[2],"8bit")) {
	io->flags=PCMCIA_MAP_8;
    } else if(!strcmp(argv[2],"16bit")) {
	io->flags=PCMCIA_MAP_16;
    }
}

void
dcp(char **argv,char *target) {
    strcpy(target,argv[0]);
}

void
parse_cmdline(int argc,char **argv,struct pcmcia_conf *pc_cf) {
    int memwin=0;
    while(argc>0) {
	OPT_ARG("irq", pc_cf->irq_num,atoi);
	OPT_ARGN("iowin",3,pc_cf->io[iowins++],readiowin);
	/*OPT_ARGN("-memwin",4,pc_cf->mem,readmem,win);/**/
	OPT("pirq", pc_cf->irq_pulse);
	OPT("lirq", pc_cf->irq_level);
	OPT("iocard", pc_cf->iocard);
	OPTOV("dosreset", |= ,DOSRESET,pc_cf->cfgtype);
	OPTOV_ARG("configid",|= CFGENTRYID |,atoi,pc_cf->cfgtype);
	OPT_ARGN("driver",1, pc_cf->driver_name[0][0],dcp);
	printf("illegal option '%s'\n",argv[0]);
	argc--;argv++;
    }
}
