/*	$OpenBSD: tables.c,v 1.2 1996/09/21 06:23:20 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *							This file by Robert Webb
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * tables.c: functions that use lookup tables for various things, generally to
 * do with special key codes.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

/*
 * Some useful tables.
 */

static struct
{
	int		mod_mask;		/* Bit-mask for particular key modifier */
	char_u	name;			/* Single letter name of modifier */
} mod_mask_table[] =
{
	{MOD_MASK_ALT,		(char_u)'M'},
	{MOD_MASK_CTRL,		(char_u)'C'},
	{MOD_MASK_SHIFT,	(char_u)'S'},
	{MOD_MASK_2CLICK,	(char_u)'2'},
	{MOD_MASK_3CLICK,	(char_u)'3'},
	{MOD_MASK_4CLICK,	(char_u)'4'},
	{0x0,				NUL}
};

/*
 * Shifted key terminal codes and their unshifted equivalent.
 * Don't add mouse codes here, they are handled seperately!
 */
static char_u shifted_keys_table[] =
{
/*  shifted     			unshifted  */
	'&', '9',				'@', '1',			/* begin */
	'&', '0',				'@', '2',			/* cancel */
	'*', '1',				'@', '4',			/* command */
	'*', '2',				'@', '5',			/* copy */
	'*', '3',				'@', '6',			/* create */
	'*', '4',				'k', 'D',			/* delete char */
	'*', '5',				'k', 'L',			/* delete line */
	'*', '7',				'@', '7',			/* end */
	'*', '9',				'@', '9',			/* exit */
	'*', '0',				'@', '0',			/* find */
	'#', '1',				'%', '1',			/* help */
	'#', '2',				'k', 'h',			/* home */
	'#', '3',				'k', 'I',			/* insert */
	'#', '4',				'k', 'l',			/* left arrow */
	'%', 'a',				'%', '3',			/* message */
	'%', 'b',				'%', '4',			/* move */
	'%', 'c',				'%', '5',			/* next */
	'%', 'd',				'%', '7',			/* options */
	'%', 'e',				'%', '8',			/* previous */
	'%', 'f',				'%', '9',			/* print */
	'%', 'g',				'%', '0',			/* redo */
	'%', 'h',				'&', '3',			/* replace */
	'%', 'i',				'k', 'r',			/* right arrow */
	'%', 'j',				'&', '5',			/* resume */
	'!', '1',				'&', '6',			/* save */
	'!', '2',				'&', '7',			/* suspend */
	'!', '3',				'&', '8',			/* undo */
	KS_EXTRA, KE_S_UP,		'k', 'u',			/* up arrow */
	KS_EXTRA, KE_S_DOWN,    'k', 'd',			/* down arrow */

	KS_EXTRA, KE_S_F1,      'k', '1',    		/* F1 */
	KS_EXTRA, KE_S_F2,      'k', '2',
	KS_EXTRA, KE_S_F3,      'k', '3',
	KS_EXTRA, KE_S_F4,      'k', '4',
	KS_EXTRA, KE_S_F5,      'k', '5',
	KS_EXTRA, KE_S_F6,      'k', '6',
	KS_EXTRA, KE_S_F7,      'k', '7',
	KS_EXTRA, KE_S_F8,      'k', '8',
	KS_EXTRA, KE_S_F9,      'k', '9',
	KS_EXTRA, KE_S_F10,     'k', ';',			/* F10 */

	KS_EXTRA, KE_S_F11,     'F', '1',
	KS_EXTRA, KE_S_F12,     'F', '2',
	KS_EXTRA, KE_S_F13,     'F', '3',
	KS_EXTRA, KE_S_F14,     'F', '4',
	KS_EXTRA, KE_S_F15,     'F', '5',
	KS_EXTRA, KE_S_F16,     'F', '6',
	KS_EXTRA, KE_S_F17,     'F', '7',
	KS_EXTRA, KE_S_F18,     'F', '8',
	KS_EXTRA, KE_S_F19,     'F', '9',
	KS_EXTRA, KE_S_F20,     'F', 'A',

	KS_EXTRA, KE_S_F21,     'F', 'B',
	KS_EXTRA, KE_S_F22,     'F', 'C',
	KS_EXTRA, KE_S_F23,     'F', 'D',
	KS_EXTRA, KE_S_F24,     'F', 'E',
	KS_EXTRA, KE_S_F25,     'F', 'F',
	KS_EXTRA, KE_S_F26,     'F', 'G',
	KS_EXTRA, KE_S_F27,     'F', 'H',
	KS_EXTRA, KE_S_F28,     'F', 'I',
	KS_EXTRA, KE_S_F29,     'F', 'J',
	KS_EXTRA, KE_S_F30,     'F', 'K',

	KS_EXTRA, KE_S_F31,     'F', 'L',
	KS_EXTRA, KE_S_F32,     'F', 'M',
	KS_EXTRA, KE_S_F33,     'F', 'N',
	KS_EXTRA, KE_S_F34,     'F', 'O',
	KS_EXTRA, KE_S_F35,     'F', 'P',

	KS_EXTRA, KE_S_TAB,     KS_EXTRA, KE_TAB,	/* TAB */
	NUL
};

