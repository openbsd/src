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
void parse_cfent_dumpinfo(u_char *, int,   int);
void read_extended_speed(u_char *, int *);
const char *tuple_name(int), *dtype_name(int), *dsize_name(int),
     *dspeed_name(int);
void parse_tuples(u_char *buf, int print_tuples);
dbuf(u_char *buf,int len,int verb) {
    int i;
    for(i=0;i<len;i++) {
	if(i%8==0)
	    printf("%04x : ",i);
	printf(" %02x(%c)",buf[i],buf[i]<' '||buf[i]>126?'.':buf[i]);
	if(i%8==7)
	    printf("\n");
    }
    if(i%16!=0)
		printf("\n");
}

main(int argc,char **argv) {
    int fd;
    char namebuf[64];
    int sockid=-1;
    int verb=0;
    struct pcmcia_status stbuf;
    struct pcmcia_info inbuf;
    char *file;

    if(argc<2)  {
	exit(1);
    }

    if(*(argv[1])<'0' || *(argv[1])>'9') {
	file=argv[1];
    } else {
        sockid=atoi(argv[1]);
    }
    if(argc>2) {
	verb=atoi(argv[2]);
    }

    if(sockid>=0) {
	sprintf(namebuf,"/dev/pcmcia/slot%d",sockid);

	if((fd=open(namebuf,O_RDWR))<0) {
	    printf("errno %d\n",errno);
	    perror("open");
	    exit(1);
	}
	printf("open ok\n",stbuf.slot,stbuf.status);
	if(ioctl(fd,PCMCIAIO_GET_STATUS,&stbuf)<0) {
	    printf("errno %d\n",errno);
	    perror("ioctl PCMCIAIO_GET_STATUS");
	    exit(1);
	}
	printf("Status slot  %d %x\n",stbuf.slot,stbuf.status);
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
	if(!(stbuf.status&PCMCIA_READY)) {
	    printf("Card in slot %d not ready\n",stbuf.slot);
	    /*exit(1);/**/
	}
	if(ioctl(fd,PCMCIAIO_GET_INFO,&inbuf)<0) {
	    printf("errno %d\n",errno);
	    perror("ioctl PCMCIAIO_GET_INFO");
	    exit(1);
	}
    } else if(file) {
	fd=open(file,O_RDONLY);
	if(fd==-1) {
	    perror("Can't open file");
	    exit(1);
	}
	if(read(fd,inbuf.cis_data,512)==-1) {
	    perror("Can't read file");
	    exit(1);
	}
    }
    if(verb)
       dbuf(inbuf.cis_data,512,verb);/**/
    parse_tuples(inbuf.cis_data,1);
    exit(0);

}

void parse_tuples(u_char *buf, int print_tuples)
{
     u_char code, len,*tbuf;
     int done; 

     done = 0;
     while (!done) {
	  code=*buf++;
	  if (code == CIS_NULL) {
	       if (print_tuples)
		    printf("NULL tuple\n");
	       continue;
	  }

	  len=*buf++;
	  if(code!=CIS_END)
	      if (print_tuples)
		   printf("%s (%d):\n", tuple_name(code), len);


	  tbuf=buf;
	  buf+=len;
	  switch (code) {
	  case CIS_NULL:
	       if (print_tuples)
		    printf("NULL\n");
	       break;
	  case CIS_END:
	       done = 1;
	       break;
	  case CIS_DEVICE:
	  case CIS_DEVICE_A:
	       if (!print_tuples)
		    break;
	       parse_device(tbuf, len, print_tuples);
	       break;
	  case CIS_VER1:
	       parse_ver1(tbuf, len, print_tuples);
	       break;
	  case CIS_CFG_INFO:
	       parse_config(tbuf, len, print_tuples);
	       break;
	  case CIS_CFG_ENT:
	       parse_cfent_dumpinfo(tbuf, len, print_tuples);
	       break;
	  default:
	       if (print_tuples)
		    printf("\tskpping\n");
	       break;
	  }
     }
}

