/*	$OpenBSD: alloc.pro,v 1.1.1.1 1996/09/07 21:40:30 downsj Exp $	*/
/* alloc.c */
char_u *alloc __PARMS((unsigned size));
char_u *alloc_check __PARMS((unsigned size));
char_u *lalloc __PARMS((long_u size, int message));
void do_outofmem_msg __PARMS((void));
char_u *strsave __PARMS((char_u *string));
char_u *strnsave __PARMS((char_u *string, int len));
char_u *strsave_escaped __PARMS((char_u *string, char_u *esc_chars));
void copy_spaces __PARMS((char_u *ptr, size_t count));
void del_trailing_spaces __PARMS((char_u *ptr));
int copy_option_part __PARMS((char_u **option, char_u *buf, int maxlen, char *sep_chars));
void vim_free __PARMS((void *x));
int vim_strnicmp __PARMS((char_u *s1, char_u *s2, size_t len));
char_u *vim_strchr __PARMS((char_u *string, int n));
char_u *vim_strrchr __PARMS((char_u *string, int n));
int vim_isspace __PARMS((int x));
