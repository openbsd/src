#include <HTUtils.h>
#include <HTCJK.h>
#include <HTTP.h>
#include <HTAlert.h>
#include <LYCurses.h>
#include <GridText.h>
#include <LYCharSets.h>
#include <UCAux.h>
#include <LYGlobalDefs.h>
#include <LYUtils.h>
#include <LYStrings.h>
#include <LYKeymap.h>
#include <LYClean.h>

#include <LYLeaks.h>

#ifdef USE_COLOR_STYLE
#include <AttrList.h>
#include <LYHash.h>
#endif

#if defined(VMS) && !defined(USE_SLANG)
#define CTRL_W_HACK DO_NOTHING
#else
#define CTRL_W_HACK 23  /* CTRL-W refresh without clearok */
#endif /* VMS && !USE_SLANG */

PRIVATE int form_getstr PARAMS((
	struct link *	form_link,
	BOOLEAN		use_last_tfpos,
	BOOLEAN		redraw_only));

/*
 * Returns an array of pointers to the given list
 */
PRIVATE char ** options_list ARGS1(
	OptionType *,	opt_ptr)
{
    char **result = 0;
    size_t len;
    int pass;
    OptionType *tmp_ptr;

    for (pass = 0; pass < 2; pass++) {
	for (tmp_ptr = opt_ptr, len = 0; tmp_ptr != 0; tmp_ptr = tmp_ptr->next) {
	    if (pass != 0)
		result[len] = tmp_ptr->name;
	    len++;
	}
	if (pass == 0) {
	    len++;
	    result = typecallocn(char *, len);
	} else {
	    result[len] = 0;
	}
    }

    return result;
}

