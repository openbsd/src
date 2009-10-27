/*	$OpenBSD: file.c,v 1.13 2009/10/27 23:59:52 deraadt Exp $ */

/*
 * Copyright (c) 1995-96 Mats O Jansson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include "os.h"
#include "common/common.h"
#include "common/mopdef.h"

#define INFO_PRINT 1

#ifndef NOAOUT
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/exec_aout.h>
#endif
#if defined(__bsdi__)
#define NOAOUT
#endif
#if defined(__FreeBSD__)
#include <sys/imgact_aout.h>
#endif
#if !defined(MID_I386)
#define MID_I386 134
#endif
#if !defined(MID_SPARC)
#define MID_SPARC 138
#endif
#if !defined(MID_VAX)
#define MID_VAX 140
#endif
#endif

void
mopFilePutLX(u_char *buf, int idx, u_long value, int cnt)
{
	int i;
	for (i = 0; i < cnt; i++) {
		buf[idx+i] = (u_char)(value % 256);
		value = (u_char)(value / 256);
	}
}

void
mopFilePutBX(u_char *buf, int idx, u_long value, int cnt)
{
	int i;
	for (i = 0; i < cnt; i++) {
		buf[idx+cnt-1-i] = (u_char)(value % 256);
		value = value / 256;
	}
}

u_long
mopFileGetLX(void *buffer, int idx, int cnt)
{
	u_long	 ret = 0;
	int	 i;
	u_char	*buf = (u_char *)buffer;

	for (i = 0; i < cnt; i++)
		ret = ret*256 + buf[idx+cnt-1-i];

	return (ret);
}

u_long
mopFileGetBX(void *buffer, int idx, int cnt)
{
	u_long	 ret = 0;
	int	 i;
	u_char	*buf = (u_char *)buffer;

	for (i = 0; i < cnt; i++)
		ret = ret*256 + buf[idx+i];

	return (ret);
}

void
mopFileSwapX(void *buffer, int idx, int cnt)
{
	int	 i;
	u_char	 c;
	u_char	*buf = (u_char *)buffer;

	for (i = 0; i < (cnt / 2); i++) {
		c = buf[idx+i];
		buf[idx+i] = buf[idx+cnt-1-i];
		buf[idx+cnt-1-i] = c;
	}

}

int
CheckMopFile(int fd)
{
	u_char	header[512];
	short	image_type;

	if (read(fd, header, 512) != 512)
		return (-1);

	lseek(fd, 0, SEEK_SET);

	image_type = (short)mopFileGetLX(header,IHD_W_ALIAS,2);

	switch (image_type) {
		case IHD_C_NATIVE:		/* Native mode image (VAX)   */
		case IHD_C_RSX:			/* RSX image produced by TKB */
		case IHD_C_BPA:			/* BASIC plus analog         */
		case IHD_C_ALIAS:		/* Alias		     */
		case IHD_C_CLI:			/* Image is CLI		     */
		case IHD_C_PMAX:		/* PMAX system image	     */
		case IHD_C_ALPHA:		/* ALPHA system image	     */
			break;
		default:
			return (-1);
	}

	return (0);
}

