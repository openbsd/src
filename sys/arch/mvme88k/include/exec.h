/*	$OpenBSD: exec.h,v 1.6 1999/02/09 06:36:26 smurph Exp $ */
#ifndef _MACHINE_EXEC_H_
#define _MACHINE_EXEC_H_ 

#define __LDPGSZ        4096

struct relocation_info_m88k { 
        unsigned int r_address;         /* offset in text or data segment */
        unsigned int r_symbolnum : 24,  /* ordinal number of add symbol */
                        r_extern :  1,  /* 1 if need to add symbol to value */
                        r_baserel : 1,
                        r_pcrel : 1,
                        r_jmptable : 1,
                        r_type : 4;

        int r_addend;
};
#define relocation_info relocation_info_m88k

#define	_NLIST_DO_AOUT

#define	_KERN_DO_AOUT

#endif _MACHINE_EXEC_H_ 
