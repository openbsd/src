/*	$OpenBSD: funmap.c,v 1.64 2022/10/20 18:59:24 op Exp $	*/

/* This file is in the public domain */

#include <sys/queue.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def.h"
#include "funmap.h"
#include "kbd.h"

/*
 * funmap structure: a list of functions and their command-names/#parameters.
 *
 * If the function is NULL, it must be listed with the same name in the
 * map_table.
 */
struct funmap {
	PF		 fn_funct;
	const		 char *fn_name;
	int		 fn_nparams;
	struct funmap	*fn_next;
};
static struct funmap *funs;

/*
 * 3rd column in the functnames structure indicates how many parameters the
 * function takes in 'normal' usage. This column is only used to identify
 * function profiles when lines of a buffer are being evaluated via excline().
 *
 *  0 = a toggle, non-modifiable insert/delete, region modifier, etc
 *  1 = value can be string or number value (like: file/buf name, search string)
 *  2 = multiple type value required, see auto-execute, or global-set-key, etc
 * -1 = error: interactive commmand, unsuitable for interpreter
 *
 * Some functions when used interactively may ask for a 'y' or 'n' (or another
 * character) to continue, in excline, a 'y' is assumed. Functions like this
 * have '0' in the 3rd column below.
 */
static struct funmap functnames[] = {
	{apropos_command, "apropos", 1},
	{toggleaudiblebell, "audible-bell", 0},
	{auto_execute, "auto-execute", 2},
	{fillmode, "auto-fill-mode", 0},
	{indentmode, "auto-indent-mode", 0},
	{backtoindent, "back-to-indentation", 0},
	{backuptohomedir, "backup-to-home-directory", 0},
	{backchar, "backward-char", 1},
	{delbword, "backward-kill-word", 1},
	{gotobop, "backward-paragraph", 1},
	{backword, "backward-word", 1},
	{gotobob, "beginning-of-buffer", 0},
	{gotobol, "beginning-of-line", 0},
	{showmatch, "blink-and-insert", 1},		/* startup only	*/
	{bsmap, "bsmap-mode", 0},
	{NULL, "c-x 4 prefix", 0},			/* internal	*/
	{NULL, "c-x prefix", 0},			/* internal	*/
	{executemacro, "call-last-kbd-macro", 0},
	{capword, "capitalize-word", 1},
	{changedir, "cd", 1},
	{clearmark, "clear-mark", 0},
	{colnotoggle, "column-number-mode", 0},
	{copyregion, "copy-region-as-kill", 0},
#ifdef	REGEX
	{cntmatchlines, "count-matches", 1},
	{cntnonmatchlines, "count-non-matches", 1},
#endif /* REGEX */
	{cscreatelist, "cscope-create-list-of-files-to-index", 1},
	{csfuncalled, "cscope-find-called-functions", 1},
	{csegrep, "cscope-find-egrep-pattern", 1},
	{csfindinc, "cscope-find-files-including-file", 1},
	{cscallerfuncs, "cscope-find-functions-calling-this-function", 1},
	{csdefinition, "cscope-find-global-definition", 1},
	{csfindfile, "cscope-find-this-file", 1},
	{cssymbol, "cscope-find-this-symbol", 1},
	{csfindtext, "cscope-find-this-text-string", 1},
	{csnextfile, "cscope-next-file", 0},
	{csnextmatch, "cscope-next-symbol", 0},
	{csprevfile, "cscope-prev-file", 0},
	{csprevmatch, "cscope-prev-symbol", 0},
	{redefine_key, "define-key", 3},
	{backdel, "delete-backward-char", 1},
	{deblank, "delete-blank-lines", 0},
	{forwdel, "delete-char", 1},
	{delwhite, "delete-horizontal-space", 0},
	{delleadwhite, "delete-leading-space", 0},
#ifdef	REGEX
	{delmatchlines, "delete-matching-lines", 1},
	{delnonmatchlines, "delete-non-matching-lines", 1},
#endif /* REGEX */
	{onlywind, "delete-other-windows", 0},
	{deltrailwhite, "delete-trailing-space", 0},
	{delwind, "delete-window", 0},
	{wallchart, "describe-bindings", 0},
	{desckey, "describe-key-briefly", 1},
	{diffbuffer, "diff-buffer-with-file", 0},
	{digit_argument, "digit-argument", 1},
	{dired_jump, "dired-jump", 1},
	{lowerregion, "downcase-region", 0},
	{lowerword, "downcase-word", 1},
	{showversion, "emacs-version", 0},
	{finishmacro, "end-kbd-macro", 0},
	{gotoeob, "end-of-buffer", 0},
	{gotoeol, "end-of-line", 0},
	{enlargewind, "enlarge-window", 0},
	{NULL, "esc prefix", 0},			/* internal	*/
	{evalbuffer, "eval-current-buffer", 0},
	{evalexpr, "eval-expression", 0},
	{swapmark, "exchange-point-and-mark", 0},
	{extend, "execute-extended-command", 1},
	{fillpara, "fill-paragraph", 0},
	{filevisitalt, "find-alternate-file", 1},
	{filevisit, "find-file", 1},
	{poptofile, "find-file-other-window", 1},
	{filevisitro, "find-file-read-only", 1},
	{findtag, "find-tag", 1},
	{forwchar, "forward-char", 1},
	{gotoeop, "forward-paragraph", 1},
	{forwword, "forward-word", 1},
	{bindtokey, "global-set-key", 2},
	{unbindtokey, "global-unset-key", 1},
	{globalwdtoggle, "global-wd-mode", 0},
	{gotoline, "goto-line", 1},
	{help_help, "help-help", 0},
	{indent, "indent-current-line", 0},
	{insert, "insert", 1},
	{bufferinsert, "insert-buffer", 1},
	{fileinsert, "insert-file", 1},
	{fillword, "insert-with-wrap", 1},		/* startup only */
	{backisearch, "isearch-backward", 1},
	{forwisearch, "isearch-forward", 1},
	{joinline, "join-line", 0},
	{justone, "just-one-space", 0},
	{ctrlg, "keyboard-quit", 0},
	{killbuffer_cmd, "kill-buffer", 1},
	{killline, "kill-line", 1},
	{killpara, "kill-paragraph", 1},
	{zaptochar, "zap-to-char", 1},
	{zapuptochar, "zap-up-to-char", 1},
	{killregion, "kill-region", 0},
	{delfword, "kill-word", 1},
	{toggleleavetmp, "leave-tmpdir-backups", 0},
	{linenotoggle, "line-number-mode", 0},
	{listbuffers, "list-buffers", 0},
	{evalfile, "load", 1},
	{localbind, "local-set-key", 1},
	{localunbind, "local-unset-key", 1},
	{makebkfile, "make-backup-files", 0},
	{makedir, "make-directory", 1},
	{markpara, "mark-paragraph", 1},
	{markbuffer, "mark-whole-buffer", 0},
	{do_meta, "meta-key-mode", 0},	/* better name, anyone? */
	{negative_argument, "negative-argument", 1},
	{enewline, "newline", 1},
	{lfindent, "newline-and-indent", 1},
	{forwline, "next-line", 1},
#ifdef NOTAB
	{notabmode, "no-tab-mode", 0},
#endif /* NOTAB */
	{notmodified, "not-modified", 0},
	{openline, "open-line", 1},
	{nextwind, "other-window", 0},
	{overwrite_mode, "overwrite-mode", 0},
	{poptag, "pop-tag-mark", 0},
	{prefixregion, "prefix-region", 0},
	{backline, "previous-line", 1},
	{prevwind, "previous-window", 0},
	{spawncli, "push-shell", 0},
	{showcwdir, "pwd", 0},
	{queryrepl, "query-replace", -1},
#ifdef REGEX
	{re_queryrepl, "query-replace-regexp", -1},
#endif /* REGEX */
	{quote, "quoted-insert", 1},
#ifdef REGEX
	{re_searchagain, "re-search-again", 0},
	{re_backsearch, "re-search-backward", 0},
	{re_forwsearch, "re-search-forward", 0},
#endif /* REGEX */
	{reposition, "recenter", 0},
	{redraw, "redraw-display", 0},
#ifdef REGEX
	{re_repl, "replace-regexp", 2},
	{replstr, "replace-string", 2},
#endif /* REGEX */
	{revertbuffer, "revert-buffer", 0},
	{filesave, "save-buffer", 1},
	{quit, "save-buffers-kill-emacs", 0},
	{savebuffers, "save-some-buffers", 0},
	{backpage, "scroll-down", 1},
	{back1page, "scroll-one-line-down", 1},
	{forw1page, "scroll-one-line-up", 1},
	{pagenext, "scroll-other-window", 1},
	{forwpage, "scroll-up", 1},
	{searchagain, "search-again", 0},
	{backsearch, "search-backward", 0},
	{forwsearch, "search-forward", 0},
	{ask_selfinsert, "self-insert-char", 1},
	{selfinsert, "self-insert-command", 1},		/* startup only */
	{sentencespace, "sentence-end-double-space", 0},
#ifdef REGEX
	{setcasefold, "set-case-fold-search", 0},
#endif /* REGEX */
	{setcasereplace, "set-case-replace", 0},
	{set_default_mode, "set-default-mode", 1},
	{setfillcol, "set-fill-column", 1},
	{setmark, "set-mark-command", 0},
	{setprefix, "set-prefix-string", 1},
	{shellcommand, "shell-command", 1},
	{piperegion, "shell-command-on-region", 1},
	{shrinkwind, "shrink-window", 1},
#ifdef NOTAB
	{space_to_tabstop, "space-to-tabstop", 0},
#endif /* NOTAB */
	{splitwind, "split-window-vertically", 0},
	{definemacro, "start-kbd-macro", 0},
	{spawncli, "suspend-emacs", 0},
	{usebuffer, "switch-to-buffer", 1},
	{poptobuffer, "switch-to-buffer-other-window", 1},
	{togglereadonly, "toggle-read-only", 0},
	{togglereadonlyall, "toggle-read-only-all", 0},
	{twiddle, "transpose-chars", 0},
	{transposepara, "transpose-paragraphs", 0},
	{transposeword, "transpose-words", 0},
	{undo, "undo", 0},
	{undo_add_boundary, "undo-boundary", 0},
	{undo_boundary_enable, "undo-boundary-toggle", 0},
	{undo_enable, "undo-enable", 0},
	{undo_dump, "undo-list", 0},
	{universal_argument, "universal-argument", 1},
	{upperregion, "upcase-region", 0},
	{upperword, "upcase-word", 1},
	{togglevisiblebell, "visible-bell", 0},
	{tagsvisit, "visit-tags-table", 0},
	{showcpos, "what-cursor-position", 0},
	{filewrite, "write-file", 1},
	{yank, "yank", 1},
	{NULL, NULL, 0}
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
funmap_add(PF fun, const char *fname, int fparams)
{
	struct funmap *fn;

	if ((fn = malloc(sizeof(*fn))) == NULL)
		return (FALSE);

	fn->fn_funct = fun;
	fn->fn_name = fname;
	fn->fn_nparams = fparams;
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

/*
 * Find number of parameters for function name.
 */
int
numparams_function(PF fun)
{
	struct funmap *fn;

	for (fn = funs; fn != NULL; fn = fn->fn_next) {
		if (fn->fn_funct == fun)
			return (fn->fn_nparams);
	}
	return (FALSE);
}
