#ifndef hist_h
#define hist_h

#include "bfd.h"

extern bfd_vma s_lowpc;		/* lowpc from the profile file */
extern bfd_vma s_highpc;	/* highpc from the profile file */
extern bfd_vma lowpc, highpc;	/* range profiled, in UNIT's */
extern int hist_num_bins;	/* number of histogram bins */
extern int *hist_sample;	/* code histogram */
/*
 * Scale factor converting samples to pc values: each sample covers
 * HIST_SCALE bytes:
 */
extern double hist_scale;


extern void hist_read_rec PARAMS ((FILE * ifp, const char *filename));
extern void hist_write_hist PARAMS ((FILE * ofp, const char *filename));
extern void hist_assign_samples PARAMS ((void));
extern void hist_print PARAMS ((void));

#endif /* hist_h */
