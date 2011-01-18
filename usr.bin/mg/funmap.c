/*	$OpenBSD: funmap.c,v 1.34 2011/01/18 16:25:40 kjell Exp $	*/

/* This file is in the public domain */

#include "def.h"
#include "kbd.h"
#include "funmap.h"

/*
 * If the function is NULL, it must be listed with the
 * same name in the map_table.
 */

struct funmap {
	PF		 fn_funct;
	const		 char *fn_name;
	struct funmap	*fn_next;
};

static struct funmap *funs;

static struct funmap functnames[] = {
#ifndef	NO_HELP
	{apropos_command, "apropos",},
#endif /* !NO_HELP */
	{auto_execute, "auto-execute", },
	{fillmode, "auto-fill-mode",},
	{indentmode, "auto-indent-mode",},
	{backtoindent, "back-to-indentation",},
	{backchar, "backward-char",},
	{delbword, "backward-kill-word",},
	{gotobop, "backward-paragraph",},
	{backword, "backward-word",},
	{gotobob, "beginning-of-buffer",},
	{gotobol, "beginning-of-line",},
	{showmatch, "blink-and-insert",},
	{bsmap, "bsmap-mode",},
	{NULL, "c-x 4 prefix",},
	{NULL, "c-x prefix",},
#ifndef NO_MACRO
	{executemacro, "call-last-kbd-macro",},
#endif /* !NO_MACRO */
	{capword, "capitalize-word",},
	{changedir, "cd",},
	{clearmark, "clear-mark",},
	{copyregion, "copy-region-as-kill",},
#ifdef	REGEX
	{cntmatchlines, "count-matches",},
	{cntnonmatchlines, "count-non-matches",},
#endif /* REGEX */
	{redefine_key, "define-key",},
	{backdel, "delete-backward-char",},
	{deblank, "delete-blank-lines",},
	{forwdel, "delete-char",},
	{delwhite, "delete-horizontal-space",},
	{delleadwhite, "delete-leading-space",},
	{deltrailwhite, "delete-trailing-space",},
#ifdef	REGEX
	{delmatchlines, "delete-matching-lines",},
	{delnonmatchlines, "delete-non-matching-lines",},
#endif /* REGEX */
	{onlywind, "delete-other-windows",},
	{delwind, "delete-window",},
#ifndef NO_HELP
	{wallchart, "describe-bindings",},
	{desckey, "describe-key-briefly",},
#endif /* !NO_HELP */
	{digit_argument, "digit-argument",},
	{lowerregion, "downcase-region",},
	{lowerword, "downcase-word",},
	{showversion, "emacs-version",},
#ifndef NO_MACRO
	{finishmacro, "end-kbd-macro",},
#endif /* !NO_MACRO */
	{globalwdtoggle, "global-wd-mode",},
	{gotoeob, "end-of-buffer",},
	{gotoeol, "end-of-line",},
	{enlargewind, "enlarge-window",},
	{NULL, "esc prefix",},
#ifndef NO_STARTUP
	{evalbuffer, "eval-current-buffer",},
	{evalexpr, "eval-expression",},
#endif /* !NO_STARTUP */
	{swapmark, "exchange-point-and-mark",},
	{extend, "execute-extended-command",},
	{fillpara, "fill-paragraph",},
	{filevisit, "find-file",},
	{filevisitro, "find-file-read-only",},
	{filevisitalt, "find-alternate-file",},
	{poptofile, "find-file-other-window",},
	{forwchar, "forward-char",},
	{gotoeop, "forward-paragraph",},
	{forwword, "forward-word",},
	{bindtokey, "global-set-key",},
	{unbindtokey, "global-unset-key",},
	{gotoline, "goto-line",},
#ifndef NO_HELP
	{help_help, "help-help",},
#endif /* !NO_HELP */
	{insert, "insert",},
	{bufferinsert, "insert-buffer",},
	{fileinsert, "insert-file",},
	{fillword, "insert-with-wrap",},
	{backisearch, "isearch-backward",},
	{forwisearch, "isearch-forward",},
	{joinline, "join-line",},
	{justone, "just-one-space",},
	{ctrlg, "keyboard-quit",},
	{killbuffer_cmd, "kill-buffer",},
	{killline, "kill-line",},
	{killpara, "kill-paragraph",},
	{killregion, "kill-region",},
	{delfword, "kill-word",},
	{linenotoggle, "line-number-mode",},
	{listbuffers, "list-buffers",},
#ifndef NO_STARTUP
	{evalfile, "load",},
#endif /* !NO_STARTUP */
	{localbind, "local-set-key",},
	{localunbind, "local-unset-key",},
	{makebkfile, "make-backup-files",},
	{do_meta, "meta-key-mode",},	/* better name, anyone? */
	{negative_argument, "negative-argument",},
	{newline, "newline",},
	{lfindent, "newline-and-indent",},
	{indent, "indent-current-line",},
	{forwline, "next-line",},
#ifdef NOTAB
	{notabmode, "no-tab-mode",},
#endif /* NOTAB */
	{notmodified, "not-modified",},
	{openline, "open-line",},
	{nextwind, "other-window",},
	{overwrite_mode, "overwrite-mode",},
	{prefixregion, "prefix-region",},
	{backline, "previous-line",},
	{prevwind, "previous-window",},
	{spawncli, "push-shell",},
	{showcwdir, "pwd",},
	{queryrepl, "query-replace",},
#ifdef REGEX
	{replstr, "replace-string",},
	{re_queryrepl, "query-replace-regexp",},
#endif /* REGEX */
	{quote, "quoted-insert",},
#ifdef REGEX
	{re_searchagain, "re-search-again",},
	{re_backsearch, "re-search-backward",},
	{re_forwsearch, "re-search-forward",},
#endif /* REGEX */
	{reposition, "recenter",},
	{redraw, "redraw-display",},
	{filesave, "save-buffer",},
	{quit, "save-buffers-kill-emacs",},
	{savebuffers, "save-some-buffers",},
	{backpage, "scroll-down",},
	{back1page, "scroll-one-line-down",},
	{forw1page, "scroll-one-line-up",},
	{pagenext, "scroll-other-window",},
	{forwpage, "scroll-up",},
	{searchagain, "search-again",},
	{backsearch, "search-backward",},
	{forwsearch, "search-forward",},
	{selfinsert, "self-insert-command",},
#ifdef REGEX
	{setcasefold, "set-case-fold-search",},
#endif /* REGEX */
	{set_default_mode, "set-default-mode",},
	{setfillcol, "set-fill-column",},
	{setmark, "set-mark-command",},
	{setprefix, "set-prefix-string",},
	{shrinkwind, "shrink-window",},
#ifdef NOTAB
	{space_to_tabstop, "space-to-tabstop",},
#endif /* NOTAB */
	{splitwind, "split-window-vertically",},
#ifndef NO_MACRO
	{definemacro, "start-kbd-macro",},
#endif /* !NO_MACRO */
	{spawncli, "suspend-emacs",},
	{usebuffer, "switch-to-buffer",},
	{poptobuffer, "switch-to-buffer-other-window",},
	{togglereadonly, "toggle-read-only" },
	{twiddle, "transpose-chars",},
	{undo, "undo", },
	{undo_enable, "undo-enable", },
	{undo_boundary_enable, "undo-boundary-toggle", },
	{undo_add_boundary, "undo-boundary", },
	{undo_dump, "undo-list", },
	{universal_argument, "universal-argument",},
	{upperregion, "upcase-region",},
	{upperword, "upcase-word",},
	{showcpos, "what-cursor-position",},
	{filewrite, "write-file",},
	{yank, "yank",},
	{NULL, NULL,}
};