PUBLIC int change_form_link_ex ARGS8(
	struct link *,	form_link,
	document *,	newdoc,
	BOOLEAN *,	refresh_screen,
	char *,		link_name,
	char *,		link_value,
	BOOLEAN,	use_last_tfpos,
	BOOLEAN,	immediate_submit,
	BOOLEAN,	redraw_only)
{
    FormInfo *form = form_link->form;
    int newdoc_changed = 0;
    int c = DO_NOTHING;
    int OrigNumValue;
    char **my_data = 0;

    /*
     *  If there is no form to perform action on, don't do anything.
     */
    if (form == NULL) {
	return(c);
    }
    my_data = options_list(form->select_list);

    /*
     *  Move to the link position.
     */
    LYmove(form_link->ly, form_link->lx);

    switch(form->type) {
	case F_CHECKBOX_TYPE:
	    if (form->disabled == YES)
		break;
	    if (form->num_value) {
		form_link->hightext = unchecked_box;
		form->num_value = 0;
	    } else {
		form_link->hightext = checked_box;
		form->num_value = 1;
	    }
	    break;

	case F_OPTION_LIST_TYPE:
	    if (form->select_list == 0) {
		HTAlert(BAD_HTML_NO_POPUP);
		c = DO_NOTHING;
		break;
	    }

	    if (form->disabled == YES) {
		int dummy;
		dummy = LYhandlePopupList(form->num_value,
					  form_link->ly,
					  form_link->lx,
					  (CONST char **)my_data,
					  form->size,
					  form->size_l,
					  form->disabled,
					  FALSE,
					  FALSE);
#if CTRL_W_HACK != DO_NOTHING
		if (!enable_scrollback)
		    c = CTRL_W_HACK;  /* CTRL-W refresh without clearok */
		else
#endif
		    c = 12;  /* CTRL-L for repaint */
		break;
	    }
	    OrigNumValue = form->num_value;
	    form->num_value = LYhandlePopupList(form->num_value,
						form_link->ly,
						form_link->lx,
						(CONST char **)my_data,
						form->size,
						form->size_l,
						form->disabled,
						FALSE,
						FALSE);
	    {
		OptionType * opt_ptr = form->select_list;
		int i;
		for (i = 0; i < form->num_value; i++, opt_ptr = opt_ptr->next)
		    ; /* null body */
		/*
		 *  Set the name.
		 */
		form->value = opt_ptr->name;
		/*
		 *  Set the value.
		 */
		form->cp_submit_value = opt_ptr->cp_submit_value;
		 /*
		  *  Set charset in which we have the submit value. - kw
		  */
		form->value_cs = opt_ptr->value_cs;
	    }
#if CTRL_W_HACK != DO_NOTHING
	    if (!enable_scrollback)
		c = CTRL_W_HACK;	 /* CTRL-W refresh without clearok */
	    else
#endif
		c = 12;	 /* CTRL-L for repaint */
	    break;

	case F_RADIO_TYPE:
	    if (form->disabled == YES)
		break;
		/*
		 *  Radio buttons must have one and
		 *  only one down at a time!
		 */
	    if (form->num_value) {
		HTUserMsg(NEED_CHECKED_RADIO_BUTTON);
	    } else {
		int i;
		/*
		 *  Run though list of the links on the screen and
		 *  unselect any that are selected. :)
		 */
		lynx_start_radio_color ();
		for (i = 0; i < nlinks; i++) {
		    if (links[i].type == WWW_FORM_LINK_TYPE &&
			links[i].form->type == F_RADIO_TYPE &&
			links[i].form->number == form->number &&
			/*
			 *  If it has the same name and its on...
			 */
			!strcmp(links[i].form->name, form->name) &&
			links[i].form->num_value) {
			LYmove(links[i].ly, links[i].lx);
			LYaddstr(unchecked_radio);
			links[i].hightext = unchecked_radio;
		    }
		}
		lynx_stop_radio_color ();
		/*
		 *  Will unselect other button and select this one.
		 */
		HText_activateRadioButton(form);
		/*
		 *  Now highlight this one.
		 */
		form_link->hightext = checked_radio;
	    }
	    break;

	case F_FILE_TYPE:
	case F_TEXT_TYPE:
	case F_TEXTAREA_TYPE:
	case F_PASSWORD_TYPE:
	    c = form_getstr(form_link, use_last_tfpos, redraw_only);
	    if (form->type == F_PASSWORD_TYPE)
		form_link->hightext = STARS(strlen(form->value));
	    else
		form_link->hightext = form->value;
	    break;

	case F_RESET_TYPE:
	    if (form->disabled == YES)
		break;
	    HText_ResetForm(form);
	    *refresh_screen = TRUE;
	    break;

	case F_TEXT_SUBMIT_TYPE:
	    if (redraw_only) {
		c = form_getstr(form_link, use_last_tfpos, TRUE);
		break;
	    }
	    if (!immediate_submit)
		c = form_getstr(form_link, use_last_tfpos, FALSE);
	    if (form->disabled == YES &&
		(c == '\r' || c == '\n' || immediate_submit)) {
		if (peek_mouse_link() >= 0)
		    c = LAC_TO_LKC0(LYK_ACTIVATE);
		else
		    c = '\t';
		break;
	    }
	    /*
	     *  If immediate_submit is set, we didn't enter the line editor
	     *  above, and will now try to call HText_SubmitForm() directly.
	     *  If immediate_submit is not set, c is the lynxkeycode returned
	     *  from line editing.   Then if c indicates that a key was pressed
	     *  that means we should submit, but with some extra considerations
	     *  (i.e. NOCACHE, DOWNLOAD, different from simple Enter), or if
	     *  we should act on some *other* link selected with the mouse,
	     *  we'll just return c and leave it to mainloop() to do the
	     *  right thing; if everything checks out, it should call this
	     *  function again, with immediate_submit set.
	     *  If c indicates that line editing ended with Enter, we still
	     *  defer to mainloop() for further checking if the submit
	     *  action URL could require more checks than we do here.
	     *  Only in the remaining cases do we proceed to call
	     *  HText_SubmitForm() directly before returning. - kw
	     */
	    if (immediate_submit ||
		((c == '\r' || c == '\n' || c == LAC_TO_LKC0(LYK_SUBMIT)) &&
		 peek_mouse_link() == -1)) {
		form_link->hightext = form->value;
#ifdef TEXT_SUBMIT_CONFIRM_WANTED
		if (!immediate_submit && (c == '\r' || c == '\n') &&
		    !HTConfirmDefault(NO_SUBMIT_BUTTON_QUERY), YES) {
		    /* User was prompted and declined; if canceled with ^G
		     * let mainloop stay on this field, otherwise move on to
		     * the next field or link. - kw
		     */
		    if (HTLastConfirmCancelled())
			c = DO_NOTHING;
		    else
			c = LAC_TO_LKC(LYK_NEXT_LINK);
		    break;
		}
#endif
		if (!form->submit_action || *form->submit_action == '\0') {
		    HTUserMsg(NO_FORM_ACTION);
		    c = DO_NOTHING;
		    break;
		} else if (form->submit_method == URL_MAIL_METHOD && no_mail) {
		    HTAlert(FORM_MAILTO_DISALLOWED);
		    c = DO_NOTHING;
		    break;
		} else if (!immediate_submit &&
			   ((no_file_url &&
			     !strncasecomp(form->submit_action, "file:", 5)) ||
			    !strncasecomp(form->submit_action, "lynx", 4))) {
		    c = LAC_TO_LKC0(LYK_SUBMIT);
		    break;
		} else {
		    if (form->no_cache &&
			form->submit_method != URL_MAIL_METHOD) {
			LYforce_no_cache = TRUE;
			reloading = TRUE;
		    }
		    newdoc_changed =
			HText_SubmitForm(form, newdoc, link_name, form->value);
		}
		if (form->submit_method == URL_MAIL_METHOD) {
		    *refresh_screen = TRUE;
		} else {
		    /*
		     *  Returns new document URL.
		     */
		    newdoc->link = 0;
		    newdoc->internal_link = FALSE;
		}
		c = DO_NOTHING;
		break;
	    } else {
		form_link->hightext = form->value;
	    }
	    break;

	case F_SUBMIT_TYPE:
	case F_IMAGE_SUBMIT_TYPE:
	    if (form->disabled == YES)
		break;
	    if (form->no_cache &&
		form->submit_method != URL_MAIL_METHOD) {
		LYforce_no_cache = TRUE;
		reloading = TRUE;
	    }
	    newdoc_changed =
		HText_SubmitForm(form, newdoc, link_name, link_value);
	    if (form->submit_method == URL_MAIL_METHOD)
		*refresh_screen = TRUE;
	    else {
		/* returns new document URL */
		newdoc->link = 0;
		newdoc->internal_link = FALSE;
	    }
	    break;

    }

    if (newdoc_changed) {
	c = LKC_DONE;
    } else {
	/*
	 *  These flags may have been set in mainloop, anticipating that
	 *  a request will be submitted.  But if we haven't filled in
	 *  newdoc, that won't actually be the case, so unset them. - kw
	 */
	LYforce_no_cache = FALSE;
	reloading = FALSE;
    }
    FREE(my_data);
    return(c);
}