void parse_device(u_char *tbuf, int tlen, int print_tuples)
{
     int i, idx, addr_units;
     
     i = 0;
     while (i < tlen) {
	  /* last info structure? */
	  if (tbuf[i] == 0xff) {
	    break;
	    }
	  
	  /* device id */
	  idx = (tbuf[i] & CIS_DEVICE_TYPE) >> CIS_DEVICE_TYPE_SHIFT;
	  printf("\tType %s, ", dtype_name(idx));
	  printf("WPS %s, ", tbuf[i] & CIS_DEVICE_WPS ? "set" : "clear");
	  idx = tbuf[i] & CIS_DEVICE_SPEED;
	  printf("Speed %s, ", dspeed_name(idx));
	  
	  /* device size */
	  i++;
	  if (tbuf[i] != 0xff) {
	       addr_units = ((tbuf[i] & CIS_DEVICE_ADDRS) >>
			     CIS_DEVICE_ADDRS_SHIFT) + 1;
	       idx = tbuf[i] & CIS_DEVICE_SIZE;
	       printf("Size %s * %d", dsize_name(idx), addr_units);
	  } else {
	       printf("IGNORED");
	       /* ignore this device info entry */
	  }
	  
	  printf("\n");
	  
	  i++;
     }
}

void parse_ver1(u_char *tbuf, int len, int print_tuples)
{
     int i;
     char manufacturer[33],prod_name[33],addl_info1[33],addl_info2[33];
     
     i = 0;
     if (tbuf[i++] != 0x04) {
	  if (print_tuples)
	       fprintf(stderr, "Major version != 0x04\n");
	  return;
     }
     if (tbuf[i++] != 0x01) {
	  if (print_tuples)
	       fprintf(stderr, "Minor version != 0x01\n");
	  return;
     }
     strncpy(manufacturer, &tbuf[i], sizeof(manufacturer)-1);
     i += strlen(manufacturer) + 1;
     strncpy(prod_name, &tbuf[i], sizeof(prod_name)-1);
     i += strlen(&tbuf[i]) + 1;
     if(tbuf[i]==0xff)
	addl_info1[0]=0;
     else {
	 strncpy(addl_info1, &tbuf[i], sizeof(addl_info1)-1);
	 i += strlen(&tbuf[i]) + 1;
     }
     if(tbuf[i]==0xff)
	addl_info2[0]=0;
     else {
	 strncpy(addl_info2, &tbuf[i], sizeof(addl_info2)-1);
	 i += strlen(&tbuf[i]) + 1;
     }
     if (print_tuples) {
	  printf("\tManufacturer: %s\n", manufacturer);	    
	  printf("\tProduct name: %s\n", prod_name);
	  printf("\tAddl info 1: %s\n", addl_info1);
	  printf("\tAddl info 2: %s\n", addl_info2);
     }
     if (tbuf[i] != 0xff) {
	       fprintf(stderr, "Tuple not ended by 0xff!\n");
	  return;
     }
}

