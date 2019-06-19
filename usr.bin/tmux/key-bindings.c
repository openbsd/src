/* $OpenBSD: key-bindings.c,v 1.98 2019/05/28 10:27:11 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

#define DEFAULT_CLIENT_MENU \
	" 'Detach' 'd' {detach-client}" \
	" 'Detach & Kill' 'X' {detach-client -P}" \
	" 'Detach Others' 'o' {detach-client -a}" \
	" ''" \
	" 'Lock' 'l' {lock-client}"
#define DEFAULT_SESSION_MENU \
	" 'Next' 'n' {switch-client -n}" \
	" 'Previous' 'p' {switch-client -p}" \
	" ''" \
	" 'Renumber' 'N' {move-window -r}" \
	" 'Rename' 'n' {command-prompt -I \"#S\" \"rename-session -- '%%'\"}" \
	" ''" \
	" 'New Session' 's' {new-session}" \
	" 'New Window' 'w' {new-window}"
#define DEFAULT_WINDOW_MENU \
	" 'Swap Left' 'l' {swap-window -t:-1}" \
	" 'Swap Right' 'r' {swap-window -t:+1}" \
	" '#{?pane_marked_set,,-}Swap Marked' 's' {swap-window}" \
	" ''" \
	" 'Kill' 'X' {kill-window}" \
	" 'Respawn' 'R' {respawn-window -k}" \
	" '#{?pane_marked,Unmark,Mark}' 'm' {select-pane -m}" \
	" 'Rename' 'n' {command-prompt -I \"#W\" \"rename-window -- '%%'\"}" \
	" ''" \
	" 'New After' 'w' {new-window -a}" \
	" 'New At End' 'W' {new-window}"
#define DEFAULT_PANE_MENU \
	" '#{?mouse_word,Search For #[underscore]#{=/9/...:mouse_word},}' 'C-r' {copy-mode -t=; send -Xt= search-backward \"#{q:mouse_word}\"}" \
	" '#{?mouse_word,Type #[underscore]#{=/9/...:mouse_word},}' 'C-y' {send-keys -l -- \"#{q:mouse_word}\"}" \
	" '#{?mouse_word,Copy #[underscore]#{=/9/...:mouse_word},}' 'c' {set-buffer -- \"#{q:mouse_word}\"}" \
	" '#{?mouse_line,Copy Line,}' 'l' {set-buffer -- \"#{q:mouse_line}\"}" \
	" ''" \
	" 'Horizontal Split' 'h' {split-window -h}" \
	" 'Vertical Split' 'v' {split-window -v}" \
	" ''" \
	" 'Swap Up' 'u' {swap-pane -U}" \
	" 'Swap Down' 'd' {swap-pane -D}" \
	" '#{?pane_marked_set,,-}Swap Marked' 's' {swap-pane}" \
	" ''" \
	" 'Kill' 'X' {kill-pane}" \
	" 'Respawn' 'R' {respawn-pane -k}" \
	" '#{?pane_marked,Unmark,Mark}' 'm' {select-pane -m}" \
	" '#{?window_zoomed_flag,Unzoom,Zoom}' 'z' {resize-pane -Z}"


static int key_bindings_cmp(struct key_binding *, struct key_binding *);
RB_GENERATE_STATIC(key_bindings, key_binding, entry, key_bindings_cmp);
static int key_table_cmp(struct key_table *, struct key_table *);
RB_GENERATE_STATIC(key_tables, key_table, entry, key_table_cmp);
static struct key_tables key_tables = RB_INITIALIZER(&key_tables);

static int
key_table_cmp(struct key_table *table1, struct key_table *table2)
{
	return (strcmp(table1->name, table2->name));
}

static int
key_bindings_cmp(struct key_binding *bd1, struct key_binding *bd2)
{
	if (bd1->key < bd2->key)
		return (-1);
	if (bd1->key > bd2->key)
		return (1);
	return (0);
}

struct key_table *
key_bindings_get_table(const char *name, int create)
{
	struct key_table	table_find, *table;

	table_find.name = name;
	table = RB_FIND(key_tables, &key_tables, &table_find);
	if (table != NULL || !create)
		return (table);

	table = xmalloc(sizeof *table);
	table->name = xstrdup(name);
	RB_INIT(&table->key_bindings);

	table->references = 1; /* one reference in key_tables */
	RB_INSERT(key_tables, &key_tables, table);

	return (table);
}

