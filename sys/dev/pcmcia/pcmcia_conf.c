/*	$OpenBSD: pcmcia_conf.c,v 1.7 1998/08/13 06:39:22 deraadt Exp $	*/
/*
 * Copyright (c) 1996 John T. Kohl.  All rights reserved.
 * Copyright (c) 1994 Stefan Grefen.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
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

/*
 * This file is shared between user and kernel space, so be careful with the
 * coding conventions.
 */
#ifdef _KERNEL
#include <sys/errno.h>
#else
#include <errno.h>
#endif
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/time.h>
#include <sys/systm.h>

#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmcia_ioctl.h>

#ifdef CFG_DEBUG
static void dump __P((u_char *, int));

static
void dump(addr, len)
	u_char *addr;
	int len;
{
	int i;

	for (i = 0; i < len; i++) {
		printf("%02x ", addr[i]);
		if (i != 0 && (i & 0xf) == 0)
			printf("\n");
	}
	if (i != 0 && (i & 0xf) == 0)
		printf("\n");
}
#endif

int
pcmcia_get_cf(pc_link, data, dlen, idx, pc_cf)
	struct pcmcia_link *pc_link;
	u_char         *data;
	int             dlen, idx;
	struct pcmcia_conf *pc_cf;
{
	u_char          code, len, *tbuf, *endp;
	int             done;

	endp = data + dlen;

	done = 0;
	while (!done && data < endp) {
		code = *data++;
		if (code == CIS_NULL) {
			continue;
		}
		len = *data++;

		tbuf = data;
		data += len;
		switch (code) {
		case CIS_END:
			done = 1;
			break;
		case CIS_CFG_INFO:
			read_cfg_info(tbuf, len, pc_cf);
			break;
		case CIS_CFG_ENT:
			if ((idx & CFGENTRYMASK) != CFGENTRYID ||
			    pc_cf->cfgid == 0)
				parse_cfent(tbuf, len, idx, pc_cf);
			break;
		default:
			break;
		}
	}
	return 0;
}


void
read_cfg_info(tbuf, len, pc_cf)
	u_char         *tbuf;
	int             len;
	struct pcmcia_conf *pc_cf;
{
	int             rasz, rmsz;

	rasz = (tbuf[0] & TPCC_RASZ) >> TPCC_RASZ_SHIFT;
	rmsz = (tbuf[0] & TPCC_RMSZ) >> TPCC_RMSZ_SHIFT;

#ifdef CFG_DEBUG
	printf("read_cfg_info\n");
	dump(tbuf, len);
#endif

	pc_cf->cfg_off = 0;
	switch (rasz) {
	case 3:
		pc_cf->cfg_off |= (tbuf[5] << 24);
	case 2:
		pc_cf->cfg_off |= (tbuf[4] << 16);
	case 1:
		pc_cf->cfg_off |= (tbuf[3] << 8);
	case 0:
		pc_cf->cfg_off |= tbuf[2];
	}

	tbuf += rasz + 3;
	pc_cf->cfg_regmask = 0;
	switch (rmsz & 3) {
	case 3:
		pc_cf->cfg_regmask |= (tbuf[3] << 24);
	case 2:
		pc_cf->cfg_regmask |= (tbuf[2] << 16);
	case 1:
		pc_cf->cfg_regmask |= (tbuf[1] << 8);
	case 0:
		pc_cf->cfg_regmask |= tbuf[0];
	}
}

void
parse_cfent(tbuf, len, slotid, pc_cf)
	u_char         *tbuf;
	int             len;
	int             slotid;
	struct pcmcia_conf *pc_cf;
{
	volatile int i, idx, defp, intface, k;
	int ios, ftrs;
	int host_addr_p, addr_size, len_size;

#ifdef CFG_DEBUG
	printf("parse_cfent\n");
	dump(tbuf, len);
#endif

	i = 0;
	intface = (tbuf[i] & TPCE_INDX_INT);
	idx = (tbuf[i] & TPCE_INDX_ENTRY);
	defp = (tbuf[i] & TPCE_INDX_DEF);

