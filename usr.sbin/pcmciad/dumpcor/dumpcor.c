#include <stdio.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#include <dev/ic/i82365reg.h>
#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmcia_ioctl.h>


struct reg_t {
     int addr;
     char *name;
     int width;
};

struct reg_t cor_regs[] = {
     {PCMCIA_COR , "Configuration and Option Register", 1,},
     {PCMCIA_CCSR, "Card Configuration and Status Register", 1,},
     {PCMCIA_PIR, "Pin Replacement Register", 1,},
     {PCMCIA_SCR,"Socket and Copy Register", 1,},
     {0, NULL, 0,}
};     

struct pcmcia_info data;

main(int argc, char **argv)
{
     int i, j, idx;
     int fd;
     char nmbuf[80];

     if (argc < 2)
	 exit(1);
     idx=atoi(argv[1]);

     sprintf(nmbuf,"/dev/pcmcia/slot%d",idx);

     if((fd=open(nmbuf,O_RDWR))<0) {
	perror("open");
	exit(1);
     }

     if(ioctl(fd,PCMCIAIO_READ_COR,&data)<0) {
	perror("ioctl");
	exit(1);
     }

      dump_cor_regs(&data.cis_data);
      printf("\n");
     return 0;
}

dump_cor_regs(unsigned char *data)
{
     int i;

     for(;;) {
	    unsigned int idx=*data++;
	    unsigned int val=*data++;
	  if(idx==0xff)
	    return;
	  if(idx<4)
	      printf("%s: 0x%x\n", cor_regs[idx].name,val);
	  else
	      printf("unkown 0x%x: %x\n", idx,val);
     }
}
