/*
 * Copyright 1996 1995 by Open Software Foundation, Inc.   
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */

#include <sys/exec_som.h>

int
som_recog(struct file *fp, objfmt_t ofmt, void *hdr)
{
    struct header *filehdr = *(struct header **)hdr;
    
    return (filehdr->system_id == CPU_PA_RISC1_0 ||
	    filehdr->system_id == CPU_PA_RISC1_1);
}

int
som_load(struct file *fp, objfmt_t ofmt, void *hdr)
{
    struct loader_info *lp = &ofmt->info;
    struct header filehdr;
    struct som_exec_auxhdr x;
    register int	result;

    /*
     * first read in the hp file header, there is a pointer to the "exec"
     * structure in the header.
     */

    result = read_file(fp, 0, (vm_offset_t)&filehdr, sizeof(filehdr)); 
    if (result)
	return (result);

    /*
     * now read in the hp800 equivalent of an exec structure
     */

    result = read_file(fp, (vm_offset_t)filehdr.aux_header_location,
		       (vm_offset_t)&x, sizeof(x));
    if (result)
	return (result);

    lp->text_start  = x.exec_tmem;
    lp->text_size   = round_page(x.exec_tsize);
    lp->text_offset = x.exec_tfile;
    lp->data_start  = x.exec_dmem;
    lp->data_size   = round_page(x.exec_dsize);
    lp->data_offset = x.exec_dfile;
    lp->bss_size    = x.exec_bsize;

    lp->entry_1 = x.exec_flags;
    lp->entry_2 = filehdr.presumed_dp;

    lp->sym_offset[0] = 0;
    lp->sym_size[0] = 0;

    lp->str_offset = 0;
    lp->str_size = 0;
    
    return 0;
}

