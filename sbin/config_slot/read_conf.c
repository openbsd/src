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
/* With 'help' of Barry Jaspan's code */

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/device.h>

#include <dev/pcmcia/pcmcia.h>
#include <dev/pcmcia/pcmciabus.h>
#include <dev/pcmcia/pcmcia_ioctl.h>

read_conf(u_char *buf, int blen,int cfidx,struct pcmcia_conf *pc_cf)
{
     u_char code, len,*tbuf,*endp=buf+blen;
     int done; 

     done = 0;
     while (!done && buf<endp) {
	  code=*buf++;
	  if (code == CIS_NULL) {
	       continue;
	  }

	  len=*buf++;

	  tbuf=buf;
	  buf+=len;
	  switch (code) {
	  case CIS_END:
	       done = 1;
	       break;
	  case CIS_CFG_INFO:
	       read_cfg_info(tbuf, len, pc_cf);
	       break;
	  case CIS_CFG_ENT:
	       parse_cfent(tbuf, len,cfidx, pc_cf);
	       break;
	  default:
	       break;
	  }
     }
     return 0;
}

read_cfg_info(u_char *tbuf, int len,  struct pcmcia_conf *pc_cf)
{
     int i, rasz, rmsz;
     
     i = 0;
     rasz = (tbuf[i] & TPCC_RASZ) >> TPCC_RASZ_SHIFT;
     rmsz = (tbuf[i] & TPCC_RMSZ) >> TPCC_RMSZ_SHIFT;
     
     i+=2;
     pc_cf->cfg_off = 0;
     switch (rasz) {
     case 3:
	  pc_cf->cfg_off |= (tbuf[i+3] << 24);
     case 2:
	  pc_cf->cfg_off |= (tbuf[i+2] << 16);
     case 1:
	  pc_cf->cfg_off |= (tbuf[i+1] << 8);
     case 0:
	  pc_cf->cfg_off |= tbuf[i];
     }
     i+=rasz;
     pc_cf->cfg_regmask = 0;
     switch (rmsz) {
     default:
     case 3:
	  pc_cf->cfg_regmask |= (tbuf[i+3] << 24);
     case 2:
	  pc_cf->cfg_regmask |= (tbuf[i+2] << 16);
     case 1:
	  pc_cf->cfg_regmask |= (tbuf[i+1] << 8);
     case 0:
	  pc_cf->cfg_regmask |= tbuf[i];
    }

}

parse_cfent(u_char *tbuf, int len,int slotid,  struct pcmcia_conf *pc_cf)
{
     int i, idx,defp,iop,io_16,ios,ftrs,intface,k;
     int host_addr_p, addr_size, len_size;
     
     i = 0;
     intface = (tbuf[i] & TPCE_INDX_INT);
     idx = (tbuf[i] & TPCE_INDX_ENTRY);
     defp = (tbuf[i] & TPCE_INDX_DEF);
     printf("%x %x\n",idx,slotid);
     if((idx==slotid) ||(defp && slotid==-1)) {
	 int j;
	 printf("** %x %x\n",idx,slotid);
	 if (intface) {
	    i++;
	    pc_cf->iocard=(tbuf[i] & TPCE_IF_TYPE)==1;
	 }

	 i++;
	 ftrs = tbuf[i++];
	 for (j = 0; j < (ftrs & TPCE_FS_PWR); j++) {
	      int pwr_desc = tbuf[i++];
	      /* for each struct, skip all parameter defns */
	      for (k = 0; k < 8; pwr_desc >>= 1, k++) {
		   if (pwr_desc & 0x01) {
			/* skip bytes until non-ext found */
			while (tbuf[i++] & 0x80)
			     ;
		   }
	      }
	 }
	 
	 if (ftrs & TPCE_FS_TD) {
	      i++;
	 }
	 
	 if (ftrs & TPCE_FS_IO) {
	      int io_addrs[16],io_lens[16];
	      int io_16,io_block_len, io_block_size;
	      iop = 1;
	      io_16 = tbuf[i] & TPCE_FS_IO_BUS16;
	      i++;
	      ios--; /*TMPFIX*/;
	      io_block_len = (tbuf[i] & TPCE_FS_IO_LEN) >>
		   TPCE_FS_IO_LEN_SHIFT;
	      io_block_size = (tbuf[i] & TPCE_FS_IO_SIZE) >>
		   TPCE_FS_IO_SIZE_SHIFT;
	      ios = (tbuf[i] & TPCE_FS_IO_NUM) + 1;
	      i++;
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
		   pc_cf->io[j].start=io_addrs[j];
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
		   /* io_lens[j]++; /*TMPFIX*/;
		   pc_cf->io[j].len=io_lens[j];
		   pc_cf->io[j].flags=io_16?PCMCIA_MAP_16:PCMCIA_MAP_8;
		   i += io_block_len + (io_block_len == 3 ? 1
					: 0);
		   
	      }
	      pc_cf->iowin=ios;
	 }
	 
	 if (ftrs & TPCE_FS_IRQ) {
	      int irq_mask,irqp,irq;
	      pc_cf->irq_level=!!(tbuf[i] & TPCE_FS_IRQ_LEVEL);
	      pc_cf->irq_pulse=!!(tbuf[i] & TPCE_FS_IRQ_PULSE);
	      pc_cf->irq_share=!!(tbuf[i] & TPCE_FS_IRQ_SHARE);
	      if (tbuf[i] & TPCE_FS_IRQ_MASK) {
		   irq_mask = (tbuf[i+2] << 8) + tbuf[i+1];
		   i += 2;
	      } else {
		   pc_cf->irq_num = tbuf[i] & TPCE_FS_IRQ_IRQN;
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
		   
		   break;
	      case 3:
		   host_addr_p = tbuf[i] & TPCE_FS_MEM_HOST;
		   addr_size = (tbuf[i] & TPCE_FS_MEM_ADDR) >>
			TPCE_FS_MEM_ADDR_SHIFT;
		   len_size = (tbuf[i] & TPCE_FS_MEM_LEN) >>
			TPCE_FS_MEM_LEN_SHIFT;
		   mems = (tbuf[i] & TPCE_FS_MEM_WINS) + 1;
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
			
		   }
	      }
	 }
     }
}