void parse_config(u_char *tbuf, int len,  int print_tuples)
{
     int i, rasz, rmsz,config_midx,base_addr,regmask[4];
     
     i = 0;
     rasz = (tbuf[i] & TPCC_RASZ) >> TPCC_RASZ_SHIFT;
     rmsz = (tbuf[i] & TPCC_RMSZ) >> TPCC_RMSZ_SHIFT;
     if (print_tuples)
	  printf("\tRASZ %d, RMSZ %d, ", rasz+1, rmsz+1);
     
     i++;
     config_midx = (tbuf[i] & TPCC_LAST) >> TPCC_LAST_SHIFT;
     if (print_tuples)
	  printf("last idx %d, ", config_midx);
     
     i++;
     base_addr = 0;
     switch (rasz) {
     case 3:
	  base_addr |= (tbuf[i+3] << 24);
     case 2:
	  base_addr |= (tbuf[i+2] << 16);
     case 1:
	  base_addr |= (tbuf[i+1] << 8);
     case 0:
	  base_addr |= tbuf[i];
     }
     if (print_tuples)
	  printf("base addr 0x%08x\n", base_addr);
     
     i += rasz + 1;
     regmask[0] = regmask[1] = 0;
     regmask[2] = regmask[3] = 0; 
     switch (rmsz) {
     case 15:
	  regmask[3] |= (tbuf[i+15] << 24);
     case 14:
	  regmask[3] |= (tbuf[i+14] << 16);
     case 13:
	  regmask[3] |= (tbuf[i+13] << 8);
     case 12:
	  regmask[3] |= tbuf[i+12];
     case 11:
	  regmask[2] |= (tbuf[i+11] << 24);
     case 10:
	  regmask[2] |= (tbuf[i+10] << 16);
     case 9:
	  regmask[2] |= (tbuf[i+9] << 8);
     case 8:
	  regmask[2] |= tbuf[i+8];
     case 7:
	  regmask[1] |= (tbuf[i+7] << 24);
     case 6:
	  regmask[1] |= (tbuf[i+6] << 16);
     case 5:
	  regmask[1] |= (tbuf[i+5] << 8);
     case 4:
	  regmask[1] |= tbuf[i+4];
     case 3:
	  regmask[0] |= (tbuf[i+3] << 24);
     case 2:
	  regmask[0] |= (tbuf[i+2] << 16);
     case 1:
	  regmask[0] |= (tbuf[i+1] << 8);
     case 0:
	  regmask[0] |= tbuf[i+0];
	  break;
     }
     if (print_tuples)
	  printf("\treg mask 0x%04x%04x%04x%04x, ",
		 regmask[3], regmask[2],
		 regmask[1], regmask[0]); 
     
     i += rmsz + 1;
     if (print_tuples)
	  printf("\n\t%d bytes in subtuples\n", len - i);
}

