/*	$OpenBSD: pcvt_mouse.c,v 1.1 2000/09/01 05:46:02 aaron Exp $ */

/*
 * Copyright (c) 2000 Jean-Baptiste Marchand, Julien Montagne and Jerome Verdon
 * 
 * All rights reserved.
 *
 * This code is for mouse console support under the pcvt console driver.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by
 *	Hellmuth Michaelis, Brian Dunford-Shore, Joerg Wunsch, Scott Turner
 *	and Charles Hannum.
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */
 
#include "pcvt_hdr.h"

void mouse_moverel(char dx, char dy);

void inverse_char(unsigned short c);
void inverse_region(unsigned short start, unsigned short end);
void order_region(void);

unsigned char skip_spc_right(char);
unsigned char skip_spc_left(void);
unsigned char skip_char_right(void);
unsigned char skip_char_left(void);

void mouse_copy_start(void);
void mouse_copy_end(void);
void mouse_copy_word(void);
void mouse_copy_line(void);
void mouse_copy_extend(void);
void remove_selection(void);
void mouse_copy_selection(void);
void mouse_paste(void);
uid_t current_uid(void);

void mouse_zaxis(int z);
void mouse_button(int button, int clicks);

#define NO_BORDER 0
#define BORDER 1

#define MAXCOL (vsp->maxcol - 1)
#define MAXROW (vsp->screen_rows - 1)

#define XY_TO_X(col,row) (((row) * (vsp->maxcol)) + (col))
#define XABS_TO_XREL(col) (((col) - vsp->Crtat) % vsp->maxcol)

#define IS_ALPHANUM(pos) ((*(vsp->Crtat + (pos)) & 0x00ff) != ' ')
#define IS_SPACE(c) ((c) == ' ')

int
mouse_ioctl(Dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int device = minor(dev);
	mouse_info_t mouse_infos = *(mouse_info_t *) data; 	
	unsigned char c;
	video_state  *cs;

	if (device == PCVTCTL_MINOR && cmd == PCVT_MOUSECTL) {
		switch (mouse_infos.operation) {
		case MOUSE_INIT: 
			for (c = 0; c < PCVT_NSCREENS; c++) { 
				cs = &vs[c];
				cs->mouse = (cs->maxcol *
				    cs->screen_rows) / 2;
				cs->mouse_flags = 0;
			}
			Paste_avail = 0;
			break;

		case MOUSE_HIDE:
			remove_selection();
			mouse_hide();
			break;

		case MOUSE_MOTION_EVENT :
			mouse_moverel(mouse_infos.u.data.x,
			    mouse_infos.u.data.y);
			if (mouse_infos.u.data.z)
				mouse_zaxis(mouse_infos.u.data.z);
			break;

		case MOUSE_BUTTON_EVENT :
			mouse_button(mouse_infos.u.event.id,
			    mouse_infos.u.event.value);
			break;

		default:
			return 0;
		}
		return 0;
	}
	return (-1); /* continue treatment in pcioctl */
}

void
mouse_hide(void)
{
	if (IS_MOUSE_VISIBLE(vsp)) {
		inverse_char(vsp->mouse);
		vsp->mouse_flags &= ~MOUSE_VISIBLE;
	}
}

/*
 * function to move the cursor to a relative new position
 */

void
mouse_moverel(char dx, char dy)
{
	unsigned short old_mouse = vsp->mouse;
	unsigned char mouse_col = (vsp->mouse % vsp->maxcol);
	unsigned char mouse_row = (vsp->mouse / vsp->maxcol);
	
	if (scrnsv_active) /* if the screen saver is active */
		pcvt_scrnsv_reset();		/* unblank NOW !	*/

	/* update position */
	if (mouse_col + dx >= MAXCOL)
		mouse_col = MAXCOL;
	else
		if (mouse_col + dx <= 0)
			mouse_col = 0;
		else
			mouse_col += dx;
	if (mouse_row + dy >= MAXROW)
		mouse_row = MAXROW;
	else
		if (mouse_row + dy <= 0)
			mouse_row = 0;
		else
			mouse_row += dy;
	vsp->mouse = XY_TO_X(mouse_col, mouse_row);

	/* if we have moved */
	if (old_mouse != vsp->mouse) {
		/* hide the previous cursor, if not in a selection */
		if (IS_MOUSE_VISIBLE(vsp) && (!IS_SEL_IN_PROGRESS(vsp)))
			inverse_char(old_mouse);
		
		if (IS_SEL_IN_PROGRESS(vsp)) {
			/* selection in progress */
			mouse_copy_extend();
		}
		else {
			inverse_char(vsp->mouse);
			vsp->mouse_flags |= MOUSE_VISIBLE;
		}
	}
}
	
/* 
 * function to video inverse a character of the display
 */