int
GetMopFileInfo(int fd, u_long *load, u_long *xfr, int info)
{
	u_char	header[512];
	short	image_type, isd, iha;
	u_long	load_addr, xfr_addr, isize, hbcnt;

	if (read(fd, header, 512) != 512)
		return (-1);

	image_type = (short)mopFileGetLX(header,IHD_W_ALIAS,2);

	switch (image_type) {
		case IHD_C_NATIVE:		/* Native mode image (VAX)   */
			isd = (short)mopFileGetLX(header,IHD_W_SIZE,2);
			iha = (short)mopFileGetLX(header,IHD_W_ACTIVOFF,2);
			hbcnt = header[IHD_B_HDRBLKCNT];
			isize = mopFileGetLX(header,isd+ISD_W_PAGCNT,2) * 512;
			load_addr = (mopFileGetLX(header,isd+ISD_V_VPN,2) &
			    ISD_M_VPN) * 512;
			xfr_addr = mopFileGetLX(header,iha+IHA_L_TFRADR1,4) &
			    0x7fffffff;
			if (info == INFO_PRINT) {
				printf("Native Image (VAX)\n");
				printf("Header Block Count: %lu\n", hbcnt);
				printf("Image Size:         %08lx\n", isize);
				printf("Load Address:       %08lx\n", load_addr);
				printf("Transfer Address:   %08lx\n", xfr_addr);
			}
			break;
		case IHD_C_RSX:			/* RSX image produced by TKB */
			hbcnt = mopFileGetLX(header,L_BBLK,2);
			isize = mopFileGetLX(header,L_BLDZ,2) * 64;
			load_addr = mopFileGetLX(header,L_BSA,2);
			xfr_addr = mopFileGetLX(header,L_BXFR,2);
			if (info == INFO_PRINT) {
				printf("RSX Image\n");
				printf("Header Block Count: %lu\n",hbcnt);
				printf("Image Size:         %08lx\n", isize);
				printf("Load Address:       %08lx\n", load_addr);
				printf("Transfer Address:   %08lx\n", xfr_addr);
			}
			break;
		case IHD_C_BPA:			/* BASIC plus analog         */
			if (info == INFO_PRINT) {
				printf("BASIC-Plus Image, not supported\n");
			}
			return (-1);
		case IHD_C_ALIAS:		/* Alias		     */
			if (info == INFO_PRINT) {
				printf("Alias, not supported\n");
			}
			return (-1);
		case IHD_C_CLI:			/* Image is CLI		     */
			if (info == INFO_PRINT) {
				printf("CLI, not supported\n");
			}
			return (-1);
		case IHD_C_PMAX:		/* PMAX system image	     */
			isd = (short)mopFileGetLX(header,IHD_W_SIZE,2);
			iha = (short)mopFileGetLX(header,IHD_W_ACTIVOFF,2);
			hbcnt = header[IHD_B_HDRBLKCNT];
			isize = mopFileGetLX(header,isd+ISD_W_PAGCNT,2) * 512;
			load_addr = mopFileGetLX(header,isd+ISD_V_VPN,2) * 512;
			xfr_addr = mopFileGetLX(header,iha+IHA_L_TFRADR1,4);
			if (info == INFO_PRINT) {
				printf("PMAX Image \n");
				printf("Header Block Count: %lu\n", hbcnt);
				printf("Image Size:         %08lx\n", isize);
				printf("Load Address:       %08lx\n", load_addr);
				printf("Transfer Address:   %08lx\n", xfr_addr);
			}
			break;
		case IHD_C_ALPHA:		/* ALPHA system image	     */
			isd = (short)mopFileGetLX(header,EIHD_L_ISDOFF,4);
			hbcnt = mopFileGetLX(header,EIHD_L_HDRBLKCNT,4);
			isize = mopFileGetLX(header,isd+EISD_L_SECSIZE,4);
			load_addr = 0;
			xfr_addr = 0;
			if (info == INFO_PRINT) {
				printf("Alpha Image \n");
				printf("Header Block Count: %lu\n", hbcnt);
				printf("Image Size:         %08lx\n", isize);
				printf("Load Address:       %08lx\n", load_addr);
				printf("Transfer Address:   %08lx\n", xfr_addr);
			}
			break;
		default:
			if (info == INFO_PRINT) {
				printf("Unknown Image (%d)\n", image_type);
			}
			return (-1);
	}

	if (load != NULL)
		*load = load_addr;

	if (xfr != NULL)
		*xfr  = xfr_addr;

	return (0);
}

#ifndef NOAOUT
int
getMID(int old_mid, int new_mid)
{
	int	mid;

	mid = old_mid;

	switch (new_mid) {
	case MID_I386:
		mid = MID_I386;
		break;
#ifdef MID_M68K
	case MID_M68K:
		mid = MID_M68K;
		break;
#endif
#ifdef MID_M68K4K
	case MID_M68K4K:
		mid = MID_M68K4K;
		break;
#endif
#ifdef MID_NS32532
	case MID_NS32532:
		mid = MID_NS32532;
		break;
#endif
	case MID_SPARC:
		mid = MID_SPARC;
		break;
#ifdef MID_PMAX
	case MID_PMAX:
		mid = MID_PMAX;
		break;
#endif
#ifdef MID_VAX
	case MID_VAX:
		mid = MID_VAX;
		break;
#endif
#ifdef MID_ALPHA
	case MID_ALPHA:
		mid = MID_ALPHA;
		break;
#endif
#ifdef MID_MIPS
	case MID_MIPS:
		mid = MID_MIPS;
		break;
#endif
#ifdef MID_ARM6
	case MID_ARM6:
		mid = MID_ARM6;
		break;
#endif
	default:
		break;
	}

	return (mid);
}