void parse_cfent_dumpinfo(u_char *tbuf, int len, int print_tuples)
{
     int i, j, k, intface, ftrsm,idx,defp,iop,io_16,ios,ftrs;
     int pwr_desc, wait_scale, rdy_scale, rsv_scale;
     int io_block_len, io_block_size;
     int host_addr_p, addr_size, len_size,elen;

     
     i = 0;
     intface = (tbuf[i] & TPCE_INDX_INT);
     idx = (tbuf[i] & TPCE_INDX_ENTRY);
     defp = (tbuf[i] & TPCE_INDX_DEF);
     if (print_tuples)
	  printf("\tEntry %d, %sdefault, %sinterface\n", idx,
		 defp ? "" : "not ", intface ? "" : "no ");
     if (intface) {
	  i++;
	  if (print_tuples)
	       printf("\ttype %d, BVD %d, WP %d, RdyBsy %d, "
		      "wait sig %d\n",
		      tbuf[i] & TPCE_IF_TYPE,
		      !!tbuf[i] & TPCE_IF_BVD,
		      !!tbuf[i] & TPCE_IF_WP,
		      !!tbuf[i] & TPCE_IF_RDYBSY,
		      !!tbuf[i] & TPCE_IF_MWAIT);
     }
     i++;
     
     ftrs = tbuf[i++];
     if (print_tuples)
	  printf("\tfeatures 0x%02x (%x)\n", ftrs,ftrs&TPCE_FS_PWR);
     
     /* XXX skip all power description structures */
     for (j = 0; j < (ftrs & TPCE_FS_PWR); j++) {
	  pwr_desc = tbuf[i++];
          printf("PWR %x\n",pwr_desc);
	  /* for each struct, skip all parameter defns */
	  for (k = 0; k < 8; pwr_desc >>= 1, k++) {
	       if (pwr_desc & 0x01) {
		    printf("%d: ",k);
		    /* skip bytes until non-ext found */
		    printf("%x ",tbuf[i]);
		    while (tbuf[i++] & 0x80)
			printf("%x ",tbuf[i]);
			 ;/**/
		    printf("\n");
	       }
	  }
     }
     
     if (ftrs & TPCE_FS_TD) {
	  wait_scale = tbuf[i] & TPCE_FS_TD_WAIT;
	  rdy_scale = (tbuf[i] & TPCE_FS_TD_RDY) >>
	       TPCE_FS_TD_RDY_SHIFT;
	  rsv_scale = (tbuf[i] & TPCE_FS_TD_RSV) >>
	       TPCE_FS_TD_RSV_SHIFT;
	  i++;
	  if (wait_scale != 3) {
	       read_extended_speed(tbuf, &i);
	       if (print_tuples)
		    printf("\twait scale %d\n", wait_scale);
	  }
	  if (rdy_scale != 7) {
	       read_extended_speed(tbuf, &i);
	       if (print_tuples)
		    printf("\tReady/Busy scale %d\n", rdy_scale);
	  }
	  if (rsv_scale != 7) {
	       read_extended_speed(tbuf, &i);
	       if (print_tuples)
		    printf("\tReserved scale %d\n", rsv_scale);
	  }
     }
     
     if (ftrs & TPCE_FS_IO) {
	  int io_addrs[16],io_lens[16];
	  int iptr,ilen,ranges;
	  iop = 1;
	  io_16 = tbuf[i] & TPCE_FS_IO_BUS16;
	  if (print_tuples)
	       printf("\tIO lines %x, bus8 %d, bus16 %d, range %d\n",
		      (tbuf[i] & TPCE_FS_IO_LINES),
		      !!(tbuf[i] & TPCE_FS_IO_BUS8),
		      !!io_16,
		      ranges=!!(tbuf[i] & TPCE_FS_IO_RANGE));
	  i++;
	  if(ranges) {
	      io_block_len = (tbuf[i] & TPCE_FS_IO_LEN) >>
		   TPCE_FS_IO_LEN_SHIFT;
	      io_block_size = (tbuf[i] & TPCE_FS_IO_SIZE) >>
		   TPCE_FS_IO_SIZE_SHIFT;
	      ios = (tbuf[i] & TPCE_FS_IO_NUM) + 1;
	      elen=io_block_len+(io_block_len==3?1:0)+
		   io_block_size+(io_block_size==3?1:0);
	      i++;
	    if((ftrs & TPCE_FS_IRQ)!=0) {
	      iptr=(ios*elen)+i;
	      if((tbuf[iptr]&(TPCE_FS_IRQ_PULSE|TPCE_FS_IRQ_LEVEL))==0) {
		if(((tbuf[iptr-elen]) &(TPCE_FS_IRQ_PULSE|TPCE_FS_IRQ_LEVEL))!=0) {
		    iptr-=elen;
		}
	      }
		if((tbuf[iptr]&TPCE_FS_IRQ_MASK)!=0) {
		    ilen=2;
		} else {
		    ilen=1;
		}
	      } else {
		ilen=0;
	      }
	      if((i+(ios*elen)+ilen)>len) {
		  if (print_tuples)
		    printf("Warning: CIS range info doesn't fit in entry!"
			   " Reducing # of ranges by 1\n");
		    printf("%d %d %d %d %d\n",i,ios,ilen,ios*elen,len);
		   ios--;
	      }
	      if (print_tuples)
		   printf("\t# ranges %d, length size %d, "
			  "addr size %d\n", ios,
			  io_block_len, io_block_size); 
	      for (j = 0; j < ios; j++) {
		   io_addrs[j] = io_lens[j] = 0;
		   switch (io_block_size) {
		   case 3:
			io_addrs[j] |= tbuf[i+3] << 24;
			io_addrs[j] |= tbuf[i+2] << 16;
		   case 2:
			io_addrs[j] |= tbuf[i+1] << 8;
		   case 1:
			io_addrs[j] |= tbuf[i];
			break;
		   }
		   i += io_block_size + (io_block_size == 3 ? 1
					 : 0);
		   switch (io_block_len) {
		   case 3:
			io_lens[j] |= tbuf[i+3] << 24;
			io_lens[j] |= tbuf[i+2] << 16;
		   case 2:
			io_lens[j] |= tbuf[i+1] << 8;
		   case 1:
			io_lens[j] |= tbuf[i];
			break;
		   }
		   io_lens[j]++;
		   i += io_block_len + (io_block_len == 3 ? 1
					: 0);
		   
		   if (print_tuples)
		    if(io_lens[j]&1) 
			printf("\taddr %08x, len %d (Assuming incorect CIS entry"
			       " CIS value == %d)\n",
			       io_addrs[j],
			       io_lens[j]-1,
			       io_lens[j]);
		    else
			printf("\taddr %08x, len %d\n",
			       io_addrs[j],
			       io_lens[j]);
	      }
	  }
     }
     
     if (ftrs & TPCE_FS_IRQ) {
	  int irq_mask,irqp,irq;
	  irqp = 1;
	  if (print_tuples)
	       printf("\tIRQ: share %d, pulse %d, level %d, ",
		      !!(tbuf[i] & TPCE_FS_IRQ_SHARE),
		      !!(tbuf[i] & TPCE_FS_IRQ_PULSE),
		      !!(tbuf[i] & TPCE_FS_IRQ_LEVEL));
	  if (tbuf[i] & TPCE_FS_IRQ_MASK) {
	       irq_mask = (tbuf[i+2] << 8) + tbuf[i+1];
	       if (print_tuples)
		    printf("VEND %d, BERR %d, IOCK %d, NMI %d\n"
			   "\t    mask 0x%04x\n",
			   !!(tbuf[i] & TPCE_FS_IRQ_VEND),
			   !!(tbuf[i] & TPCE_FS_IRQ_BERR),
			   !!(tbuf[i] & TPCE_FS_IRQ_IOCK),
			   !!(tbuf[i] & TPCE_FS_IRQ_NMI),
			   irq_mask);
	       i += 2;
	  } else {
	       irq = tbuf[i] & TPCE_FS_IRQ_IRQN;
	       if (print_tuples)
		    printf("IRQ %d\n", irq);
	  }
	  
	  i++;
     }
     
     if (ftrs & TPCE_FS_MEM) {
	  int memp,mems,mem_lens[16],mem_caddrs[16],mem_haddrs[16];
	  memp = 1;
	  switch ((ftrs & TPCE_FS_MEM) >> TPCE_FS_MEM_SHIFT) {
	  case 1:
	       mems = 1;
	       mem_lens[0] = (tbuf[i+1] << 8) + tbuf[i];
	       mem_lens[0] <<= 8;
	       printf("\tmem: len %d\n", mem_lens[0]);
	       
	       break;
	  case 2:
	       mems = 1;
	       mem_lens[0] = (tbuf[i+1] << 8) + tbuf[i];
	       mem_caddrs[0] = mem_haddrs[0] =
		    (tbuf[i+3] << 8) + tbuf[i+2];

	       mem_lens[0] <<= 8;
	       mem_caddrs[0] <<= 8;
	       
	       if (print_tuples)
		    printf("\tmem: len %d, addr %d\n",
			   mem_lens[0],
			   mem_caddrs[0]); 
	       break;
	  case 3:
	       host_addr_p = tbuf[i] & TPCE_FS_MEM_HOST;
	       addr_size = (tbuf[i] & TPCE_FS_MEM_ADDR) >>
		    TPCE_FS_MEM_ADDR_SHIFT;
	       len_size = (tbuf[i] & TPCE_FS_MEM_LEN) >>
		    TPCE_FS_MEM_LEN_SHIFT;
	       mems = (tbuf[i] & TPCE_FS_MEM_WINS) + 1;
	       if (print_tuples)
		    printf("\tmem (%x): host %d, addr size %d, len "
			   "size %d, # wins %d\n", tbuf[i],
			   !!host_addr_p, addr_size,
			   len_size, mems); 
	       i++;
	       for (j = 0; j < mems; j++) {
		    mem_lens[j] = 0;
		    mem_caddrs[j] = 0;
		    mem_haddrs[j] = 0;  
		    switch (len_size) {
		    case 3:
			 mem_lens[j] |= (tbuf[i+2] << 16);
		    case 2:
			 mem_lens[j] |= (tbuf[i+1] << 8);
		    case 1:
			 mem_lens[j] |= tbuf[i];
		    }
		    i += len_size;
		    switch (addr_size) {
		    case 3:
			 mem_caddrs[j] |= (tbuf[i+2] << 16);
		    case 2:
			 mem_caddrs[j] |= (tbuf[i+1] << 8);
		    case 1:
			 mem_caddrs[j] |= tbuf[i];
		    }
		    i += addr_size;
		    if (host_addr_p) {
			 switch (addr_size) {
			 case 3:
			      mem_haddrs[j] |=
				   (tbuf[i+2] << 16); 
			 case 2:
			      mem_haddrs[j] |=
				   (tbuf[i+1] << 8); 
			 case 1:
			      mem_haddrs[j] |=
				   tbuf[i]; 
			 }
			 i += addr_size;
		    }

		    mem_lens[j] <<= 8;
		    mem_caddrs[j] <<= 8;
		    mem_haddrs[j] <<= 8;
		    
		    if (print_tuples)
			 printf("\t\twin %d: len %d, card addr "
				"%x, host addr %x\n", j,
				mem_lens[j],
				mem_caddrs[j],
				mem_haddrs[j]);
	       }
	  }
     }
}

