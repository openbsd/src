#ifndef _PERLIOL_H
#define _PERLIOL_H

typedef struct {
    PerlIO_funcs *funcs;
    SV *arg;
} PerlIO_pair_t;

struct PerlIO_list_s {
    IV refcnt;
    IV cur;
    IV len;
    PerlIO_pair_t *array;
};

struct _PerlIO_funcs {
    Size_t fsize;
    char *name;
    Size_t size;
    U32 kind;
    IV (*Pushed) (pTHX_ PerlIO *f, const char *mode, SV *arg, PerlIO_funcs *tab);
    IV (*Popped) (pTHX_ PerlIO *f);
    PerlIO *(*Open) (pTHX_ PerlIO_funcs *tab,
		     PerlIO_list_t *layers, IV n,
		     const char *mode,
		     int fd, int imode, int perm,
		     PerlIO *old, int narg, SV **args);
    IV (*Binmode)(pTHX_ PerlIO *f);
    SV *(*Getarg) (pTHX_ PerlIO *f, CLONE_PARAMS *param, int flags);
    IV (*Fileno) (pTHX_ PerlIO *f);
    PerlIO *(*Dup) (pTHX_ PerlIO *f, PerlIO *o, CLONE_PARAMS *param, int flags);
    /* Unix-like functions - cf sfio line disciplines */
     SSize_t(*Read) (pTHX_ PerlIO *f, void *vbuf, Size_t count);
     SSize_t(*Unread) (pTHX_ PerlIO *f, const void *vbuf, Size_t count);
     SSize_t(*Write) (pTHX_ PerlIO *f, const void *vbuf, Size_t count);
    IV (*Seek) (pTHX_ PerlIO *f, Off_t offset, int whence);
     Off_t(*Tell) (pTHX_ PerlIO *f);
    IV (*Close) (pTHX_ PerlIO *f);
    /* Stdio-like buffered IO functions */
    IV (*Flush) (pTHX_ PerlIO *f);
    IV (*Fill) (pTHX_ PerlIO *f);
    IV (*Eof) (pTHX_ PerlIO *f);
    IV (*Error) (pTHX_ PerlIO *f);
    void (*Clearerr) (pTHX_ PerlIO *f);
    void (*Setlinebuf) (pTHX_ PerlIO *f);
    /* Perl's snooping functions */
    STDCHAR *(*Get_base) (pTHX_ PerlIO *f);
     Size_t(*Get_bufsiz) (pTHX_ PerlIO *f);
    STDCHAR *(*Get_ptr) (pTHX_ PerlIO *f);
     SSize_t(*Get_cnt) (pTHX_ PerlIO *f);
    void (*Set_ptrcnt) (pTHX_ PerlIO *f, STDCHAR * ptr, SSize_t cnt);
};

/*--------------------------------------------------------------------------------------*/
/* Kind values */
#define PERLIO_K_RAW		0x00000001
#define PERLIO_K_BUFFERED	0x00000002
#define PERLIO_K_CANCRLF	0x00000004
#define PERLIO_K_FASTGETS	0x00000008
#define PERLIO_K_DUMMY		0x00000010
#define PERLIO_K_UTF8		0x00008000
#define PERLIO_K_DESTRUCT	0x00010000
#define PERLIO_K_MULTIARG	0x00020000

/*--------------------------------------------------------------------------------------*/
struct _PerlIO {
    PerlIOl *next;		/* Lower layer */
    PerlIO_funcs *tab;		/* Functions for this layer */
    U32 flags;			/* Various flags for state */
};

/*--------------------------------------------------------------------------------------*/

/* Flag values */
#define PERLIO_F_EOF		0x00000100
#define PERLIO_F_CANWRITE	0x00000200
#define PERLIO_F_CANREAD	0x00000400
#define PERLIO_F_ERROR		0x00000800
#define PERLIO_F_TRUNCATE	0x00001000
#define PERLIO_F_APPEND		0x00002000
#define PERLIO_F_CRLF		0x00004000
#define PERLIO_F_UTF8		0x00008000
#define PERLIO_F_UNBUF		0x00010000
#define PERLIO_F_WRBUF		0x00020000
#define PERLIO_F_RDBUF		0x00040000
#define PERLIO_F_LINEBUF	0x00080000
#define PERLIO_F_TEMP		0x00100000
#define PERLIO_F_OPEN		0x00200000
#define PERLIO_F_FASTGETS	0x00400000
#define PERLIO_F_TTY		0x00800000

#define PerlIOBase(f)      (*(f))
#define PerlIOSelf(f,type) ((type *)PerlIOBase(f))
#define PerlIONext(f)      (&(PerlIOBase(f)->next))
#define PerlIOValid(f)     ((f) && *(f))

/*--------------------------------------------------------------------------------------*/
/* Data exports - EXT rather than extern is needed for Cygwin */
EXT PerlIO_funcs PerlIO_unix;
EXT PerlIO_funcs PerlIO_perlio;
EXT PerlIO_funcs PerlIO_stdio;
EXT PerlIO_funcs PerlIO_crlf;
EXT PerlIO_funcs PerlIO_utf8;
EXT PerlIO_funcs PerlIO_byte;
EXT PerlIO_funcs PerlIO_raw;
EXT PerlIO_funcs PerlIO_pending;
#ifdef HAS_MMAP
EXT PerlIO_funcs PerlIO_mmap;
#endif
#ifdef WIN32
EXT PerlIO_funcs PerlIO_win32;
#endif
extern PerlIO *PerlIO_allocate(pTHX);
extern SV *PerlIO_arg_fetch(PerlIO_list_t *av, IV n);
#define PerlIOArg PerlIO_arg_fetch(layers,n)