void
inverse_char(unsigned short pos)
{
	u_short *current_char = vsp->Crtat + pos;
	u_short inverse = *current_char;
	
	if ((inverse >> 8) == 0)
		inverse = (FG_LIGHTGREY << 8);

	inverse = (((inverse >> 8) & 0x88) | ((((inverse >> 8) >> 4) |
	    ((inverse >> 8) << 4)) & 0x77)) << 8;

	*current_char = inverse | ((*current_char) & 0x00ff);
}

/*
 * Function to order extremum of the selection region
 */

void
order_region(void)
{
	unsigned short max_pos; 
	
	if (vsp->cpy_start > vsp->cpy_end) {
		max_pos = vsp->cpy_start;
		vsp->cpy_start = vsp->cpy_end;
		vsp->cpy_end = max_pos;
	}
}

/*
 * Function to video inverse a region of the display
 * start must be inferior to end
 */

void
inverse_region(unsigned short start, unsigned short end)
{
	unsigned short current_pos;
	unsigned char c = 0;
	
	current_pos = start;
	while (current_pos <= end) {
		inverse_char(current_pos++);
		c++;
	}
}
	
/*
 * Function which returns the number of contiguous blank characters between 
 * the right margin if border == 1 or between the next non-blank character
 * and the current mouse cursor if border == 0
 */

unsigned char
skip_spc_right(char border)
{
	unsigned short *current;	
	unsigned short *limit;
	unsigned char mouse_col = (vsp->mouse % vsp->maxcol);
	unsigned char res = 0;
		
	current = vsp->Crtat + vsp->cpy_end;
	limit = current + (vsp->maxcol - mouse_col - 1); 
	while (((*current & 0x00ff) == ' ') && (current <= limit)) {
		current++;
		res++;
	}
	if (border == BORDER) {
		if (current > limit)
			return (res - 1);
		else
			return (0);
	}
	else
		return (res - 1);	
}

/* 
 * Function which returns the number of contiguous blank characters between
 * the first of the contiguous blank characters and the current mouse cursor
 */

unsigned char
skip_spc_left(void)
{
	unsigned short *current;	
	unsigned short *limit;
	unsigned char mouse_col = (vsp->mouse % vsp->maxcol);
	unsigned char res = 0;
	
	current = vsp->Crtat + vsp->cpy_start;
	limit = current - mouse_col;
	while (((*current & 0x00ff) == ' ') && (current >= limit)) {
		current--;
		res++;
	}

	if (res)
		res--;
	return (res);
}

/* 
 * Function which find the first blank beginning after the current cursor
 * position
 */

unsigned char
skip_char_right(void)
{
	unsigned short *current;	
	unsigned short *limit;
	unsigned char mouse_col = (vsp->mouse % vsp->maxcol);
	unsigned char res = 0;
		
	current = vsp->Crtat + vsp->cpy_end;
	limit = current + (vsp->maxcol - mouse_col - 1); 
	while (((*current & 0x00ff) != ' ') && (current <= limit)) {
		current++;
		res++;
	}

	if (res)
		res--;
	return (res);
}

/*
 * Function which find the first non-blank character before the cursor position
 */

unsigned char
skip_char_left(void)
{
	unsigned short *current;	
	unsigned short *limit;
	unsigned char mouse_col = (vsp->mouse % vsp->maxcol);
	unsigned char res = 0;
	
	current = vsp->Crtat + vsp->cpy_start;
	limit = current - mouse_col;
	while (((*current & 0x00ff) != ' ') && (current >= limit)) {
		current--;
		res++;
	}

	if (res)
		res--;
	return (res);
}

/*
 * Function to handle beginning of a copy operation
 */

void
mouse_copy_start(void)
{
#if 0
	unsigned char right;
	unsigned char left;
#endif

	/* if no selection, then that's the first one */
	if (!Paste_avail)
		Paste_avail = 1;
	
	/* remove the previous selection */
	if (IS_SEL_EXISTS(vsp)) {
		remove_selection();
	}

	/* initial show of the cursor */
	if (!IS_MOUSE_VISIBLE(vsp))
		inverse_char(vsp->mouse);
		
	vsp->cpy_start = vsp->mouse;
	vsp->cpy_end = vsp->mouse;

#if 0
	/* if there is a blank region, we select all the region */
	if ((right = skip_spc_right(BORDER)) > 0) {
		left = skip_spc_left();
		vsp->cpy_start -= left;
		vsp->cpy_end += right;
		if (vsp->cpy_start % vsp->maxcol) {
			inverse_region(vsp->cpy_start, vsp->cpy_end);
			inverse_char(vsp->mouse); /* reverse back the cursor */
		}
		else {
			/* we select only one character on a full blank line */
			vsp->cpy_start = vsp->mouse;
			vsp->cpy_end = vsp->mouse;
		}
	}
#endif

	/* for correct action in remove_selection() */
	vsp->cpy_orig = vsp->cpy_start;

	vsp->mouse_flags |= SEL_IN_PROGRESS;
	vsp->mouse_flags |= SEL_EXISTS;

	/* mouse cursor hidden in the selection */
	vsp->mouse_flags &= ~MOUSE_VISIBLE;
}