	if ((idx == slotid) || (defp && slotid!=-2 && 
				(slotid & CFGENTRYMASK) == CFGENTRYMASK)) {
		int j;
		if (intface) {
			i++;
			pc_cf->iocard = (tbuf[i] & TPCE_IF_TYPE) == 1;
		}
		i++;
		ftrs = tbuf[i++];
		for (j = 0; j < (ftrs & TPCE_FS_PWR); j++) {
			int             pwr_desc = tbuf[i++];
			/* for each struct, skip all parameter defns */
			for (k = 0; k < 8; pwr_desc >>= 1, k++) {
				if (pwr_desc & 0x01) {
					/* skip bytes until non-ext found */
					while (tbuf[i++] & 0x80)
						continue;
				}
			}
		}
		/* TODO read timing info */
		if (ftrs & TPCE_FS_TD) {
#define BONE(a,b)	(j & a) != 7 << b ? 1 : 0
			int j = tbuf[i++];
			i += ((j & TPCE_FS_TD_WAIT) != 3 ? 1 : 0);
			i += BONE(TPCE_FS_TD_RDY,TPCE_FS_TD_RDY_SHIFT);
			i += BONE(TPCE_FS_TD_RSV,TPCE_FS_TD_RSV_SHIFT);
#undef BONE
		}
		if (ftrs & TPCE_FS_IO) {
			int io_addrs[16], io_lens[16];
			int io_16, io_block_len, io_block_size, io_lines;
			int io_range;

			io_lines = tbuf[i] & TPCE_FS_IO_LINES;
			io_16 = tbuf[i] & TPCE_FS_IO_BUS16;
			io_range = tbuf[i] &TPCE_FS_IO_RANGE;
			i++;
			if (io_range) {
				int iptr, ilen, elen;

				io_block_len = (tbuf[i] & TPCE_FS_IO_LEN) >>
						TPCE_FS_IO_LEN_SHIFT;
				io_block_size = (tbuf[i] & TPCE_FS_IO_SIZE) >>
						TPCE_FS_IO_SIZE_SHIFT;
				ios = (tbuf[i] & TPCE_FS_IO_NUM) + 1;
				i++;
				elen=io_block_len+(io_block_len==3?1:0)+
					io_block_size+(io_block_size==3?1:0);
				if ((ftrs & TPCE_FS_IRQ) != 0) {
					iptr=(ios * elen) + i;
#define IRQTYPE (TPCE_FS_IRQ_PULSE|TPCE_FS_IRQ_LEVEL)
#define IRQMASK TPCE_FS_IRQ_MASK
					if ((tbuf[iptr] & IRQTYPE) == 0)
						if ((tbuf[iptr-elen] &
						     IRQTYPE) != 0)
							iptr -= elen;
					if ((tbuf[iptr] & IRQMASK) != 0)
						ilen = 2;
					else
						ilen=1;
				}
				else
					ilen=0;

				if ((i + (ios * elen) + ilen) > len) {
					printf(
"Warning: CIS range info doesn't fit in entry! Reducing # of ranges by 1\n");
					ios--;
				}

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
					pc_cf->io[j].start = io_addrs[j];
					i += io_block_size +
						(io_block_size == 3 ? 1 : 0);
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
					if(io_lens[j] & 1) {
					       printf(
"Odd IO window length!! (Assuming incorrect CIS entry %d)\n", io_lens[j]);
					       io_lens[j]--;
					}
			 
					pc_cf->io[j].len = io_lens[j];
					pc_cf->io[j].flags = io_16 ?
						PCMCIA_MAP_16 : PCMCIA_MAP_8;
					i += io_block_len +
						(io_block_len == 3 ? 1 : 0);
				}
				pc_cf->iowin = ios;
			}
			else {
				pc_cf->iowin = 1;
				pc_cf->io[0].len  = 1 << io_lines;
				pc_cf->io[0].start= 0 ;
				pc_cf->io[0].flags = io_16 ? 
					PCMCIA_MAP_16 : PCMCIA_MAP_8;
			}
		}
		if (ftrs & TPCE_FS_IRQ) {
			pc_cf->irq_level = (tbuf[i] & TPCE_FS_IRQ_LEVEL) != 0;
			pc_cf->irq_pulse = (tbuf[i] & TPCE_FS_IRQ_PULSE) != 0;
			pc_cf->irq_share = (tbuf[i] & TPCE_FS_IRQ_SHARE) != 0;
			if (tbuf[i] & TPCE_FS_IRQ_MASK) {
				pc_cf->irq_mask = (tbuf[i+2] << 8) + tbuf[i+1];
				if (pc_cf->irq_mask & (1 << 2))
					pc_cf->irq_mask |= 1 << 9;
				i += 2;
			} else {
				pc_cf->irq_num = tbuf[i] & TPCE_FS_IRQ_IRQN;
				pc_cf->irq_mask = -1;
			}

			i++;
		}
		if (ftrs & TPCE_FS_MEM) {
			int memp, mems, mem_lens[16], mem_caddrs[16],
			    mem_haddrs[16];
			memp = 1;
			switch ((ftrs & TPCE_FS_MEM) >> TPCE_FS_MEM_SHIFT) {
			case 1:
				mems = 1;
				mem_lens[0] = (tbuf[i+1] << 8) + tbuf[i];
				mem_lens[0] <<= 8;

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
						mem_lens[j] |=
						    (tbuf[i+2] << 16);
					case 2:
						mem_lens[j] |=
						    (tbuf[i+1] << 8);
					case 1:
						mem_lens[j] |= tbuf[i];
					}
					i += len_size;
					switch (addr_size) {
					case 3:
						mem_caddrs[j] |=
						    (tbuf[i+2] << 16);
					case 2:
						mem_caddrs[j] |=
						    (tbuf[i+1] << 8);
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
				break;
			default:
				mems = 0;
				break;
			}
			for (j = 0; j < mems; j++) {
				pc_cf->mem[j].len = mem_lens[j];
				pc_cf->mem[j].caddr = mem_caddrs[j];
				pc_cf->mem[j].start = mem_haddrs[j];
				pc_cf->mem[j].flags = 0;
			}
			pc_cf->memwin = mems;
		} else
			pc_cf->memwin = 0;
		return;
	}


	if (slotid == -2 ) {
		/* find matching slotid */
		struct pcmcia_conf tmp_cf;
		/* get defaults */
		parse_cfent(tbuf, len, -1, &tmp_cf);
		/* change to selected */
		parse_cfent(tbuf, len, idx, &tmp_cf);
#ifdef CFG_DEBUG
		printf("slotid %d %d iowin %d %d memwin %d %d wins %x %x %d %d\n",
		    pc_cf->iocard , tmp_cf.iocard,
		    pc_cf->iowin , tmp_cf.iowin,
		    pc_cf->memwin , tmp_cf.memwin,
		    pc_cf->io[0].start , tmp_cf.io[0].start,
		    pc_cf->io[0].len , tmp_cf.io[0].len
		    );
#endif

		if((pc_cf->iocard == tmp_cf.iocard) && /* same type */
		   (pc_cf->iowin == tmp_cf.iowin) &&
		   (pc_cf->memwin == tmp_cf.memwin)) {
			int i;
			for (i = 0; i < tmp_cf.iowin; i++)
				if (pc_cf->io[i].len != tmp_cf.io[i].len ||
				    pc_cf->io[i].start != tmp_cf.io[i].start)
					return;

			for (i = 0; i < tmp_cf.memwin; i++)
			    if (pc_cf->mem[i].len!=tmp_cf.mem[i].len ||
			        pc_cf->mem[i].start!=tmp_cf.mem[i].start)
				    return;

			/* *pc_cf = tmp_cf; */
			pc_cf->cfgid = idx;
		}
		return;
	}
}