static struct key_name_entry
{
	int		key;		/* Special key code or ascii value */
	char_u	*name;		/* Name of key */
} key_names_table[] =
{
	{' ',				(char_u *)"Space"},
	{TAB,				(char_u *)"Tab"},
	{K_TAB,				(char_u *)"Tab"},
	{NL,				(char_u *)"NL"},
	{NL,				(char_u *)"NewLine"},	/* Alternative name */
	{NL,				(char_u *)"LineFeed"},	/* Alternative name */
	{NL,				(char_u *)"LF"},		/* Alternative name */
	{CR,				(char_u *)"CR"},
	{CR,				(char_u *)"Return"},	/* Alternative name */
	{ESC,				(char_u *)"Esc"},
	{'|',				(char_u *)"Bar"},
	{K_UP,				(char_u *)"Up"},
	{K_DOWN,			(char_u *)"Down"},
	{K_LEFT,			(char_u *)"Left"},
	{K_RIGHT,			(char_u *)"Right"},

	{K_F1,	 			(char_u *)"F1"},
	{K_F2,	 			(char_u *)"F2"},
	{K_F3,	 			(char_u *)"F3"},
	{K_F4,	 			(char_u *)"F4"},
	{K_F5,	 			(char_u *)"F5"},
	{K_F6,	 			(char_u *)"F6"},
	{K_F7,	 			(char_u *)"F7"},
	{K_F8,	 			(char_u *)"F8"},
	{K_F9,	 			(char_u *)"F9"},
	{K_F10,				(char_u *)"F10"},

	{K_F11,				(char_u *)"F11"},
	{K_F12,				(char_u *)"F12"},
	{K_F13,				(char_u *)"F13"},
	{K_F14,				(char_u *)"F14"},
	{K_F15,				(char_u *)"F15"},
	{K_F16,				(char_u *)"F16"},
	{K_F17,				(char_u *)"F17"},
	{K_F18,				(char_u *)"F18"},
	{K_F19,				(char_u *)"F19"},
	{K_F20,				(char_u *)"F20"},

	{K_F21,				(char_u *)"F21"},
	{K_F22,				(char_u *)"F22"},
	{K_F23,				(char_u *)"F23"},
	{K_F24,				(char_u *)"F24"},
	{K_F25,				(char_u *)"F25"},
	{K_F26,				(char_u *)"F26"},
	{K_F27,				(char_u *)"F27"},
	{K_F28,				(char_u *)"F28"},
	{K_F29,				(char_u *)"F29"},
	{K_F30,				(char_u *)"F30"},

	{K_F31,				(char_u *)"F31"},
	{K_F32,				(char_u *)"F32"},
	{K_F33,				(char_u *)"F33"},
	{K_F34,				(char_u *)"F34"},
	{K_F35,				(char_u *)"F35"},

	{K_HELP,			(char_u *)"Help"},
	{K_UNDO,			(char_u *)"Undo"},
	{K_BS,				(char_u *)"BS"},
	{K_BS,				(char_u *)"BackSpace"},	/* Alternative name */
	{K_INS,				(char_u *)"Insert"},
	{K_INS,				(char_u *)"Ins"},		/* Alternative name */
	{K_DEL,				(char_u *)"Del"},
	{K_DEL,				(char_u *)"Delete"},	/* Alternative name */
	{K_HOME,			(char_u *)"Home"},
	{K_END,				(char_u *)"End"},
	{K_PAGEUP,			(char_u *)"PageUp"},
	{K_PAGEDOWN,		(char_u *)"PageDown"},
	{K_KHOME,			(char_u *)"kHome"},
	{K_KEND,			(char_u *)"kEnd"},
	{K_KPAGEUP,			(char_u *)"kPageUp"},
	{K_KPAGEDOWN,		(char_u *)"kPageDown"},

	{K_MOUSE,			(char_u *)"Mouse"},
	{K_LEFTMOUSE,		(char_u *)"LeftMouse"},
	{K_LEFTDRAG,		(char_u *)"LeftDrag"},
	{K_LEFTRELEASE,		(char_u *)"LeftRelease"},
	{K_MIDDLEMOUSE,		(char_u *)"MiddleMouse"},
	{K_MIDDLEDRAG,		(char_u *)"MiddleDrag"},
	{K_MIDDLERELEASE,	(char_u *)"MiddleRelease"},
	{K_RIGHTMOUSE,		(char_u *)"RightMouse"},
	{K_RIGHTDRAG,		(char_u *)"RightDrag"},
	{K_RIGHTRELEASE,	(char_u *)"RightRelease"},
	{K_ZERO,			(char_u *)"Nul"},
	{0,					NULL}
};

