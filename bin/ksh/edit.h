/*	$OpenBSD: edit.h,v 1.3 1999/11/14 22:04:02 d Exp $	*/

/* NAME:
 *      edit.h - globals for edit modes
 *
 * DESCRIPTION:
 *      This header defines various global edit objects.
 *
 * SEE ALSO:
 *      
 *
 * RCSid:
 *      $From: edit.h,v 1.2 1994/05/19 18:32:40 michael Exp michael $
 *
 */

/* some useful #defines */
#ifdef EXTERN
# define I__(i) = i
#else
# define I__(i)
# define EXTERN extern
# define EXTERN_DEFINED
#endif

#define	BEL		0x07

/* tty driver characters we are interested in */
typedef struct {
	int erase;
	int kill;
	int werase;
	int intr;
	int quit;
	int eof;
} X_chars;

EXTERN X_chars edchars;

/* x_fc_glob() flags */
#define XCF_COMMAND	BIT(0)	/* Do command completion */
#define XCF_FILE	BIT(1)	/* Do file completion */
#define XCF_FULLPATH	BIT(2)	/* command completion: store full path */
#define XCF_COMMAND_FILE (XCF_COMMAND|XCF_FILE)

/* edit.c */
int 	x_getc		ARGS((void));
void 	x_flush		ARGS((void));
void 	x_putc		ARGS((int c));
void 	x_puts		ARGS((const char *s));
bool_t 	x_mode		ARGS((bool_t onoff));
int 	promptlen	ARGS((const char *cp, const char **spp));
int	x_do_comment	ARGS((char *buf, int bsize, int *lenp));
void	x_print_expansions ARGS((int nwords, char *const *words, int is_command));
int	x_cf_glob ARGS((int flags, const char *buf, int buflen, int pos, int *startp,
			  int *endp, char ***wordsp, int *is_commandp));
int	x_longest_prefix ARGS((int nwords, char *const *words));
int	x_basename ARGS((const char *s, const char *se));
void	x_free_words ARGS((int nwords, char **words));
int	x_escape ARGS((const char *, size_t, int (*)(const char *s, size_t len)));
/* emacs.c */
int 	x_emacs		ARGS((char *buf, size_t len));
void 	x_init_emacs	ARGS((void));
void	x_emacs_keys	ARGS((X_chars *ec));
/* vi.c */
int 	x_vi		ARGS((char *buf, size_t len));


#ifdef DEBUG
# define D__(x) x
#else
# define D__(x)
#endif

/* This lot goes at the END */
/* be sure not to interfere with anyone else's idea about EXTERN */
#ifdef EXTERN_DEFINED
# undef EXTERN_DEFINED
# undef EXTERN
#endif
#undef I__
/*
 * Local Variables:
 * version-control:t
 * comment-column:40
 * End:
 */