void
funmap_init(void)
{
	struct funmap *fn;

	for (fn = functnames; fn->fn_name != NULL; fn++) {
		fn->fn_next = funs;
		funs = fn;
	}
}

int
funmap_add(PF fun, const char *fname)
{
	struct funmap *fn;

	if ((fn = malloc(sizeof(*fn))) == NULL)
		return (FALSE);

	fn->fn_funct = fun;
	fn->fn_name = fname;
	fn->fn_next = funs;

	funs = fn;
	return (TRUE);
}

/*
 * Translate from function name to function pointer.
 */
PF
name_function(const char *fname)
{
	struct funmap *fn;

	for (fn = funs; fn != NULL; fn = fn->fn_next) {
		if (strcmp(fn->fn_name, fname) == 0)
			return (fn->fn_funct);
	}
	return (NULL);
}

const char *
function_name(PF fun)
{
	struct funmap *fn;

	for (fn = funs; fn != NULL; fn = fn->fn_next) {
		if (fn->fn_funct == fun)
			return (fn->fn_name);
	}
	return (NULL);
}

/*
 * List possible function name completions.
 */
struct list *
complete_function_list(const char *fname)
{
	struct funmap	*fn;
	struct list	*head, *el;
	int		 len;

	len = strlen(fname);
	head = NULL;
	for (fn = funs; fn != NULL; fn = fn->fn_next) {
		if (memcmp(fname, fn->fn_name, len) == 0) {
			if ((el = malloc(sizeof(*el))) == NULL) {
				free_file_list(head);
				return (NULL);
			}
			el->l_name = strdup(fn->fn_name);
			el->l_next = head;
			head = el;
		}
	}
	return (head);
}