u_int
getCLBYTES(int mid)
{
	u_int	clbytes;

	switch (mid) {
#ifdef MID_VAX
	case MID_VAX:
		clbytes = 1024;
		break;
#endif
	case MID_I386:
#ifdef MID_M68K4K
	case MID_M68K4K:
#endif
#ifdef MID_NS32532
	case MID_NS32532:
#endif
	case MID_SPARC:				/* It might be 8192 */
#ifdef MID_PMAX
	case MID_PMAX:
#endif
#ifdef MID_MIPS
	case MID_MIPS:
#endif
#ifdef MID_ARM6
	case MID_ARM6:
#endif
		clbytes = 4096;
		break;
#ifdef MID_M68K
	case MID_M68K:
#endif
#ifdef MID_ALPHA
	case MID_ALPHA:
#endif
#if defined(MID_M68K) || defined(MID_ALPHA)
		clbytes = 8192;
		break;
#endif
	default:
		clbytes = 0;
	}

	return (clbytes);
}
#endif

int
CheckAOutFile(int fd)
{
#ifdef NOAOUT
	return (-1);
#else
	struct exec	ex, ex_swap;
	int		mid = -1;

	if (read(fd, &ex, sizeof(ex)) != (ssize_t)sizeof(ex))
		return (-1);

	lseek(fd, 0, SEEK_SET);

	if (read(fd, &ex_swap, sizeof(ex_swap)) != (ssize_t)sizeof(ex_swap))
		return (-1);

	lseek(fd, 0, SEEK_SET);

	mid = getMID(mid, (int)N_GETMID(ex));

	if (mid == -1)
		mid = getMID(mid, (int)N_GETMID(ex_swap));

	if (mid != -1)
		return (0);
	else
		return (-1);
#endif /* NOAOUT */
}

