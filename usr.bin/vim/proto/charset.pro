/*	$OpenBSD: charset.pro,v 1.1.1.1 1996/09/07 21:40:28 downsj Exp $	*/
/* charset.c */
int init_chartab __PARMS((void));
void trans_characters __PARMS((char_u *buf, int bufsize));
char_u *transchar __PARMS((int c));
void transchar_nonprint __PARMS((char_u *buf, int c));
int charsize __PARMS((register int c));
int strsize __PARMS((register char_u *s));
int chartabsize __PARMS((register int c, colnr_t col));
int win_chartabsize __PARMS((register WIN *wp, register int c, colnr_t col));
int linetabsize __PARMS((char_u *s));
int isidchar __PARMS((int c));
int iswordchar __PARMS((int c));
int isfilechar __PARMS((int c));
int isprintchar __PARMS((int c));
int lbr_chartabsize __PARMS((unsigned char *s, colnr_t col));
int win_lbr_chartabsize __PARMS((WIN *wp, unsigned char *s, colnr_t col, int *head));
void getvcol __PARMS((WIN *wp, FPOS *pos, colnr_t *start, colnr_t *cursor, colnr_t *end));