struct key_table *
key_bindings_first_table(void)
{
	return (RB_MIN(key_tables, &key_tables));
}

struct key_table *
key_bindings_next_table(struct key_table *table)
{
	return (RB_NEXT(key_tables, &key_tables, table));
}

void
key_bindings_unref_table(struct key_table *table)
{
	struct key_binding	*bd;
	struct key_binding	*bd1;

	if (--table->references != 0)
		return;

	RB_FOREACH_SAFE(bd, key_bindings, &table->key_bindings, bd1) {
		RB_REMOVE(key_bindings, &table->key_bindings, bd);
		cmd_list_free(bd->cmdlist);
		free(bd);
	}

	free((void *)table->name);
	free(table);
}

struct key_binding *
key_bindings_get(struct key_table *table, key_code key)
{
	struct key_binding	bd;

	bd.key = key;
	return (RB_FIND(key_bindings, &table->key_bindings, &bd));
}

struct key_binding *
key_bindings_first(struct key_table *table)
{
	return (RB_MIN(key_bindings, &table->key_bindings));
}

struct key_binding *
key_bindings_next(__unused struct key_table *table, struct key_binding *bd)
{
	return (RB_NEXT(key_bindings, &table->key_bindings, bd));
}

void
key_bindings_add(const char *name, key_code key, int repeat,
    struct cmd_list *cmdlist)
{
	struct key_table	*table;
	struct key_binding	 bd_find, *bd;

	table = key_bindings_get_table(name, 1);

	bd_find.key = (key & ~KEYC_XTERM);
	bd = RB_FIND(key_bindings, &table->key_bindings, &bd_find);
	if (bd != NULL) {
		RB_REMOVE(key_bindings, &table->key_bindings, bd);
		cmd_list_free(bd->cmdlist);
		free(bd);
	}

	bd = xcalloc(1, sizeof *bd);
	bd->key = key;
	RB_INSERT(key_bindings, &table->key_bindings, bd);

	if (repeat)
		bd->flags |= KEY_BINDING_REPEAT;
	bd->cmdlist = cmdlist;
}

void
key_bindings_remove(const char *name, key_code key)
{
	struct key_table	*table;
	struct key_binding	 bd_find, *bd;

	table = key_bindings_get_table(name, 0);
	if (table == NULL)
		return;

	bd_find.key = (key & ~KEYC_XTERM);
	bd = RB_FIND(key_bindings, &table->key_bindings, &bd_find);
	if (bd == NULL)
		return;

	RB_REMOVE(key_bindings, &table->key_bindings, bd);
	cmd_list_free(bd->cmdlist);
	free(bd);

	if (RB_EMPTY(&table->key_bindings)) {
		RB_REMOVE(key_tables, &key_tables, table);
		key_bindings_unref_table(table);
	}
}

void
key_bindings_remove_table(const char *name)
{
	struct key_table	*table;
	struct client		*c;

	table = key_bindings_get_table(name, 0);
	if (table != NULL) {
		RB_REMOVE(key_tables, &key_tables, table);
		key_bindings_unref_table(table);
	}
	TAILQ_FOREACH(c, &clients, entry) {
		if (c->keytable == table)
			server_client_set_key_table(c, NULL);
	}
}