#ifdef PERLIO_USING_CRLF
#define PERLIO_STDTEXT "t"
#else
#define PERLIO_STDTEXT ""
#endif

/*--------------------------------------------------------------------------------------*/
/* Generic, or stub layer functions */

extern IV PerlIOBase_fileno(pTHX_ PerlIO *f);
extern PerlIO *PerlIOBase_dup(pTHX_ PerlIO *f, PerlIO *o, CLONE_PARAMS *param, int flags);
extern IV PerlIOBase_pushed(pTHX_ PerlIO *f, const char *mode, SV *arg, PerlIO_funcs *tab);
extern IV PerlIOBase_popped(pTHX_ PerlIO *f);
extern IV PerlIOBase_binmode(pTHX_ PerlIO *f);
extern SSize_t PerlIOBase_read(pTHX_ PerlIO *f, void *vbuf, Size_t count);
extern SSize_t PerlIOBase_unread(pTHX_ PerlIO *f, const void *vbuf,
				 Size_t count);
extern IV PerlIOBase_eof(pTHX_ PerlIO *f);
extern IV PerlIOBase_error(pTHX_ PerlIO *f);
extern void PerlIOBase_clearerr(pTHX_ PerlIO *f);
extern IV PerlIOBase_close(pTHX_ PerlIO *f);
extern void PerlIOBase_setlinebuf(pTHX_ PerlIO *f);
extern void PerlIOBase_flush_linebuf(pTHX);

extern IV PerlIOBase_noop_ok(pTHX_ PerlIO *f);
extern IV PerlIOBase_noop_fail(pTHX_ PerlIO *f);

/*--------------------------------------------------------------------------------------*/
/* perlio buffer layer
   As this is reasonably generic its struct and "methods" are declared here
   so they can be used to "inherit" from it.
*/

typedef struct {
    struct _PerlIO base;	/* Base "class" info */
    STDCHAR *buf;		/* Start of buffer */
    STDCHAR *end;		/* End of valid part of buffer */
    STDCHAR *ptr;		/* Current position in buffer */
    Off_t posn;			/* Offset of buf into the file */
    Size_t bufsiz;		/* Real size of buffer */
    IV oneword;			/* Emergency buffer */
} PerlIOBuf;

extern int PerlIO_apply_layera(pTHX_ PerlIO *f, const char *mode,
		    PerlIO_list_t *layers, IV n, IV max);
extern int PerlIO_parse_layers(pTHX_ PerlIO_list_t *av, const char *names);
extern void PerlIO_list_free(pTHX_ PerlIO_list_t *list);
extern PerlIO_funcs *PerlIO_layer_fetch(pTHX_ PerlIO_list_t *av, IV n, PerlIO_funcs *def);


extern SV *PerlIO_sv_dup(pTHX_ SV *arg, CLONE_PARAMS *param);
extern PerlIO *PerlIOBuf_open(pTHX_ PerlIO_funcs *self,
			      PerlIO_list_t *layers, IV n,
			      const char *mode, int fd, int imode,
			      int perm, PerlIO *old, int narg, SV **args);
extern IV PerlIOBuf_pushed(pTHX_ PerlIO *f, const char *mode, SV *arg, PerlIO_funcs *tab);
extern IV PerlIOBuf_popped(pTHX_ PerlIO *f);
extern PerlIO *PerlIOBuf_dup(pTHX_ PerlIO *f, PerlIO *o, CLONE_PARAMS *param, int flags);
extern SSize_t PerlIOBuf_read(pTHX_ PerlIO *f, void *vbuf, Size_t count);
extern SSize_t PerlIOBuf_unread(pTHX_ PerlIO *f, const void *vbuf, Size_t count);
extern SSize_t PerlIOBuf_write(pTHX_ PerlIO *f, const void *vbuf, Size_t count);
extern IV PerlIOBuf_seek(pTHX_ PerlIO *f, Off_t offset, int whence);
extern Off_t PerlIOBuf_tell(pTHX_ PerlIO *f);
extern IV PerlIOBuf_close(pTHX_ PerlIO *f);
extern IV PerlIOBuf_flush(pTHX_ PerlIO *f);
extern IV PerlIOBuf_fill(pTHX_ PerlIO *f);
extern STDCHAR *PerlIOBuf_get_base(pTHX_ PerlIO *f);
extern Size_t PerlIOBuf_bufsiz(pTHX_ PerlIO *f);
extern STDCHAR *PerlIOBuf_get_ptr(pTHX_ PerlIO *f);
extern SSize_t PerlIOBuf_get_cnt(pTHX_ PerlIO *f);
extern void PerlIOBuf_set_ptrcnt(pTHX_ PerlIO *f, STDCHAR * ptr, SSize_t cnt);

extern int PerlIOUnix_oflags(const char *mode);

/*--------------------------------------------------------------------------------------*/

#endif				/* _PERLIOL_H */