#define KEY_NAMES_TABLE_LEN (sizeof(key_names_table) / sizeof(struct key_name_entry))

#ifdef USE_MOUSE
static struct
{
	int		pseudo_code;		/* Code for pseudo mouse event */
	int		button;				/* Which mouse button is it? */
	int		is_click;			/* Is it a mouse button click event? */
	int		is_drag;			/* Is it a mouse drag event? */
} mouse_table[] =
{
	{KE_LEFTMOUSE,		MOUSE_LEFT,		TRUE,	FALSE},
	{KE_LEFTDRAG,		MOUSE_LEFT,		FALSE,	TRUE},
	{KE_LEFTRELEASE,	MOUSE_LEFT,		FALSE,	FALSE},
	{KE_MIDDLEMOUSE,	MOUSE_MIDDLE,	TRUE,	FALSE},
	{KE_MIDDLEDRAG,		MOUSE_MIDDLE,	FALSE,	TRUE},
	{KE_MIDDLERELEASE,	MOUSE_MIDDLE,	FALSE,	FALSE},
	{KE_RIGHTMOUSE,		MOUSE_RIGHT,	TRUE,	FALSE},
	{KE_RIGHTDRAG,		MOUSE_RIGHT,	FALSE,	TRUE},
	{KE_RIGHTRELEASE,	MOUSE_RIGHT,	FALSE,	FALSE},
	{KE_IGNORE,			MOUSE_RELEASE,	FALSE,	TRUE},	/* DRAG without CLICK */
	{KE_IGNORE,			MOUSE_RELEASE,	FALSE,	FALSE}, /* RELEASE without CLICK */
	{0,					0,				0,		0},
};
#endif /* USE_MOUSE */

/*
 * Return the modifier mask bit (MOD_MASK_*) which corresponds to the given
 * modifier name ('S' for Shift, 'C' for Ctrl etc).
 */
	int
name_to_mod_mask(c)
	int		c;
{
	int		i;

	for (i = 0; mod_mask_table[i].mod_mask; i++)
		if (TO_LOWER(c) == TO_LOWER(mod_mask_table[i].name))
			return mod_mask_table[i].mod_mask;
	return 0x0;
}

/*
 * Decide whether the given key code (K_*) is a shifted special
 * key (by looking at mod_mask).  If it is, then return the appropriate shifted
 * key code, otherwise just return the character as is.
 */
	int
check_shifted_spec_key(c)
	int		c;
{
	int		i;
	int		key0;
	int		key1;

	if (mod_mask & MOD_MASK_SHIFT)
	{
		if (c == TAB)			/* TAB is not in the table, K_TAB is */
			return K_S_TAB;
		key0 = KEY2TERMCAP0(c);
		key1 = KEY2TERMCAP1(c);
		for (i = 0; shifted_keys_table[i] != NUL; i += 4)
			if (key0 == shifted_keys_table[i + 2] &&
											key1 == shifted_keys_table[i + 3])
				return TERMCAP2KEY(shifted_keys_table[i],
												   shifted_keys_table[i + 1]);
	}
	return c;
}

/*
 * Decide whether the given special key is shifted or not.  If it is we
 * return OK and change it to the equivalent unshifted special key code,
 * otherwise we leave it as is and return FAIL.
 */
	int
unshift_special_key(p)
	char_u	*p;
{
	int		i;

	for (i = 0; shifted_keys_table[i]; i += 4)
		if (p[0] == shifted_keys_table[i] && p[1] == shifted_keys_table[i + 1])
		{
			p[0] = shifted_keys_table[i + 2];
			p[1] = shifted_keys_table[i + 3];
			return OK;
		}
	return FAIL;
}

/*
 * Return a string which contains the name of the given key when the given
 * modifiers are down.
 */
	char_u *