/*
 * Function to handle copy of the word under the cursor 
 */

void
mouse_copy_word()
{
	unsigned char right;
	unsigned char left;
	
	if (IS_SEL_EXISTS(vsp))
		remove_selection();

	if (IS_SEL_IN_PROGRESS(vsp))
		vsp->mouse_flags &= ~SEL_IN_PROGRESS;

	if (IS_MOUSE_VISIBLE(vsp))
		inverse_char(vsp->mouse);

	vsp->cpy_start = vsp->mouse;
	vsp->cpy_end = vsp->mouse;

	if (IS_ALPHANUM(vsp->mouse)) {
		right = skip_char_right();
		left = skip_char_left();
	}
	else {
		right = skip_spc_right(NO_BORDER);
		left = skip_spc_left();
	}

	vsp->cpy_start -= left;
	vsp->cpy_end += right;
	vsp->cpy_orig = vsp->cpy_start;
	inverse_region(vsp->cpy_start, vsp->cpy_end);
	vsp->mouse_flags |= SEL_EXISTS;

	/* mouse cursor hidden in the selection */
	vsp->mouse_flags &= ~MOUSE_VISIBLE;
}

/* 
 * Function to handle copy of the current line
 */

void 
mouse_copy_line(void)
{
	unsigned char row = vsp->mouse / vsp->maxcol;
	
	if (IS_SEL_EXISTS(vsp))
		remove_selection();

	if (IS_SEL_IN_PROGRESS(vsp))
		vsp->mouse_flags &= ~(SEL_IN_PROGRESS);

	if (IS_MOUSE_VISIBLE(vsp))
		inverse_char(vsp->mouse);

	vsp->cpy_start = row * vsp->maxcol;
	vsp->cpy_end = vsp->cpy_start + MAXCOL;
	vsp->cpy_orig = vsp->cpy_start;
	inverse_region(vsp->cpy_start, vsp->cpy_end);

	vsp->mouse_flags |= SEL_EXISTS;
	vsp->mouse_flags &= ~MOUSE_VISIBLE;
}

/*
 * Function to handle the end of a copy operation
 */

void 
mouse_copy_end(void)
{
	vsp->mouse_flags &= ~(SEL_IN_PROGRESS);
}

/* 
 * Function to extend a previously selected region
 */

void
mouse_copy_extend()
{
	if (IS_SEL_EXISTS(vsp)) {
		if (vsp->mouse < vsp->cpy_start) {
			/* we are over the initial selection position */	

			/* reverse the old selection region */
			if (IS_SEL_EXISTS(vsp)) 
				remove_selection();

			/* new delimiters for the selection zone */
			vsp->cpy_start = vsp->mouse;
			vsp->cpy_end = vsp->cpy_orig - 1; 
			inverse_char(vsp->cpy_end);
		}
		if (vsp->cpy_start < vsp->cpy_orig && vsp->mouse >= vsp->cpy_orig)
		{
			/* we go back behind the initial selection
			   position */

			/* reverse the old selection region */
			if (IS_SEL_EXISTS(vsp)) 
				remove_selection();

			vsp->cpy_start = vsp->cpy_orig;
			vsp->cpy_end = vsp->cpy_orig;
			inverse_char(vsp->cpy_start);
		}

		if (vsp->mouse < vsp->cpy_end) {
			/* reducing selection */
			if (vsp->mouse >= vsp->cpy_orig) 
				inverse_region(vsp->mouse + 1, vsp->cpy_end);
			else
				inverse_region(vsp->mouse, vsp->cpy_end - 1);
		}
		else {
			/* extending selection */
			if (vsp->mouse >= vsp->cpy_orig)  
				inverse_region(vsp->cpy_end + 1, vsp->mouse);
			else
				inverse_region(vsp->cpy_end, vsp->mouse - 1);
		}
		vsp->cpy_end = vsp->mouse;
		order_region();	
	}
	else {
		/* no selection yet! */
		sysbeep(PCVT_SYSBEEPF / 1500, hz / 4);
	}
}

/*
 * Function to remove a previously selected region
 */

void
remove_selection()
{
	if (vsp->cpy_start < vsp->cpy_orig) 
		/* backward selection */
		inverse_region(vsp->cpy_end, vsp->cpy_orig - 1);
	else
		/* forward selection */
		inverse_region(vsp->cpy_start, vsp->cpy_end);
}

