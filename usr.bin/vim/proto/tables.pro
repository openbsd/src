/*	$OpenBSD: tables.pro,v 1.1.1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* tables.c */
int name_to_mod_mask __PARMS((int c));
int check_shifted_spec_key __PARMS((int c));
int unshift_special_key __PARMS((char_u *p));
char_u *get_special_key_name __PARMS((int c, int modifiers));
int find_special_key_in_table __PARMS((int c));
int get_special_key_code __PARMS((char_u *name));
char_u *get_key_name __PARMS((int i));
int get_mouse_button __PARMS((int code, int *is_click, int *is_drag));
int get_pseudo_mouse_code __PARMS((int button, int is_click, int is_drag));