int
GetAOutFileInfo(int fd, u_long *load, u_long *xfr, u_long *a_text,
    u_long *a_text_fill, u_long *a_data, u_long *a_data_fill, u_long *a_bss,
    u_long *a_bss_fill, int *aout, int info)
{
#ifdef NOAOUT
	return (-1);
#else
	struct exec	ex, ex_swap;
	int		mid = -1;
	u_long		magic, clbytes, clofset;

	if (read(fd, &ex, sizeof(ex)) != (ssize_t)sizeof(ex))
		return (-1);

	lseek(fd, 0, SEEK_SET);

	if (read(fd, &ex_swap, sizeof(ex_swap)) != (ssize_t)sizeof(ex_swap))
		return (-1);

	mopFileSwapX(&ex_swap, 0, 4);

	mid = getMID(mid, (int)N_GETMID(ex));

	if (mid == -1) {
		mid = getMID(mid, (int)N_GETMID(ex_swap));
		if (mid != -1)
			mopFileSwapX(&ex, 0, 4);
	}

	if (mid == -1)
		return (-1);

	if (N_BADMAG(ex))
		return (-1);

	switch (mid) {
	case MID_I386:
#ifdef MID_NS32532
	case MID_NS32532:
#endif
#ifdef MID_PMAX
	case MID_PMAX:
#endif
#ifdef MID_VAX
	case MID_VAX:
#endif
#ifdef MID_ALPHA
	case MID_ALPHA:
#endif
#ifdef MID_ARM6
	case MID_ARM6:
#endif
		ex.a_text   = (u_int)mopFileGetLX(&ex_swap,  4, 4);
		ex.a_data   = (u_int)mopFileGetLX(&ex_swap,  8, 4);
		ex.a_bss    = (u_int)mopFileGetLX(&ex_swap, 12, 4);
		ex.a_syms   = (u_int)mopFileGetLX(&ex_swap, 16, 4);
		ex.a_entry  = (u_int)mopFileGetLX(&ex_swap, 20, 4);
		ex.a_trsize = (u_int)mopFileGetLX(&ex_swap, 24, 4);
		ex.a_drsize = (u_int)mopFileGetLX(&ex_swap, 28, 4);
		break;
#ifdef MID_M68K
	case MID_M68K:
#endif
#ifdef MID_M68K4K
	case MID_M68K4K:
#endif
	case MID_SPARC:
#ifdef MID_MIPS
	case MID_MIPS:
#endif
		ex.a_text   = (u_int)mopFileGetBX(&ex_swap,  4, 4);
		ex.a_data   = (u_int)mopFileGetBX(&ex_swap,  8, 4);
		ex.a_bss    = (u_int)mopFileGetBX(&ex_swap, 12, 4);
		ex.a_syms   = (u_int)mopFileGetBX(&ex_swap, 16, 4);
		ex.a_entry  = (u_int)mopFileGetBX(&ex_swap, 20, 4);
		ex.a_trsize = (u_int)mopFileGetBX(&ex_swap, 24, 4);
		ex.a_drsize = (u_int)mopFileGetBX(&ex_swap, 28, 4);
		break;
	default:
		break;
	}

	if (info == INFO_PRINT) {
		printf("a.out image (");
		switch (N_GETMID(ex)) {
		case MID_I386:
			printf("i386");
			break;
#ifdef MID_M68K
		case MID_M68K:
			printf("m68k");
			break;
#endif
#ifdef MID_M68K4K
		case MID_M68K4K:
			printf("m68k 4k");
			break;
#endif
#ifdef MID_NS32532
		case MID_NS32532:
			printf("pc532");
			break;
#endif
		case MID_SPARC:
			printf("sparc");
			break;
#ifdef MID_PMAX
		case MID_PMAX:
			printf("pmax");
			break;
#endif
#ifdef MID_VAX
		case MID_VAX:
			printf("vax");
			break;
#endif
#ifdef MID_ALPHA
		case MID_ALPHA:
			printf("alpha");
			break;
#endif
#ifdef MID_MIPS
		case MID_MIPS:
			printf("mips");
			break;
#endif
#ifdef MID_ARM6
		case MID_ARM6:
			printf("arm32");
			break;
#endif
		default:
			break;
		}
		printf(") Magic: ");
		switch (N_GETMAGIC (ex)) {
		case OMAGIC:
			printf("OMAGIC");
			break;
		case NMAGIC:
			printf("NMAGIC");
			break;
		case ZMAGIC:
			printf("ZMAGIC");
			break;
		case QMAGIC:
			printf("QMAGIC");
			break;
		default:
			printf("Unknown %d",N_GETMAGIC (ex));
		}
		printf("\n");
		printf("Size of text:       %08x\n", ex.a_text);
		printf("Size of data:       %08x\n", ex.a_data);
		printf("Size of bss:        %08x\n", ex.a_bss);
		printf("Size of symbol tab: %08x\n", ex.a_syms);
		printf("Transfer Address:   %08x\n", ex.a_entry);
		printf("Size of reloc text: %08x\n", ex.a_trsize);
		printf("Size of reloc data: %08x\n", ex.a_drsize);
	}
	magic = N_GETMAGIC(ex);
	clbytes = getCLBYTES(mid);
	clofset = clbytes - 1;

	if (load != NULL)
		*load   = 0;

	if (xfr != NULL)
		*xfr = ex.a_entry;

	if (a_text != NULL)
		*a_text = ex.a_text;

	if (a_text_fill != NULL) {
		if (magic == ZMAGIC || magic == NMAGIC) {
			*a_text_fill = clbytes - (ex.a_text & clofset);
			if (*a_text_fill == clbytes)
				*a_text_fill = 0;
		} else
			*a_text_fill = 0;
	}

	if (a_data != NULL)
		*a_data = ex.a_data;

	if (a_data_fill != NULL) {
		if (magic == ZMAGIC || magic == NMAGIC) {
			*a_data_fill = clbytes - (ex.a_data & clofset);
			if (*a_data_fill == clbytes)
				*a_data_fill = 0;
		} else
			*a_data_fill = 0;
	}

	if (a_bss != NULL)
		*a_bss  = ex.a_bss;

	if (a_bss_fill != NULL) {
		if (magic == ZMAGIC || magic == NMAGIC) {
			*a_bss_fill = clbytes - (ex.a_bss & clofset);
			if (*a_bss_fill == clbytes)
				*a_bss_fill = 0;
		} else {
			*a_bss_fill = clbytes -
			    ((ex.a_text + ex.a_data + ex.a_bss) & clofset);
			if (*a_text_fill == clbytes)
				*a_text_fill = 0;
	        }
	}

	if (aout != NULL)
		*aout = mid;

	return (0);
#endif /* NOAOUT */
}

