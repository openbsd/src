#define SAVEt_ITEM	0
#define SAVEt_SV	1
#define SAVEt_AV	2
#define SAVEt_HV	3
#define SAVEt_INT	4
#define SAVEt_LONG	5
#define SAVEt_I32	6
#define SAVEt_IV	7
#define SAVEt_SPTR	8
#define SAVEt_APTR	9
#define SAVEt_HPTR	10
#define SAVEt_PPTR	11
#define SAVEt_NSTAB	12
#define SAVEt_SVREF	13
#define SAVEt_GP	14
#define SAVEt_FREESV	15
#define SAVEt_FREEOP	16
#define SAVEt_FREEPV	17
#define SAVEt_CLEARSV	18
#define SAVEt_DELETE	19
#define SAVEt_DESTRUCTOR 20
#define SAVEt_REGCONTEXT 21

#define SSCHECK(need) if (savestack_ix + need > savestack_max) savestack_grow()
#define SSPUSHINT(i) (savestack[savestack_ix++].any_i32 = (I32)(i))
#define SSPUSHLONG(i) (savestack[savestack_ix++].any_long = (long)(i))
#define SSPUSHIV(i) (savestack[savestack_ix++].any_iv = (IV)(i))
#define SSPUSHPTR(p) (savestack[savestack_ix++].any_ptr = (void*)(p))
#define SSPUSHDPTR(p) (savestack[savestack_ix++].any_dptr = (p))
#define SSPOPINT (savestack[--savestack_ix].any_i32)
#define SSPOPLONG (savestack[--savestack_ix].any_long)
#define SSPOPIV (savestack[--savestack_ix].any_iv)
#define SSPOPPTR (savestack[--savestack_ix].any_ptr)
#define SSPOPDPTR (savestack[--savestack_ix].any_dptr)

#define SAVETMPS save_int((int*)&tmps_floor), tmps_floor = tmps_ix
#define FREETMPS if (tmps_ix > tmps_floor) free_tmps()
#ifdef DEPRECATED
#define FREE_TMPS() FREETMPS
#endif

#define ENTER push_scope()
#define LEAVE pop_scope()
#define LEAVE_SCOPE(old) if (savestack_ix > old) leave_scope(old)

#define SAVEINT(i) save_int((int*)(&i));
#define SAVEIV(i) save_iv((IV*)(&i));
#define SAVEI32(i) save_I32((I32*)(&i));
#define SAVELONG(l) save_long((long*)(&l));
#define SAVESPTR(s) save_sptr((SV**)(&s))
#define SAVEPPTR(s) save_pptr((char**)(&s))
#define SAVEFREESV(s) save_freesv((SV*)(s))
#define SAVEFREEOP(o) save_freeop((OP*)(o))
#define SAVEFREEPV(p) save_freepv((char*)(p))
#define SAVECLEARSV(sv) save_clearsv((SV**)(&sv))
#define SAVEDELETE(h,k,l) save_delete((HV*)(h), (char*)(k), (I32)l)
#define SAVEDESTRUCTOR(f,p) save_destructor(f,(void*)p)