/*
 * Function to get the uid of the user behind the *shell* on the current tty
 * We handle su and sudo cases...
 */

uid_t
current_uid(void)
{
	pid_t pg; /* process group id */
	struct proc *owner_proc;
	
	pg = vsp->vs_tty->t_pgrp->pg_id;
	owner_proc = pfind(pg);
	if (!owner_proc) {
		Paste_avail = 0; /* this selection will never be available
				    because the process doesn't exist... */
		return (0); /* the uid of root, just to be sure */
	}
	
	/* 
	 * We use the real user id and not the *effective* one, as a foreground
	 * setuid process could permit to paste selection of another user
	 */
	
	return (owner_proc->p_cred->p_ruid);
}

/* 
 * Function to put the current visual selection in the selection buffer
 */

void
mouse_copy_selection(void)
{
	unsigned short current = 0;
	unsigned short blank = current;
	unsigned short buf_end = ((vsp->maxcol + 1) * vsp->screen_rows);
	unsigned short *sel_cur;
	unsigned short *sel_end;
		
	if (vsp->cpy_start < vsp->cpy_orig) {
		/* backward selection */
		sel_cur = vsp->Crtat + vsp->cpy_end;
		sel_end = vsp->Crtat + vsp->cpy_orig - 1;
	}
	else {
		/* forward selection */
		sel_cur = vsp->Crtat + vsp->cpy_start;
		sel_end = vsp->Crtat + vsp->cpy_end;
	}
	while (sel_cur <= sel_end && current < buf_end - 1) {
		Copybuffer[current] = ((*sel_cur) & 0x00ff);

		if (!IS_SPACE(Copybuffer[current])) 
			blank = current + 1; /* first blank after non-blank */
		current++;

		if (XABS_TO_XREL(sel_cur) == MAXCOL) {
			/* we are on the last col of the screen */
			Copybuffer[blank] = '\r'; /* carriage return */
			current = blank + 1; /* restart just after the carriage
					       return in the buffer */
			blank = current;
		}
		sel_cur++;
	}
	
	Copybuffer[current] = '\0';

#if 0
	/* 
	   examine the last line of selection to see if it finishes with blank.
	   In that case, we put a carriage return after the last non-blank
	   character in the buffer 
	 */
	sel_cur = sel_end;
	while (IS_SPACE(*sel_cur & 0x00ff) && XABS_TO_XREL(sel_cur) < MAXCOL) {
		sel_cur--;
		current--;
	}
	if (sel_cur != sel_end) {
		Copybuffer[current++] = '\r';
		Copybuffer[current] = '\0';
	}	
#endif
	Copyowner = current_uid();
}
/*
 * Function to paste the current selection
 */

void
mouse_paste(void)
{
	unsigned short len;
	char *current = Copybuffer;
	uid_t cur_uid;

	cur_uid = current_uid();	
	if (Paste_avail && ((cur_uid == Copyowner) || !cur_uid)) {
		/* either the owner of the selection or root */
		len = strlen(Copybuffer);
		for (; len > 0; len--) {
			(*linesw[vsp->vs_tty->t_line].l_rint)
				(*current++, vsp->vs_tty);
		}
	}
	else 
		sysbeep(PCVT_SYSBEEPF / 1500, hz / 4);
}

/* 
 * Function to handle button clicks 
 */

void
mouse_button(int button, int clicks)
{
	if (scrnsv_active) /* if the screen saver is active */
		pcvt_scrnsv_reset();		/* unblank NOW !	*/
	
	switch (button) {
	case MOUSE_COPY_BUTTON:
		switch (clicks % 4) {
		case 0: /* button is up */
			mouse_copy_end();
			mouse_copy_selection();
			break;
		case 1: /* single click */
			mouse_copy_start();
			mouse_copy_selection();
			break;
		case 2: /* double click */
			mouse_copy_word();
			mouse_copy_selection();
			break;
		case 3: /* triple click */
			mouse_copy_line();
			mouse_copy_selection();
			break;
		default:
			break;
		}
		break;

	case MOUSE_PASTE_BUTTON:
		switch (clicks) {
		case 0: /* button is up */
			break;
		default: /* paste */
			mouse_paste();
			break;
		}
		break;

	case MOUSE_EXTEND_BUTTON:
		switch (clicks) {
		case 0: /* button is up */
			break;
		default: /* extend the selection */
			mouse_copy_extend();
			mouse_copy_selection();
			break;
		}
		break;

	default:
		break;
	}
}

/*
 * Function to handle the z axis 
 * The z axis (roller or wheel) is mapped by default to scrollback
 */

void
mouse_zaxis(int z)
{
	scrollback_mouse(z);
}	