int
GetFileInfo(int fd, u_long *load, u_long *xfr, int *aout, u_long *a_text,
    u_long *a_text_fill, u_long *a_data, u_long *a_data_fill, u_long *a_bss,
    u_long *a_bss_fill, int info)
{
	int	err;

	err = CheckAOutFile(fd);

	if (err == 0) {
		err = GetAOutFileInfo(fd, load, xfr, a_text, a_text_fill,
		    a_data, a_data_fill, a_bss, a_bss_fill, aout, info);
		if (err != 0)
			return (-1);
	} else {
		err = CheckMopFile(fd);

		if (err == 0) {
			err = GetMopFileInfo(fd, load, xfr, info);
			if (err != 0)
				return (-1);
			*aout = -1;
		} else
			return (-1);
	}

	return (0);
}

ssize_t
mopFileRead(struct dllist *dlslot, u_char *buf)
{
	ssize_t len, outlen;
	u_long	bsz, total, notdone;
	off_t	pos;
	
	if (dlslot->aout == -1)
		len = read(dlslot->ldfd,buf,dlslot->dl_bsz);
	else {
		bsz = dlslot->dl_bsz;
		pos = dlslot->a_lseek;
		len = 0;

		total = dlslot->a_text;

		if (pos < (off_t)total) {
			notdone = total - (u_long)pos;
			if (notdone <= bsz)
				outlen = read(dlslot->ldfd,&buf[len],notdone);
			else
				outlen = read(dlslot->ldfd,&buf[len],bsz);
			len = len + outlen;
			pos = pos + outlen;
			bsz = bsz - (u_long)outlen;
		}

		total = total + dlslot->a_text_fill;

		if ((bsz > 0) && (pos < (off_t)total)) {
			notdone = total - (u_long)pos;
			if (notdone <= bsz)
				outlen = (ssize_t)notdone;
			else
				outlen = (ssize_t)bsz;
			bzero(&buf[len],(u_long)outlen);
			len = len + outlen;
			pos = pos + outlen;
			bsz = bsz - (u_long)outlen;
		}

		total = total + dlslot->a_data;

		if ((bsz > 0) && (pos < (off_t)total)) {
			notdone = total - (u_long)pos;
			if (notdone <= bsz)
				outlen = read(dlslot->ldfd,&buf[len],notdone);
			else
				outlen = read(dlslot->ldfd,&buf[len],bsz);
			len = len + outlen;
			pos = pos + outlen;
			bsz = bsz - (u_long)outlen;
		}

		total = total + dlslot->a_data_fill;

		if ((bsz > 0) && (pos < (off_t)total)) {
			notdone = total - (u_long)pos;
			if (notdone <= bsz)
				outlen = (ssize_t)notdone;
			else
				outlen = (ssize_t)bsz;
			bzero(&buf[len],(u_long)outlen);
			len = len + outlen;
			pos = pos + outlen;
			bsz = bsz - (u_long)outlen;
		}

		total = total + dlslot->a_bss;

		if ((bsz > 0) && (pos < (off_t)total)) {
			notdone = total - (u_long)pos;
			if (notdone <= bsz)
				outlen = (ssize_t)notdone;
			else
				outlen = (ssize_t)bsz;
			bzero(&buf[len],(u_long)outlen);
			len = len + outlen;
			pos = pos + outlen;
			bsz = bsz - (u_long)outlen;
		}

		total = total + dlslot->a_bss_fill;

		if ((bsz > 0) && (pos < (off_t)total)) {
			notdone = total - (u_long)pos;
			if (notdone <= bsz)
				outlen = (ssize_t)notdone;
			else
				outlen = (ssize_t)bsz;
			bzero(&buf[len],(u_long)outlen);
			len = len + outlen;
			pos = pos + outlen;
			bsz = bsz - (u_long)outlen;
		}

		dlslot->a_lseek = pos;

	}

	return (len);
}