void
key_bindings_init(void)
{
	static const char *defaults[] = {
		"bind C-b send-prefix",
		"bind C-o rotate-window",
		"bind C-z suspend-client",
		"bind Space next-layout",
		"bind ! break-pane",
		"bind '\"' split-window",
		"bind '#' list-buffers",
		"bind '$' command-prompt -I'#S' \"rename-session -- '%%'\"",
		"bind % split-window -h",
		"bind & confirm-before -p\"kill-window #W? (y/n)\" kill-window",
		"bind \"'\" command-prompt -pindex \"select-window -t ':%%'\"",
		"bind ( switch-client -p",
		"bind ) switch-client -n",
		"bind , command-prompt -I'#W' \"rename-window -- '%%'\"",
		"bind - delete-buffer",
		"bind . command-prompt \"move-window -t '%%'\"",
		"bind 0 select-window -t:=0",
		"bind 1 select-window -t:=1",
		"bind 2 select-window -t:=2",
		"bind 3 select-window -t:=3",
		"bind 4 select-window -t:=4",
		"bind 5 select-window -t:=5",
		"bind 6 select-window -t:=6",
		"bind 7 select-window -t:=7",
		"bind 8 select-window -t:=8",
		"bind 9 select-window -t:=9",
		"bind : command-prompt",
		"bind \\; last-pane",
		"bind = choose-buffer -Z",
		"bind ? list-keys",
		"bind D choose-client -Z",
		"bind E select-layout -E",
		"bind L switch-client -l",
		"bind M select-pane -M",
		"bind [ copy-mode",
		"bind ] paste-buffer",
		"bind c new-window",
		"bind d detach-client",
		"bind f command-prompt \"find-window -Z -- '%%'\"",
		"bind i display-message",
		"bind l last-window",
		"bind m select-pane -m",
		"bind n next-window",
		"bind o select-pane -t:.+",
		"bind p previous-window",
		"bind q display-panes",
		"bind r refresh-client",
		"bind s choose-tree -Zs",
		"bind t clock-mode",
		"bind w choose-tree -Zw",
		"bind x confirm-before -p\"kill-pane #P? (y/n)\" kill-pane",
		"bind z resize-pane -Z",
		"bind '{' swap-pane -U",
		"bind '}' swap-pane -D",
		"bind '~' show-messages",
		"bind PPage copy-mode -u",
		"bind -r Up select-pane -U",
		"bind -r Down select-pane -D",
		"bind -r Left select-pane -L",
		"bind -r Right select-pane -R",
		"bind M-1 select-layout even-horizontal",
		"bind M-2 select-layout even-vertical",
		"bind M-3 select-layout main-horizontal",
		"bind M-4 select-layout main-vertical",
		"bind M-5 select-layout tiled",
		"bind M-n next-window -a",
		"bind M-o rotate-window -D",
		"bind M-p previous-window -a",
		"bind -r S-Up refresh-client -U 10",
		"bind -r S-Down refresh-client -D 10",
		"bind -r S-Left refresh-client -L 10",
		"bind -r S-Right refresh-client -R 10",
		"bind -r DC refresh-client -c",
		"bind -r M-Up resize-pane -U 5",
		"bind -r M-Down resize-pane -D 5",
		"bind -r M-Left resize-pane -L 5",
		"bind -r M-Right resize-pane -R 5",
		"bind -r C-Up resize-pane -U",
		"bind -r C-Down resize-pane -D",
		"bind -r C-Left resize-pane -L",
		"bind -r C-Right resize-pane -R",

		"bind -n MouseDown1Pane select-pane -t=\\; send-keys -M",
		"bind -n MouseDrag1Border resize-pane -M",
		"bind -n MouseDown1Status select-window -t=",
		"bind -n WheelDownStatus next-window",
		"bind -n WheelUpStatus previous-window",
		"bind -n MouseDrag1Pane if -Ft= '#{mouse_any_flag}' 'if -Ft= \"#{pane_in_mode}\" \"copy-mode -M\" \"send-keys -M\"' 'copy-mode -M'",
		"bind -n WheelUpPane if -Ft= '#{mouse_any_flag}' 'send-keys -M' 'if -Ft= \"#{pane_in_mode}\" \"send-keys -M\" \"copy-mode -et=\"'",

		"bind -n MouseDown3StatusRight display-menu -t= -xM -yS -T \"#[align=centre]#{client_name}\" " DEFAULT_CLIENT_MENU,
		"bind -n MouseDown3StatusLeft display-menu -t= -xM -yS -T \"#[align=centre]#{session_name}\" " DEFAULT_SESSION_MENU,
		"bind -n MouseDown3Status display-menu -t= -xW -yS -T \"#[align=centre]#{window_index}:#{window_name}\" " DEFAULT_WINDOW_MENU,
		"bind C-m display-menu -xW -yS -T \"#[align=centre]#{window_index}:#{window_name}\" " DEFAULT_WINDOW_MENU,
		"bind -n MouseDown3Pane if -Ft= '#{||:#{mouse_any_flag},#{pane_in_mode}}' 'select-pane -t=; send-keys -M' {display-menu -t= -xM -yM -T \"#[align=centre]#{pane_index} (#{pane_id})\" " DEFAULT_PANE_MENU "}",
		"bind -n M-MouseDown3Pane display-menu -t= -xM -yM -T \"#[align=centre]#{pane_index} (#{pane_id})\" " DEFAULT_PANE_MENU,
		"bind M-m display-menu -xP -yP -T \"#[align=centre]#{pane_index} (#{pane_id})\" " DEFAULT_PANE_MENU,

		"bind -Tcopy-mode C-Space send -X begin-selection",
		"bind -Tcopy-mode C-a send -X start-of-line",
		"bind -Tcopy-mode C-c send -X cancel",
		"bind -Tcopy-mode C-e send -X end-of-line",
		"bind -Tcopy-mode C-f send -X cursor-right",
		"bind -Tcopy-mode C-b send -X cursor-left",
		"bind -Tcopy-mode C-g send -X clear-selection",
		"bind -Tcopy-mode C-k send -X copy-end-of-line",
		"bind -Tcopy-mode C-n send -X cursor-down",
		"bind -Tcopy-mode C-p send -X cursor-up",
		"bind -Tcopy-mode C-r command-prompt -ip'(search up)' -I'#{pane_search_string}' 'send -X search-backward-incremental \"%%%\"'",
		"bind -Tcopy-mode C-s command-prompt -ip'(search down)' -I'#{pane_search_string}' 'send -X search-forward-incremental \"%%%\"'",
		"bind -Tcopy-mode C-v send -X page-down",
		"bind -Tcopy-mode C-w send -X copy-selection-and-cancel",
		"bind -Tcopy-mode Escape send -X cancel",
		"bind -Tcopy-mode Space send -X page-down",
		"bind -Tcopy-mode , send -X jump-reverse",
		"bind -Tcopy-mode \\; send -X jump-again",
		"bind -Tcopy-mode F command-prompt -1p'(jump backward)' 'send -X jump-backward \"%%%\"'",
		"bind -Tcopy-mode N send -X search-reverse",
		"bind -Tcopy-mode R send -X rectangle-toggle",
		"bind -Tcopy-mode T command-prompt -1p'(jump to backward)' 'send -X jump-to-backward \"%%%\"'",
		"bind -Tcopy-mode f command-prompt -1p'(jump forward)' 'send -X jump-forward \"%%%\"'",
		"bind -Tcopy-mode g command-prompt -p'(goto line)' 'send -X goto-line \"%%%\"'",
		"bind -Tcopy-mode n send -X search-again",
		"bind -Tcopy-mode q send -X cancel",
		"bind -Tcopy-mode t command-prompt -1p'(jump to forward)' 'send -X jump-to-forward \"%%%\"'",
		"bind -Tcopy-mode Home send -X start-of-line",
		"bind -Tcopy-mode End send -X end-of-line",
		"bind -Tcopy-mode MouseDown1Pane select-pane",
		"bind -Tcopy-mode MouseDrag1Pane select-pane\\; send -X begin-selection",
		"bind -Tcopy-mode MouseDragEnd1Pane send -X copy-selection-and-cancel",
		"bind -Tcopy-mode WheelUpPane select-pane\\; send -N5 -X scroll-up",
		"bind -Tcopy-mode WheelDownPane select-pane\\; send -N5 -X scroll-down",
		"bind -Tcopy-mode DoubleClick1Pane select-pane\\; send -X select-word",
		"bind -Tcopy-mode TripleClick1Pane select-pane\\; send -X select-line",
		"bind -Tcopy-mode NPage send -X page-down",
		"bind -Tcopy-mode PPage send -X page-up",
		"bind -Tcopy-mode Up send -X cursor-up",
		"bind -Tcopy-mode Down send -X cursor-down",
		"bind -Tcopy-mode Left send -X cursor-left",
		"bind -Tcopy-mode Right send -X cursor-right",
		"bind -Tcopy-mode M-1 command-prompt -Np'(repeat)' -I1 'send -N \"%%%\"'",
		"bind -Tcopy-mode M-2 command-prompt -Np'(repeat)' -I2 'send -N \"%%%\"'",
		"bind -Tcopy-mode M-3 command-prompt -Np'(repeat)' -I3 'send -N \"%%%\"'",
		"bind -Tcopy-mode M-4 command-prompt -Np'(repeat)' -I4 'send -N \"%%%\"'",
		"bind -Tcopy-mode M-5 command-prompt -Np'(repeat)' -I5 'send -N \"%%%\"'",
		"bind -Tcopy-mode M-6 command-prompt -Np'(repeat)' -I6 'send -N \"%%%\"'",
		"bind -Tcopy-mode M-7 command-prompt -Np'(repeat)' -I7 'send -N \"%%%\"'",
		"bind -Tcopy-mode M-8 command-prompt -Np'(repeat)' -I8 'send -N \"%%%\"'",
		"bind -Tcopy-mode M-9 command-prompt -Np'(repeat)' -I9 'send -N \"%%%\"'",
		"bind -Tcopy-mode M-< send -X history-top",
		"bind -Tcopy-mode M-> send -X history-bottom",
		"bind -Tcopy-mode M-R send -X top-line",
		"bind -Tcopy-mode M-b send -X previous-word",
		"bind -Tcopy-mode C-M-b send -X previous-matching-bracket",
		"bind -Tcopy-mode M-f send -X next-word-end",
		"bind -Tcopy-mode C-M-f send -X next-matching-bracket",
		"bind -Tcopy-mode M-m send -X back-to-indentation",
		"bind -Tcopy-mode M-r send -X middle-line",
		"bind -Tcopy-mode M-v send -X page-up",
		"bind -Tcopy-mode M-w send -X copy-selection-and-cancel",
		"bind -Tcopy-mode 'M-{' send -X previous-paragraph",
		"bind -Tcopy-mode 'M-}' send -X next-paragraph",
		"bind -Tcopy-mode M-Up send -X halfpage-up",
		"bind -Tcopy-mode M-Down send -X halfpage-down",
		"bind -Tcopy-mode C-Up send -X scroll-up",
		"bind -Tcopy-mode C-Down send -X scroll-down",

		"bind -Tcopy-mode-vi C-c send -X cancel",
		"bind -Tcopy-mode-vi C-d send -X halfpage-down",
		"bind -Tcopy-mode-vi C-e send -X scroll-down",
		"bind -Tcopy-mode-vi C-b send -X page-up",
		"bind -Tcopy-mode-vi C-f send -X page-down",
		"bind -Tcopy-mode-vi C-h send -X cursor-left",
		"bind -Tcopy-mode-vi C-j send -X copy-selection-and-cancel",
		"bind -Tcopy-mode-vi Enter send -X copy-selection-and-cancel",
		"bind -Tcopy-mode-vi C-u send -X halfpage-up",
		"bind -Tcopy-mode-vi C-v send -X rectangle-toggle",
		"bind -Tcopy-mode-vi C-y send -X scroll-up",
		"bind -Tcopy-mode-vi Escape send -X clear-selection",
		"bind -Tcopy-mode-vi Space send -X begin-selection",
		"bind -Tcopy-mode-vi '$' send -X end-of-line",
		"bind -Tcopy-mode-vi , send -X jump-reverse",
		"bind -Tcopy-mode-vi / command-prompt -p'(search down)' 'send -X search-forward \"%%%\"'",
		"bind -Tcopy-mode-vi 0 send -X start-of-line",
		"bind -Tcopy-mode-vi 1 command-prompt -Np'(repeat)' -I1 'send -N \"%%%\"'",
		"bind -Tcopy-mode-vi 2 command-prompt -Np'(repeat)' -I2 'send -N \"%%%\"'",
		"bind -Tcopy-mode-vi 3 command-prompt -Np'(repeat)' -I3 'send -N \"%%%\"'",
		"bind -Tcopy-mode-vi 4 command-prompt -Np'(repeat)' -I4 'send -N \"%%%\"'",
		"bind -Tcopy-mode-vi 5 command-prompt -Np'(repeat)' -I5 'send -N \"%%%\"'",
		"bind -Tcopy-mode-vi 6 command-prompt -Np'(repeat)' -I6 'send -N \"%%%\"'",
		"bind -Tcopy-mode-vi 7 command-prompt -Np'(repeat)' -I7 'send -N \"%%%\"'",
		"bind -Tcopy-mode-vi 8 command-prompt -Np'(repeat)' -I8 'send -N \"%%%\"'",
		"bind -Tcopy-mode-vi 9 command-prompt -Np'(repeat)' -I9 'send -N \"%%%\"'",
		"bind -Tcopy-mode-vi : command-prompt -p'(goto line)' 'send -X goto-line \"%%%\"'",
		"bind -Tcopy-mode-vi \\; send -X jump-again",
		"bind -Tcopy-mode-vi ? command-prompt -p'(search up)' 'send -X search-backward \"%%%\"'",
		"bind -Tcopy-mode-vi A send -X append-selection-and-cancel",
		"bind -Tcopy-mode-vi B send -X previous-space",
		"bind -Tcopy-mode-vi D send -X copy-end-of-line",
		"bind -Tcopy-mode-vi E send -X next-space-end",
		"bind -Tcopy-mode-vi F command-prompt -1p'(jump backward)' 'send -X jump-backward \"%%%\"'",
		"bind -Tcopy-mode-vi G send -X history-bottom",
		"bind -Tcopy-mode-vi H send -X top-line",
		"bind -Tcopy-mode-vi J send -X scroll-down",
		"bind -Tcopy-mode-vi K send -X scroll-up",
		"bind -Tcopy-mode-vi L send -X bottom-line",
		"bind -Tcopy-mode-vi M send -X middle-line",
		"bind -Tcopy-mode-vi N send -X search-reverse",
		"bind -Tcopy-mode-vi T command-prompt -1p'(jump to backward)' 'send -X jump-to-backward \"%%%\"'",
		"bind -Tcopy-mode-vi V send -X select-line",
		"bind -Tcopy-mode-vi W send -X next-space",
		"bind -Tcopy-mode-vi ^ send -X back-to-indentation",
		"bind -Tcopy-mode-vi b send -X previous-word",
		"bind -Tcopy-mode-vi e send -X next-word-end",
		"bind -Tcopy-mode-vi f command-prompt -1p'(jump forward)' 'send -X jump-forward \"%%%\"'",
		"bind -Tcopy-mode-vi g send -X history-top",
		"bind -Tcopy-mode-vi h send -X cursor-left",
		"bind -Tcopy-mode-vi j send -X cursor-down",
		"bind -Tcopy-mode-vi k send -X cursor-up",
		"bind -Tcopy-mode-vi l send -X cursor-right",
		"bind -Tcopy-mode-vi n send -X search-again",
		"bind -Tcopy-mode-vi o send -X other-end",
		"bind -Tcopy-mode-vi q send -X cancel",
		"bind -Tcopy-mode-vi t command-prompt -1p'(jump to forward)' 'send -X jump-to-forward \"%%%\"'",
		"bind -Tcopy-mode-vi v send -X rectangle-toggle",
		"bind -Tcopy-mode-vi w send -X next-word",
		"bind -Tcopy-mode-vi '{' send -X previous-paragraph",
		"bind -Tcopy-mode-vi '}' send -X next-paragraph",
		"bind -Tcopy-mode-vi % send -X next-matching-bracket",
		"bind -Tcopy-mode-vi MouseDown1Pane select-pane",
		"bind -Tcopy-mode-vi MouseDrag1Pane select-pane\\; send -X begin-selection",
		"bind -Tcopy-mode-vi MouseDragEnd1Pane send -X copy-selection-and-cancel",
		"bind -Tcopy-mode-vi WheelUpPane select-pane\\; send -N5 -X scroll-up",
		"bind -Tcopy-mode-vi WheelDownPane select-pane\\; send -N5 -X scroll-down",
		"bind -Tcopy-mode-vi DoubleClick1Pane select-pane\\; send -X select-word",
		"bind -Tcopy-mode-vi TripleClick1Pane select-pane\\; send -X select-line",
		"bind -Tcopy-mode-vi BSpace send -X cursor-left",
		"bind -Tcopy-mode-vi NPage send -X page-down",
		"bind -Tcopy-mode-vi PPage send -X page-up",
		"bind -Tcopy-mode-vi Up send -X cursor-up",
		"bind -Tcopy-mode-vi Down send -X cursor-down",
		"bind -Tcopy-mode-vi Left send -X cursor-left",
		"bind -Tcopy-mode-vi Right send -X cursor-right",
		"bind -Tcopy-mode-vi C-Up send -X scroll-up",
		"bind -Tcopy-mode-vi C-Down send -X scroll-down",
	};
	u_int			 i;
	struct cmd_parse_result	*pr;

	for (i = 0; i < nitems(defaults); i++) {
		pr = cmd_parse_from_string(defaults[i], NULL);
		if (pr->status != CMD_PARSE_SUCCESS)
			fatalx("bad default key: %s", defaults[i]);
		cmdq_append(NULL, cmdq_get_command(pr->cmdlist, NULL, NULL, 0));
		cmd_list_free(pr->cmdlist);
	}
}

static enum cmd_retval
key_bindings_read_only(struct cmdq_item *item, __unused void *data)
{
	cmdq_error(item, "client is read-only");
	return (CMD_RETURN_ERROR);
}

struct cmdq_item *
key_bindings_dispatch(struct key_binding *bd, struct cmdq_item *item,
    struct client *c, struct mouse_event *m, struct cmd_find_state *fs)
{
	struct cmd		*cmd;
	struct cmdq_item	*new_item;
	int			 readonly;

	readonly = 1;
	TAILQ_FOREACH(cmd, &bd->cmdlist->list, qentry) {
		if (!(cmd->entry->flags & CMD_READONLY))
			readonly = 0;
	}
	if (!readonly && (c->flags & CLIENT_READONLY))
		new_item = cmdq_get_callback(key_bindings_read_only, NULL);
	else {
		new_item = cmdq_get_command(bd->cmdlist, fs, m, 0);
		if (bd->flags & KEY_BINDING_REPEAT)
			new_item->shared->flags |= CMDQ_SHARED_REPEAT;
	}
	if (item != NULL)
		cmdq_insert_after(item, new_item);
	else
		cmdq_append(c, new_item);
	return (new_item);
}
