#include <machine/pcb.h>

struct reg {
    unsigned r_r[32];
    unsigned r_fpsr;
    unsigned r_fpcr;
    unsigned r_epsr;
    unsigned r_sxip;
    unsigned r_snip;
    unsigned r_sfip;
    unsigned r_ssbr;
    unsigned r_dmt0;
    unsigned r_dmd0;
    unsigned r_dma0;
    unsigned r_dmt1;
    unsigned r_dmd1;
    unsigned r_dma1;
    unsigned r_dmt2;
    unsigned r_dmd2;
    unsigned r_dma2;
    unsigned r_fpecr;
    unsigned r_fphs1;
    unsigned r_fpls1;
    unsigned r_fphs2;
    unsigned r_fpls2;
    unsigned r_fppt;
    unsigned r_fprh;
    unsigned r_fprl;
    unsigned r_fpit;
    unsigned r_vector;   /* exception vector number */
    unsigned r_mask;	   /* interrupt mask level */
    unsigned r_mode;     /* interrupt mode */
    unsigned r_scratch1; /* used by locore trap handling code */
    unsigned r_pad;      /* to make an even length */
} ;

struct fpreg {
    unsigned fp_fpecr;
    unsigned fp_fphs1;
    unsigned fp_fpls1;
    unsigned fp_fphs2;
    unsigned fp_fpls2;
    unsigned fp_fppt;
    unsigned fp_fprh;
    unsigned fp_fprl;
    unsigned fp_fpit;
};