get_special_key_name(c, modifiers)
	int		c;
	int		modifiers;
{
	static char_u string[MAX_KEY_NAME_LEN + 1];

	int		i, idx;
	char_u	*s;
	char_u	name[2];

	string[0] = '<';
	idx = 1;

	/* translate shifted keys into unshifted keys and set modifier */
	if (IS_SPECIAL(c))
	{
		name[0] = KEY2TERMCAP0(c);
		name[1] = KEY2TERMCAP1(c);
		if (unshift_special_key(&name[0]))
			modifiers |= MOD_MASK_SHIFT;
		c = TERMCAP2KEY(name[0], name[1]);
	}

	/* translate the modifier into a string */
	for (i = 0; mod_mask_table[i].mod_mask; i++)
		if (modifiers & mod_mask_table[i].mod_mask)
		{
			string[idx++] = mod_mask_table[i].name;
			string[idx++] = (char_u)'-';
		}

	/* try to find the key in the special key table */
	i = find_special_key_in_table(c);
	if (i < 0)			/* unknown special key, output t_xx */
	{
		if (IS_SPECIAL(c))
		{
			string[idx++] = 't';
			string[idx++] = '_';
			string[idx++] = KEY2TERMCAP0(c);
			string[idx++] = KEY2TERMCAP1(c);
		}
		/* Not a special key, only modifiers, output directly */
		else
		{
			if (isprintchar(c))
				string[idx++] = c;
			else
			{
				s = transchar(c);
				while (*s)
					string[idx++] = *s++;
			}
		}
	}
	else				/* use name of special key */
	{
		STRCPY(string + idx, key_names_table[i].name);
		idx = STRLEN(string);
	}
	string[idx++] = '>';
	string[idx] = NUL;
	return string;
}

/*
 * Try to find key "c" in the special key table.
 * Return the index when found, -1 when not found.
 */
	int
find_special_key_in_table(c)
	int		c;
{
	int		i;

	for (i = 0; key_names_table[i].name != NULL; i++)
		if (c == key_names_table[i].key)
			break;
	if (key_names_table[i].name == NULL)
		i = -1;
	return i;
}

/*
 * Find the special key with the given name (the given string does not have to
 * end with NUL, the name is assumed to end before the first non-idchar).
 * If the name starts with "t_" the next two characters are interpreted as a
 * termcap name.
 * Return the key code, or 0 if not found.
 */
	int
get_special_key_code(name)
	char_u	*name;
{
	char_u	*table_name;
	char_u	string[3];
	int		i, j;

	/*
	 * If it's <t_xx> we get the code for xx from the termcap
	 */
	if (name[0] == 't' && name[1] == '_' && name[2] != NUL && name[3] != NUL)
	{
		string[0] = name[2];
		string[1] = name[3];
		string[2] = NUL;
		if (add_termcap_entry(string, FALSE) == OK)
			return TERMCAP2KEY(name[2], name[3]);
	}
	else
		for (i = 0; key_names_table[i].name != NULL; i++)
		{
			table_name = key_names_table[i].name;
			for (j = 0; isidchar(name[j]) && table_name[j] != NUL; j++)
				if (TO_LOWER(table_name[j]) != TO_LOWER(name[j]))
					break;
			if (!isidchar(name[j]) && table_name[j] == NUL)
				return key_names_table[i].key;
		}
	return 0;
}

	char_u *
get_key_name(i)
	int		i;
{
	if (i >= KEY_NAMES_TABLE_LEN)
		return NULL;
	return  key_names_table[i].name;
}

#ifdef USE_MOUSE
/*
 * Look up the given mouse code to return the relevant information in the other
 * arguments.  Return which button is down or was released.
 */
	int
get_mouse_button(code, is_click, is_drag)
	int		code;
	int		*is_click;
	int		*is_drag;
{
	int		i;

	for (i = 0; mouse_table[i].pseudo_code; i++)
		if (code == mouse_table[i].pseudo_code)
		{
			*is_click = mouse_table[i].is_click;
			*is_drag = mouse_table[i].is_drag;
			return mouse_table[i].button;
		}
	return 0;		/* Shouldn't get here */
}

/*
 * Return the appropriate pseudo mouse event token (KE_LEFTMOUSE etc) based on
 * the given information about which mouse button is down, and whether the
 * mouse was clicked, dragged or released.
 */
	int
get_pseudo_mouse_code(button, is_click, is_drag)
	int		button;		/* eg MOUSE_LEFT */
	int		is_click;
	int		is_drag;
{
	int		i;

	for (i = 0; mouse_table[i].pseudo_code; i++)
		if (button == mouse_table[i].button
			&& is_click == mouse_table[i].is_click
			&& is_drag == mouse_table[i].is_drag)
		{
			return mouse_table[i].pseudo_code;
		}
	return KE_IGNORE;		/* not recongnized, ignore it */
}
#endif /* USE_MOUSE */
