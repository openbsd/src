/*	$OpenBSD: memfile.pro,v 1.1.1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* memfile.c */
MEMFILE *mf_open __PARMS((char_u *fname, int trunc_file));
int mf_open_file __PARMS((MEMFILE *mfp, char_u *fname));
void mf_close __PARMS((MEMFILE *mfp, int del_file));
BHDR *mf_new __PARMS((MEMFILE *mfp, int negative, int page_count));
BHDR *mf_get __PARMS((MEMFILE *mfp, blocknr_t nr, int page_count));
void mf_put __PARMS((MEMFILE *mfp, BHDR *hp, int dirty, int infile));
void mf_free __PARMS((MEMFILE *mfp, BHDR *hp));
int mf_sync __PARMS((MEMFILE *mfp, int all, int check_char, int do_fsync));
int mf_release_all __PARMS((void));
blocknr_t mf_trans_del __PARMS((MEMFILE *mfp, blocknr_t old_nr));
void mf_set_xfname __PARMS((MEMFILE *mfp));
void mf_fullname __PARMS((MEMFILE *mfp));
int mf_need_trans __PARMS((MEMFILE *mfp));
void mf_statistics __PARMS((void));