PUBLIC int change_form_link ARGS7(
	struct link *,	form_link,
	document *,	newdoc,
	BOOLEAN *,	refresh_screen,
	char *,		link_name,
	char *,		link_value,
	BOOLEAN,	use_last_tfpos,
	BOOLEAN,	immediate_submit)
{
    /*pass all our args and FALSE as last arg*/
    return change_form_link_ex(form_link,newdoc,refresh_screen,link_name,
	link_value,use_last_tfpos,immediate_submit, FALSE /*redraw_only*/ );
}

PRIVATE int LastTFPos = -1;	/* remember last text field position */

PRIVATE void LYSetLastTFPos ARGS1(
    int,	pos)
{
    LastTFPos = pos;
}

PRIVATE int form_getstr ARGS3(
	struct link *,	form_link,
	BOOLEAN,	use_last_tfpos,
	BOOLEAN,	redraw_only)
{
    FormInfo *form = form_link->form;
    char *value = form->value;
    int ch;
    int far_col;
    int max_length;
    int startcol, startline;
    BOOL HaveMaxlength = FALSE;
    int action, repeat;
    int last_xlkc = -1;
#ifdef SUPPORT_MULTIBYTE_EDIT
    BOOL refresh_mb = TRUE;
#endif

    EditFieldData MyEdit;
    BOOLEAN Edited = FALSE;		/* Value might be updated? */

    /*
     *  Get the initial position of the cursor.
     */
    LYGetYX(startline, startcol);
    if ((startcol + form->size) > (LYcols - 1))
	far_col = (LYcols - 1);
    else
	far_col = (startcol + form->size);

    /*
     *  Make sure the form field value does not exceed our buffer. - FM
     */
    max_length = ((form->maxlength > 0 &&
		   form->maxlength < sizeof(MyEdit.buffer)) ?
					    form->maxlength :
					    (sizeof(MyEdit.buffer) - 1));
    if (strlen(form->value) > (size_t)max_length) {
	/*
	 *  We can't fit the entire value into the editing buffer,
	 *  so enter as much of the tail as fits. - FM
	 */
	value += (strlen(form->value) - max_length);
	if (!form->disabled &&
	    !(form->submit_method == URL_MAIL_METHOD && no_mail)) {
	    /*
	     *  If we can edit it, report that we are using the tail. - FM
	     */
	    HTUserMsg(FORM_VALUE_TOO_LONG);
	    show_formlink_statusline(form, redraw_only? FOR_PANEL : FOR_INPUT);
	    LYmove(startline, startcol);
	}
    }

    /*
     *  Print panned line
     */
    LYSetupEdit(&MyEdit, value, max_length, (far_col - startcol));
    MyEdit.pad = '_';
    MyEdit.hidden = (BOOL) (form->type == F_PASSWORD_TYPE);
    if (use_last_tfpos && LastTFPos >= 0 && LastTFPos < MyEdit.strlen) {
#if defined(TEXTFIELDS_MAY_NEED_ACTIVATION) && defined(INACTIVE_INPUT_STYLE_VH)
	if (redraw_only) {
	    if (!(MyEdit.strlen >= MyEdit.dspwdth &&
		  LastTFPos >= MyEdit.dspwdth - MyEdit.margin)) {
		MyEdit.pos = LastTFPos;
		if (MyEdit.strlen >= MyEdit.dspwdth)
		    textinput_redrawn = FALSE;
	    }
	} else
#endif /* TEXTFIELDS_MAY_NEED_ACTIVATION && INACTIVE_INPUT_STYLE_VH */
	    MyEdit.pos = LastTFPos;
#ifdef ENHANCED_LINEEDIT
	if (MyEdit.pos == 0)
	    MyEdit.mark = -1 - MyEdit.strlen;	/* Do not show the region. */
#endif
    }
    /* Try to prepare for setting position based on the last mouse event */
#if defined(TEXTFIELDS_MAY_NEED_ACTIVATION) && defined(INACTIVE_INPUT_STYLE_VH)
    if (!redraw_only) {
	if (peek_mouse_levent()) {
	    if (!use_last_tfpos && !textinput_redrawn) {
		MyEdit.pos = 0;
	    }
	}
	textinput_redrawn = FALSE;
    }
#else
    if (peek_mouse_levent()) {
	if (!use_last_tfpos)
	    MyEdit.pos = 0;
    }
#endif /* TEXTFIELDS_MAY_NEED_ACTIVATION && INACTIVE_INPUT_STYLE_VH */
    LYRefreshEdit(&MyEdit);
    if (redraw_only)
	return 0;		/*return value won't be analysed*/

    /*
     *  And go for it!
     */
    for (;;) {
again:
	repeat = -1;
	get_mouse_link();	/* Reset mouse_link. */

	ch = LYgetch_input();
#ifdef SUPPORT_MULTIBYTE_EDIT
	if (!refresh_mb
	 && (EditBinding(ch) != LYE_CHAR)
#ifndef WIN_EX
	 && (EditBinding(ch) != LYE_AIX)
#endif
	    )
	    goto again;
#endif /* SUPPORT_MULTIBYTE_EDIT */
#ifdef VMS
	if (HadVMSInterrupt) {
	    HadVMSInterrupt = FALSE;
	    ch = LYCharINTERRUPT2;
	}
#endif /* VMS */

	action = 0;
#ifdef USE_MOUSE
#  if defined(NCURSES) || defined(PDCURSES)
	if (ch != -1 && (ch & LKC_ISLAC) && !(ch & LKC_ISLECLAC)) /* already lynxactioncode? */
	    break;	/* @@@ maybe move these 2 lines outside ifdef -kw */
	if (ch == MOUSE_KEY) {		/* Need to process ourselves */
#if defined(PDCURSES)
	    int curx, cury;

	    request_mouse_pos();
	    LYGetYX(cury, curx);
	    if (MOUSE_Y_POS == cury) {
		repeat = MOUSE_X_POS - curx;
		if (repeat < 0) {
		    action = LYE_BACK;
		    repeat = - repeat;
		} else
		    action = LYE_FORW;
	    }
#else
	    MEVENT	event;
	    int curx, cury;

	    getmouse(&event);
	    LYGetYX(cury, curx);
	    if (event.y == cury) {
		repeat = event.x - curx;
		if (repeat < 0) {
		    action = LYE_BACK;
		    repeat = - repeat;
		} else
		    action = LYE_FORW;
	    }
#endif /* PDCURSES */
	    else {
		/*  Mouse event passed to us as MOUSE_KEY, and apparently
		 *  not on this field's line?  Something is not as it
		 *  should be...
		 *  A call to statusline() may have happened, possibly from
		 *  within a mouse menu.  Let's at least make sure here
		 *  that the cursor position gets restored.  - kw
		 */
		MyEdit.dirty = TRUE;
	    }
	    last_xlkc = -1;
	} else
#  endif     /* NCURSES || PDCURSES */
#endif /* USE_MOUSE */

	{
	    if (!(ch & LKC_ISLECLAC))
		ch |= MyEdit.current_modifiers;
	    MyEdit.current_modifiers = 0;
	    if (last_xlkc != -1) {
		if (ch == last_xlkc)
		    ch |= LKC_MOD3;
		last_xlkc = -1;	/* consumed */
	    }
	}
	if (peek_mouse_link() != -1)
	    break;

	if (!action)
	    action = EditBinding(ch);
	if ((action & LYE_DF) && !(action & LYE_FORM_LAC)) {
	    last_xlkc = ch;
	    action &= ~LYE_DF;
	} else {
	    last_xlkc = -1;
	}

	if (action == LYE_SETM1) {
	    /*
	     *  Set flag for modifier 1.
	     */
	    MyEdit.current_modifiers |= LKC_MOD1;
	    continue;
	}
	if (action == LYE_SETM2) {
	    /*
	     *  Set flag for modifier 2.
	     */
	    MyEdit.current_modifiers |= LKC_MOD2;
	    continue;
	}
	/*
	 *  Filter out global navigation keys that should not be passed
	 *  to line editor, and LYK_REFRESH.
	 */
	if (action == LYE_ENTER)
	    break;
	if (action == LYE_FORM_PASS)
	    break;
	if (action & LYE_FORM_LAC) {
	    ch = (action & LAC_MASK) | LKC_ISLAC;
	    break;
	}
	if (action == LYE_LKCMD) {
	    _statusline(ENTER_LYNX_COMMAND);
	    ch = LYgetch();
#ifdef VMS
	    if (HadVMSInterrupt) {
		HadVMSInterrupt = FALSE;
		ch = LYCharINTERRUPT2;
	    }
#endif /* VMS */
	    break;
	}

#ifdef CAN_CUT_AND_PASTE	/* 1998/10/01 (Thu) 19:19:22 */
	if (action == LYE_PASTE) {
	    unsigned char *s = get_clip_grab(), *e;
	    char *buf = NULL;
	    int len;

	    if (!s)
		break;
	    len = strlen(s);
	    e = s + len;

	    if (len > 0) {
		unsigned char *e1 = s;

		while (e1 < e) {
		    if (*e1 < ' ') { /* Stop here? */
			if (e1 > s)
			    LYEditInsert(&MyEdit, s, e1 - s, -1, TRUE);
			s = e1;
			if (*e1 == '\t') { /* Replace by space */
			    LYEditInsert(&MyEdit, " ", 1, -1, TRUE);
			    s = ++e1;
			} else
			    break;
		    } else
			++e1;
		}
		if (e1 > s)
		    LYEditInsert(&MyEdit, s, e1 - s, -1, TRUE);
		while (e1 < e && *e1 == '\r')
		    e1++;
		if (e1 + 1 < e && *e1 == '\n')
		    StrAllocCopy(buf, e1 + 1);	/* Survive _release() */
		get_clip_release();
		if (MyEdit.strlen >= max_length) {
		    HaveMaxlength = TRUE;
		} else if (HaveMaxlength &&
			   MyEdit.strlen < max_length) {
		    HaveMaxlength = FALSE;
		    _statusline(ENTER_TEXT_ARROWS_OR_TAB);
		}
		if (strcmp(value, MyEdit.buffer) != 0) {
		    Edited = TRUE;
		}
		if (buf) {
		    put_clip(buf);
		    FREE(buf);
		    ch = '\n';		/* Sometimes moves to the next line */
		    break;
		}
		LYRefreshEdit(&MyEdit);
	    } else {
		HTInfoMsg("Clipboard empty or Not text data.");
		continue;
	    }
	}
#endif
#ifndef WIN_EX
	if (action == LYE_AIX &&
	    (HTCJK == NOCJK && LYlowest_eightbit[current_char_set] > 0x97))
	    break;
#endif
	if (action == LYE_TAB) {
	    ch = (int)('\t');
	    break;
	}
	if (action == LYE_ABORT) {
	    return(DO_NOTHING);
	}
	if (action == LYE_STOP) {
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
	    textfields_need_activation = TRUE;
	    break;
#else
#ifdef ENHANCED_LINEEDIT
	    if (MyEdit.mark >= 0)
		MyEdit.mark = -1 - MyEdit.strlen;	/* Disable. */
#endif
#endif
	}
	if (action == LYE_NOP && LKC_TO_LAC(keymap,ch) == LYK_REFRESH)
	    break;
#ifdef SH_EX
/* ASATAKU emacskey hack 1997/08/26 (Tue) 09:19:23 */
	if (emacs_keys &&
	    (EditBinding(ch) == LYE_FORWW || EditBinding(ch) == LYE_BACKW))
	    goto breakfor;
/* ASATAKU emacskey hack */
#endif
	switch (ch) {
	    default:
	    /*	[ 1999/04/14 (Wed) 15:01:33 ]
	     *  Left arrrow in column 0 deserves special treatment here,
	     *  else you can get trapped in a form without submit button!
	     */
		if (action == LYE_BACK && MyEdit.pos == 0 && repeat == -1) {
		    int c = YES;    /* Go back immediately if no changes */
		    if (textfield_prompt_at_left_edge) {
			c = HTConfirmDefault(PREV_DOC_QUERY, NO);
		    } else if (strcmp(MyEdit.buffer, value)) {
			c = HTConfirmDefault(PREV_DOC_QUERY, NO);
		    }
		    if (c == YES) {
			return(ch);
		    } else {
			if (form->disabled == YES)
			    _statusline(ARROWS_OR_TAB_TO_MOVE);
			else
			    _statusline(ENTER_TEXT_ARROWS_OR_TAB);
		    }
		}
		if (form->disabled == YES) {
		    /*
		     *  Allow actions that don't modify the contents even
		     *  in disabled form fields, so the user can scroll
		     *  through the line for reading if necessary. - kw
		     */
		    switch(action) {
		    case LYE_BOL:
		    case LYE_EOL:
		    case LYE_FORW:
		    case LYE_FORW_RL:
		    case LYE_BACK:
		    case LYE_BACK_LL:
		    case LYE_FORWW:
		    case LYE_BACKW:
#ifdef EXP_KEYBOARD_LAYOUT
		    case LYE_SWMAP:
#endif
#ifdef ENHANCED_LINEEDIT
		    case LYE_SETMARK:
		    case LYE_XPMARK:
#endif
			break;
		    default:
			goto again;
		    }
		}
		/*
		 *  Make sure the statusline uses editmode help.
		 */
		if (repeat < 0)
		    repeat = 1;
		while (repeat--) {
		    int rc = LYEdit1(&MyEdit, ch, action & ~LYE_DF, TRUE);

		    if (rc < 0) {
			ch = -rc;
			/* FORW_RL and BACK_LL may require special attention.
			   BACK_LL wanted to switch to the previous link on
			   the same line.  However, if there is no such link,
			   then we would either disactivate the form
			   (with -tna), or will reenter the form, thus we jump
			   to the end of the line; both are counterintuitive.
			   Unfortunately, we do not have access to curdoc.link,
			   so we deduce it ourselves.  We don't have the info
			   to do it inside LYLineEdit().
			   This should work for prompts too.  */
			if ( (action != LYE_BACK_LL && action != LYE_FORW_RL)
			     || ((form_link - links) >= 0
				&& (form_link - links) < nlinks
				&& (action==LYE_FORW_RL
				    ? (form_link - links) < nlinks - 1
				    : (form_link - links) > 0)
				&& form_link[action==LYE_FORW_RL ? 1 : -1].ly
				   == form_link->ly))
			    goto breakfor;
		    }
#ifdef SUPPORT_MULTIBYTE_EDIT
		    if (rc == 0) {
			if (HTCJK != NOCJK && (0x80 <= ch)
			&& (ch <= 0xfe) && refresh_mb)
			    refresh_mb = FALSE;
			else
			    refresh_mb = TRUE;
		    } else {
			if (!refresh_mb) {
			    LYEdit1(&MyEdit, 0, LYE_DELP, TRUE);
			}
		    }
#endif /* SUPPORT_MULTIBYTE_EDIT */
		}
		if (MyEdit.strlen >= max_length) {
		    HaveMaxlength = TRUE;
		} else if (HaveMaxlength &&
			   MyEdit.strlen < max_length) {
		    HaveMaxlength = FALSE;
		    _statusline(ENTER_TEXT_ARROWS_OR_TAB);
		}
		if (strcmp(value, MyEdit.buffer)) {
		    Edited = TRUE;
		}
#ifdef SUPPORT_MULTIBYTE_EDIT
		if (refresh_mb)
#endif
		LYRefreshEdit(&MyEdit);
		LYSetLastTFPos(MyEdit.pos);
	}
    }
  breakfor:
    if (Edited) {
	char  *p;

	/*
	 *  Load the new value.
	 */
	if (value == form->value) {
	    /*
	     *  The previous value did fit in the line buffer,
	     *  so replace it with the new value. - FM
	     */
	    StrAllocCopy(form->value, MyEdit.buffer);
	} else {
	    /*
	     *  Combine the modified tail with the unmodified head. - FM
	     */
	    form->value[(strlen(form->value) - strlen(value))] = '\0';
	    StrAllocCat(form->value, MyEdit.buffer);
	    HTUserMsg(FORM_TAIL_COMBINED_WITH_HEAD);
	}

	/* 2.8.4pre.3 - most browsers appear to preserve trailing spaces -VH */
	/*
	 *  Remove trailing spaces
	 *
	 *  Do we really need to do that here?  Trailing spaces will only
	 *  be there if user keyed them in.  Rather rude to throw away
	 *  their hard earned spaces.  Better deal with trailing spaces
	 *  when submitting the form????
	 */
	if (LYtrimInputFields) {
	    p = &(form->value[strlen(form->value)]);
	    while ((p != form->value) && (p[-1] == ' '))
		p--;
	    *p = '\0';
	}

	/*
	 *  If the field has been changed, assume that it is now in
	 *  current display character set, even if for some reason
	 *  it wasn't!  Hopefully a user will only submit the form
	 *  if the non-ASCII characters are displayed correctly, which
	 *  means (assuming that the display character set has been set
	 *  truthfully) the user confirms by changing the field that
	 *  the character encoding is right. - kw
	 */
	if (form->value && *form->value)
	    form->value_cs = current_char_set;
    }
    return(ch);
}

