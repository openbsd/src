/*	$OpenBSD: message.pro,v 1.1.1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* message.c */
int msg __PARMS((char_u *s));
int emsg __PARMS((char_u *s));
int emsg2 __PARMS((char_u *s, char_u *a1));
int emsgn __PARMS((char_u *s, long n));
int msg_trunc __PARMS((char_u *s));
void wait_return __PARMS((int redraw));
void msg_start __PARMS((void));
void msg_pos __PARMS((int row, int col));
void msg_outchar __PARMS((int c));
void msg_outnum __PARMS((long n));
void msg_home_replace __PARMS((char_u *fname));
int msg_outtrans __PARMS((register char_u *str));
int msg_outtrans_len __PARMS((register char_u *str, register int len));
int msg_outtrans_special __PARMS((register char_u *str, register int all));
void msg_prt_line __PARMS((char_u *s));
void msg_outstr __PARMS((char_u *s));
void msg_moremsg __PARMS((int full));
void msg_clr_eos __PARMS((void));
int msg_end __PARMS((void));
int msg_check __PARMS((void));
