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
     char addr;
     char *name;
     int width;
};

struct reg_t pcic_regs[] = {
     PCIC_ID_REV, "Identification and Revision", 1,
     PCIC_STATUS, "Interface Status", 1,
     PCIC_POWER, "Power and RESETDRV control", 1,
     PCIC_INT_GEN, "Interrupt and General Control", 1,
     PCIC_STAT_CHG, "Card Status Change", 1,
     PCIC_STAT_INT, "Card Status Change Interrupt Config", 1,
     PCIC_ADDRWINE, "Address Window Enable", 1,
     PCIC_IOCTL, "I/O Control", 1,
     PCIC_IO0_STL, "I/O Address 0 Start", 2,
     PCIC_IO0_SPL, "I/O Address 0 Stop", 2,
     PCIC_IO1_STL, "I/O Address 1 Start", 2,
     PCIC_IO1_SPL, "I/O Address 1 Stop", 2,
     PCIC_SM0_STL, "System Memory Address 0 Mapping Start", 2,
     PCIC_SM0_SPL, "System Memory Address 0 Mapping Stop", 2,
     PCIC_CM0_L, "Card Memory Offset Address 0", 2,
     PCIC_SM1_STL, "System Memory Address 1 Mapping Start", 2,
     PCIC_SM1_SPL, "System Memory Address 1 Mapping Stop", 2,
     PCIC_CM1_L, "Card Memory Offset Address 1", 2,
     PCIC_SM2_STL, "System Memory Address 2 Mapping Start", 2,
     PCIC_SM2_SPL, "System Memory Address 2 Mapping Stop", 2,
     PCIC_CM2_L, "Card Memory Offset Address 2", 2,
     PCIC_SM3_STL, "System Memory Address 3 Mapping Start", 2,
     PCIC_SM3_SPL, "System Memory Address 3 Mapping Stop", 2,
     PCIC_CM3_L, "Card Memory Offset Address 3", 2,
     PCIC_SM4_STL, "System Memory Address 4 Mapping Start", 2,
     PCIC_SM4_SPL, "System Memory Address 4 Mapping Stop", 2,
     PCIC_CM4_L, "Card Memory Offset Address 4", 2,
     0, NULL, 0,
};     

#if 0
struct reg_t pcmcia_regs[] = {
     PCMCIA_COR, "Configuration Option Register", 1,
     PCMCIA_CCSR, "Card Configuration Status Register", 1,
     PCMCIA_PIR, "Pin Replacement Register", 1,
     PCMCIA_SCR, "Socket and Copy Register", 1,
     0, NULL, 0,
};
#endif

struct pcmcia_regs data;
struct pcic_regs *pcic = (struct pcic_regs *)&data.chip_data[0];

main(int argc, char **argv)
{
     int i, j, idx;
     int fd;
     char nmbuf[80];

     if (argc < 2)
	 exit(1);

     idx=atoi(argv[1]);

     for(i=0;i<128;i++) {
	pcic->reg[i].addr=i;
     }
     pcic->cnt=128;

     sprintf(nmbuf,"/dev/pcmcia/chip%d",idx/2);

     if((fd=open(nmbuf,O_RDWR))<0) {
	perror("open");
	exit(1);
     }

     if(ioctl(fd,PCMCIAIO_READ_REGS,&data)<0) {
	perror("ioctl");
	exit(1);
     }



      dump_pcic_regs((idx&1)?0x40:0);
      printf("\n");
      /*if (argc == 2)
	   dump_pcmcia_regs(&pcic_socks[i]);/**/

     return 0;
}

dump_pcic_regs(int off)
{
     int i;

     for (i = 0; pcic_regs[i].name; i++) {
	  printf("%s: ", pcic_regs[i].name);
	  switch (pcic_regs[i].width) {
	  case 1:
	       printf("%#x", pcic->reg[pcic_regs[i].addr+off].val);
	       break;
	  case 2:
	       printf("%#x", (pcic->reg[pcic_regs[i].addr+off+1].val<<8)|pcic->reg[pcic_regs[i].addr+off].val);
	       break;
	  }
	  printf("\n");
     }
}
#if 0
dump_pcmcia_regs(struct pcic_sock *sock)
{
     int i;
     u_char v;

     pcic_map_attr(sock, PHYS_ADDR, 1);
     for (i = 0; pcmcia_regs[i].name; i++) {
	  printf("%s: ", pcmcia_regs[i].name);

	  if (lseek(mem_fd, PHYS_ADDR + sock->base_addr +
		    pcmcia_regs[i].addr, SEEK_SET) < 0) {
	       perror("seeking to tuple memory");
	       exit(1);
	  }
	  if (read(mem_fd, &v, 1) < 0) {
	       perror("reading tuple memory");
	       exit(1);
	  }
	  
	  printf("%#x\n", v);
     }

     pcic_map_attr(sock, PHYS_ADDR, 0);
}
#endif