void
pcmcia_getstr(buf, pptr, end)
	char *buf;
	u_char **pptr;
	u_char *end;
{
	u_char *ptr = *pptr;
	char *eb = buf + MAX_CIS_NAMELEN - 1;

	while (buf < eb && ptr < end)
		switch (*ptr) {
		case 0x00:
			ptr++;
			/*FALLTHROUGH*/
		case 0xff:
			*pptr = ptr;
			*buf = '\0';
			return;

		default:
			*buf++ = *ptr++;
			break;
		}
	printf("Warning: Maximum CIS string length exceeded\n");
	*buf = '\0';

	/* Keep going until we find the end */
	while (ptr < end)
		switch (*ptr) {
		case 0x00:
			ptr++;
			/*FALLTHROUGH*/
		case 0xff:
			*pptr = ptr;
			return;

		default:
			ptr++;
			break;
		}

	*pptr = ptr;
}


int
pcmcia_get_cisver1(pc_link, data, len, manu, model, add_inf1, add_inf2)
	struct pcmcia_link *pc_link;
	u_char         *data;
	int             len;
	char           *manu, *model, *add_inf1, *add_inf2;
{
	u_char         *p, *end;

	p = data;
	end = data + len;
	while ((*p != (u_char) 0xff) && (p < end)) {
		int             clen = *(p + 1);
		int             maj, min;
		if (*p == CIS_VER1) {
			u_char         *pp = p + 2;
			maj = *pp++;
			min = *pp++;
			if ((maj != 4 || min != 1) &&
			    (maj != 5 || min != 0)) {
				printf("wrong version id %d.%d for card in slot %d\n",
				       maj, min, pc_link->slot);
				return ENODEV;
			}
			pcmcia_getstr(manu, &pp, end); 
			pcmcia_getstr(model, &pp, end);
			pcmcia_getstr(add_inf1, &pp, end);
			pcmcia_getstr(add_inf2, &pp, end);
			if (*pp != (u_char) 0xff) {
				printf("WARNING: broken id for card in slot %d\n", pc_link->slot);
				printf("manu %s model %s add_inf1 %s add_inf2 %s\n", manu, model, add_inf1, add_inf2);
				return 0;
			}
			return 0;
		}
		p += clen + 2;
	}
#ifdef CFG_DEBUG
	printf("get_cisver1 failed, buffer [%p,%p)\n", p, end);
#endif
	return ENODEV;
}