/*
 *  Display statusline info tailored for the current form field.
 */
PUBLIC void show_formlink_statusline ARGS2(
    CONST FormInfo *,	form,
    int,		for_what)
{
    switch(form->type) {
    case F_PASSWORD_TYPE:
	if (form->disabled == YES)
	    statusline(FORM_LINK_PASSWORD_UNM_MSG);
	else
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
	    if (for_what == FOR_PANEL)
		statusline(FORM_LINK_PASSWORD_MESSAGE_INA);
	    else
#endif
	    statusline(FORM_LINK_PASSWORD_MESSAGE);
	break;
    case F_OPTION_LIST_TYPE:
	if (form->disabled == YES)
	    statusline(FORM_LINK_OPTION_LIST_UNM_MSG);
	else
	    statusline(FORM_LINK_OPTION_LIST_MESSAGE);
	break;
    case F_CHECKBOX_TYPE:
	if (form->disabled == YES)
	    statusline(FORM_LINK_CHECKBOX_UNM_MSG);
	else
	    statusline(FORM_LINK_CHECKBOX_MESSAGE);
	break;
    case F_RADIO_TYPE:
	if (form->disabled == YES)
	    statusline(FORM_LINK_RADIO_UNM_MSG);
	else
	    statusline(FORM_LINK_RADIO_MESSAGE);
	break;
    case F_TEXT_SUBMIT_TYPE:
	if (form->disabled == YES) {
	    statusline(FORM_LINK_TEXT_SUBMIT_UNM_MSG);
	} else if (form->submit_method ==
		   URL_MAIL_METHOD) {
	    if (no_mail)
		statusline(FORM_LINK_TEXT_SUBMIT_MAILTO_DIS_MSG);
	    else
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
		if (for_what == FOR_PANEL)
		    statusline(FORM_TEXT_SUBMIT_MAILTO_MSG_INA);
		else
#endif
		statusline(FORM_LINK_TEXT_SUBMIT_MAILTO_MSG);
	} else if (form->no_cache) {
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
	    if (for_what == FOR_PANEL)
		statusline(FORM_TEXT_RESUBMIT_MESSAGE_INA);
	    else
#endif
	    statusline(FORM_LINK_TEXT_RESUBMIT_MESSAGE);
	} else {
	    char *submit_str = NULL;
	    char *xkey_info = key_for_func_ext(LYK_NOCACHE, for_what);
	    if (xkey_info && *xkey_info) {
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
		if (for_what == FOR_PANEL)
		    HTSprintf0(&submit_str, FORM_TEXT_SUBMIT_MESSAGE_INA_X,
			       xkey_info);
		else
#endif
		    HTSprintf0(&submit_str, FORM_LINK_TEXT_SUBMIT_MESSAGE_X,
			       xkey_info);
		statusline(submit_str);
		FREE(submit_str);
	    } else {
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
		if (for_what == FOR_PANEL)
		    statusline(FORM_LINK_TEXT_SUBMIT_MESSAGE_INA);
		else
#endif
		statusline(FORM_LINK_TEXT_SUBMIT_MESSAGE);
	    }
	    FREE(xkey_info);
	}
	break;
    case F_SUBMIT_TYPE:
    case F_IMAGE_SUBMIT_TYPE:
	if (form->disabled == YES) {
	    statusline(FORM_LINK_SUBMIT_DIS_MSG);
	} else if (form->submit_method ==
		   URL_MAIL_METHOD) {
	    if (no_mail) {
		statusline(FORM_LINK_SUBMIT_MAILTO_DIS_MSG);
	    } else {
		if(user_mode == ADVANCED_MODE) {
		    char *submit_str = NULL;

		    StrAllocCopy(submit_str, FORM_LINK_SUBMIT_MAILTO_PREFIX);
		    StrAllocCat(submit_str, form->submit_action);
		    statusline(submit_str);
		    FREE(submit_str);
		} else {
		    statusline(FORM_LINK_SUBMIT_MAILTO_MSG);
		}
	    }
	} else if (form->no_cache) {
	    if(user_mode == ADVANCED_MODE) {
		char *submit_str = NULL;

		StrAllocCopy(submit_str, FORM_LINK_RESUBMIT_PREFIX);
		StrAllocCat(submit_str, form->submit_action);
		statusline(submit_str);
		FREE(submit_str);
	    } else {
		statusline(FORM_LINK_RESUBMIT_MESSAGE);
	    }
	} else {
	    if(user_mode == ADVANCED_MODE) {
		char *submit_str = NULL;

		StrAllocCopy(submit_str, FORM_LINK_SUBMIT_PREFIX);
		StrAllocCat(submit_str, form->submit_action);
		statusline(submit_str);
		FREE(submit_str);
	    } else {
		statusline(FORM_LINK_SUBMIT_MESSAGE);
	    }
	}
	break;
    case F_RESET_TYPE:
	if (form->disabled == YES)
	    statusline(FORM_LINK_RESET_DIS_MSG);
	else
	    statusline(FORM_LINK_RESET_MESSAGE);
	break;
    case F_FILE_TYPE:
	if (form->disabled == YES)
	    statusline(FORM_LINK_FILE_UNM_MSG);
	else
	    statusline(FORM_LINK_FILE_MESSAGE);
	break;
    case F_TEXT_TYPE:
	if (form->disabled == YES)
	    statusline(FORM_LINK_TEXT_UNM_MSG);
	else
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
	    if (for_what == FOR_PANEL)
		statusline(FORM_LINK_TEXT_MESSAGE_INA);
	    else
#endif
	    statusline(FORM_LINK_TEXT_MESSAGE);
	break;
    case F_TEXTAREA_TYPE:
	if (form->disabled == YES) {
	    statusline(FORM_LINK_TEXT_UNM_MSG);
	} else {
	    char *submit_str = NULL;
	    char *xkey_info = NULL;
	    if (!no_editor && editor && editor) {
		xkey_info = key_for_func_ext(LYK_EDIT_TEXTAREA, for_what);
#ifdef TEXTAREA_AUTOEXTEDIT
		if (!xkey_info)
		    xkey_info = key_for_func_ext(LYK_DWIMEDIT, for_what);
#endif
	    }
	    if (xkey_info && *xkey_info) {
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
		if (for_what == FOR_PANEL)
		    HTSprintf0(&submit_str, FORM_LINK_TEXTAREA_MESSAGE_INA_E,
			       xkey_info);
		else
#endif
		    HTSprintf0(&submit_str, FORM_LINK_TEXTAREA_MESSAGE_E,
			       xkey_info);
		statusline(submit_str);
		FREE(submit_str);
	    } else {
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
		if (for_what == FOR_PANEL)
		    statusline(FORM_LINK_TEXTAREA_MESSAGE_INA);
		else
#endif
		    statusline(FORM_LINK_TEXTAREA_MESSAGE);
	    }
	    FREE(xkey_info);
	}
	break;
    }
}