void read_extended_speed(u_char *tbuf, int *i)
{
     *i += 1;
     /* fprintf(stderr, "\tXXX read_extended_speed not implemented!\n"); */
}

const char *tuple_name(int code)
{
#define MAX_TUPLE_NAME (sizeof(tuple_names) / sizeof(char *))
     static const char *tuple_names[] = {
	  "NULL", "DEVICE",
	  "reserved", "reserved", "reserved", "reserved",
	  "reserved", "reserved", "reserved", "reserved",
	  "reserved", "reserved", "reserved", "reserved",
	  "reserved", "reserved",
	  "CHECKSUM", "LONGLINK_A", "LONGLINK_C", "LINKTARGET",
	  "NO_LINK", "VERS_1", "ALTSTR", "DEVICE_A",
	  "JEDEC_C", "JEDEC_A", "CONFIG", "CFTABLE_ENTRY",
	  "DEVICE_OC", "DEVICE_OA",
     };
     if(code==CIS_END)
	return "END";

     return (code < MAX_TUPLE_NAME) ? tuple_names[code] : "UNKNOWN";
}

const char *dtype_name(int idx)
{
#define MAX_DTYPE_NAME (sizeof(dtype_names) / sizeof(char *))
     static const char *dtype_names[] = {
	  "NULL", "ROM", "OTPROM", "EPROM",
	  "EEPROM", "FLASH", "SRAM", "DRAM",
     };

     return (idx < MAX_DTYPE_NAME) ? dtype_names[idx] : "INVALID";
}

const char *dspeed_name(int idx)
{
#define MAX_DSPEED_NAME (sizeof(dspeed_names) / sizeof(char *))
     static const char *dspeed_names[] = {
	  "NULL", "250NS", "200NS", "150NS",
	  "100NS", "reserved", "reserved", "extended",
     };

     return (idx < MAX_DSPEED_NAME) ? dspeed_names[idx] : "INVALID";
}

const char *dsize_name(int idx)
{
#define MAX_DSIZE_NAME (sizeof(dsize_names) / sizeof(char *))
     static const char *dsize_names[] = {
	  "512b", "2k", "8k", "32k", "128k", "512k", "2M", "reserved",
     };
     return (idx < MAX_DSIZE_NAME) ? dsize_names[idx] : "INVALID";
}