#define NULLCP (void *)0

/* pcmcia template string match. A '*' matches any number of characters.
   A NULL template matches all strings.
   return-value 
    0 nomatch 
    1 wildcard match 
    2 excact match
 */
int
pcmcia_strcmp(templ, val, flags, msg)
	const char *templ;
	const char *val;
	int flags;
	const char *msg;
{
	const char *ltempl = NULLCP;
	const char *lval = NULLCP;

	if (flags & PC_SHOWME)
		printf("%s = `%s'-`%s'\n", msg, templ ? templ : "X", val);

	if (templ == NULLCP) {
	    return 1;
	}
	while (*val) {
		while (*templ == '*') {
			ltempl = ++templ;
			lval = val;
		}
		if (*templ == *val) {
			templ++;
			val++;
		} else {
			if (ltempl == NULLCP)
				return 0;
			val = ++lval;
			templ = ltempl;
		}
	}
	if (*templ != 0 && *templ != '*')
		return 0;
	return (ltempl ? 1 : 2);
}

/*
 * Return a match value to estimate how good a match the specified driver
 * is for this particular card.
 */
 
int
pcmcia_matchvalue(card, dentry)
	const struct pcmcia_cardinfo *card;
	struct pcmciadevs *dentry;
{
	int match;

#ifdef PCMCIA_DEBUG
	dentry->flags |= PC_SHOWME;
#endif
	match = pcmcia_strcmp(dentry->manufacturer,
			     card->manufacturer,
			     dentry->flags, "manufacturer")<<6;
	match |= pcmcia_strcmp(dentry->model,
			       card->model, dentry->flags, "model")<<4;
	match |= pcmcia_strcmp(dentry->add_inf1,
			       card->add_info1, dentry->flags, "info1")<<2;
	match |= pcmcia_strcmp(dentry->add_inf2,
			       card->add_info2, dentry->flags, "info2");
#ifdef PCMCIA_DEBUG
	printf("match == %d\n", match);
#endif
	return match;
}

int
pcmcia_bestvalue(card, dentries, nentries, rmatch)
	struct pcmcia_cardinfo *card;
	struct pcmciadevs *dentries;
	int nentries;
	struct pcmciadevs **rmatch;
{
	int bestmatch, thismatch;
	register int i;

	for (i = 0, bestmatch = 0; i < nentries; i++) {
		if ((thismatch = pcmcia_matchvalue(card, &dentries[i])) >
		    bestmatch) {
			bestmatch = thismatch;
			*rmatch = &dentries[i];
		}
	}
	return bestmatch;
}

int
pcmcia_slave_match(parent, match, aux, devs, ndevs)
	struct device *parent;
	void *match, *aux;
	struct pcmciadevs *devs;
	int ndevs;
{
	struct pcmcia_attach_args *paa = aux;
	struct device *self = match;
	struct pcmciadevs *devmatch;
	int value;

	if (paa->paa_link->fordriver &&
	    strcmp(paa->paa_link->fordriver,
		   self->dv_cfdata->cf_driver->cd_name))
	    return 0;			/* wrong driver */
	value = pcmcia_bestvalue(paa->paa_cardinfo, devs, ndevs, &devmatch);
	if (value > paa->paa_bestmatch) {
		paa->paa_bestmatch = value;
		paa->paa_link->device = devmatch;
#ifdef PCMCIA_DEBUG
		printf("pcmcia_slave_match: best so far, %p->%p\n",
		       paa->paa_link, devmatch);
		printf("pcmcia_slave_match returns %d\n", value);
		delay(2000000);
#endif
		if (!paa->paa_matchonly)
		    return value;
	}
	return 0;
}
