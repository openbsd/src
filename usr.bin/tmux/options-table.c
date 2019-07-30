/* $OpenBSD: options-table.c,v 1.95 2018/02/22 10:54:51 nicm Exp $ */

/*
 * Copyright (c) 2011 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <string.h>
#include <paths.h>

#include "tmux.h"

/*
 * This file has a tables with all the server, session and window
 * options. These tables are the master copy of the options with their real
 * (user-visible) types, range limits and default values. At start these are
 * copied into the runtime global options trees (which only has number and
 * string types). These tables are then used to look up the real type when the
 * user sets an option or its value needs to be shown.
 */

/* Choice option type lists. */
static const char *options_table_mode_keys_list[] = {
	"emacs", "vi", NULL
};
static const char *options_table_clock_mode_style_list[] = {
	"12", "24", NULL
};
static const char *options_table_status_keys_list[] = {
	"emacs", "vi", NULL
};
static const char *options_table_status_justify_list[] = {
	"left", "centre", "right", NULL
};
static const char *options_table_status_position_list[] = {
	"top", "bottom", NULL
};
static const char *options_table_bell_action_list[] = {
	"none", "any", "current", "other", NULL
};
static const char *options_table_visual_bell_list[] = {
	"off", "on", "both", NULL
};
static const char *options_table_pane_status_list[] = {
	"off", "top", "bottom", NULL
};
static const char *options_table_set_clipboard_list[] = {
	"off", "external", "on", NULL
};

/* Top-level options. */
const struct options_table_entry options_table[] = {
	{ .name = "buffer-limit",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SERVER,
	  .minimum = 1,
	  .maximum = INT_MAX,
	  .default_num = 50
	},

	{ .name = "command-alias",
	  .type = OPTIONS_TABLE_ARRAY,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_str = "split-pane=split-window,"
			 "splitp=split-window,"
			 "server-info=show-messages -JT,"
			 "info=show-messages -JT,"
			 "choose-window=choose-tree -w,"
			 "choose-session=choose-tree -s",
	  .separator = ","
	},

	{ .name = "default-terminal",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_str = "screen"
	},

	{ .name = "escape-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SERVER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 500
	},

	{ .name = "exit-empty",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_num = 1
	},

	{ .name = "exit-unattached",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_num = 0
	},

	{ .name = "focus-events",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_num = 0
	},

	{ .name = "history-file",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_str = ""
	},

	{ .name = "message-limit",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SERVER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 100
	},

	{ .name = "set-clipboard",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SERVER,
	  .choices = options_table_set_clipboard_list,
	  .default_num = 1
	},

	{ .name = "terminal-overrides",
	  .type = OPTIONS_TABLE_ARRAY,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_str = "xterm*:XT:Ms=\\E]52;%p1%s;%p2%s\\007"
			 ":Cs=\\E]12;%p1%s\\007:Cr=\\E]112\\007"
			 ":Ss=\\E[%p1%d q:Se=\\E[2 q,screen*:XT",
	  .separator = ","
	},

	{ .name = "user-keys",
	  .type = OPTIONS_TABLE_ARRAY,
	  .scope = OPTIONS_TABLE_SERVER,
	  .default_str = "",
	  .separator = ","
	},

	{ .name = "activity-action",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_bell_action_list,
	  .default_num = ALERT_OTHER
	},

	{ .name = "assume-paste-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 1,
	},

	{ .name = "base-index",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "bell-action",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_bell_action_list,
	  .default_num = ALERT_ANY
	},

	{ .name = "default-command",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = ""
	},

	{ .name = "default-shell",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = _PATH_BSHELL
	},

	{ .name = "destroy-unattached",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0
	},

	{ .name = "detach-on-destroy",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 1
	},

	{ .name = "display-panes-active-colour",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 1
	},

	{ .name = "display-panes-colour",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 4
	},

	{ .name = "display-panes-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 1,
	  .maximum = INT_MAX,
	  .default_num = 1000
	},

	{ .name = "display-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 750
	},

	{ .name = "history-limit",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 2000
	},

	{ .name = "key-table",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "root"
	},

	{ .name = "lock-after-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "lock-command",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "lock -np"
	},

	{ .name = "message-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0,
	  .style = "message-style"
	},

	{ .name = "message-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 3,
	  .style = "message-style"
	},

	{ .name = "message-command-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0,
	  .style = "message-command-style"
	},

	{ .name = "message-command-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0,
	  .style = "message-command-style"
	},

	{ .name = "message-command-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 3,
	  .style = "message-command-style"
	},

	{ .name = "message-command-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "bg=black,fg=yellow"
	},

	{ .name = "message-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0,
	  .style = "message-style"
	},

	{ .name = "message-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "bg=yellow,fg=black"
	},

	{ .name = "mouse",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0
	},

	{ .name = "prefix",
	  .type = OPTIONS_TABLE_KEY,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = '\002',
	},

	{ .name = "prefix2",
	  .type = OPTIONS_TABLE_KEY,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = KEYC_NONE,
	},

	{ .name = "renumber-windows",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0
	},

	{ .name = "repeat-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = SHRT_MAX,
	  .default_num = 500
	},

	{ .name = "set-titles",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0
	},

	{ .name = "set-titles-string",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "#S:#I:#W - \"#T\" #{session_alerts}"
	},

	{ .name = "silence-action",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_bell_action_list,
	  .default_num = ALERT_OTHER
	},

	{ .name = "status",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 1
	},

	{ .name = "status-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0,
	  .style = "status-style"
	},

	{ .name = "status-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 2,
	  .style = "status-style"
	},

	{ .name = "status-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0,
	  .style = "status-style"
	},

	{ .name = "status-interval",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 15
	},

	{ .name = "status-justify",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_status_justify_list,
	  .default_num = 0
	},

	{ .name = "status-keys",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_status_keys_list,
	  .default_num = MODEKEY_EMACS
	},

	{ .name = "status-left",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "[#S] "
	},

	{ .name = "status-left-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0,
	  .style = "status-left-style"
	},

	{ .name = "status-left-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 8,
	  .style = "status-left-style"
	},

	{ .name = "status-left-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 8,
	  .style = "status-left-style"
	},

	{ .name = "status-left-length",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = SHRT_MAX,
	  .default_num = 10
	},

	{ .name = "status-left-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "default"
	},

	{ .name = "status-position",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_status_position_list,
	  .default_num = 1
	},

	{ .name = "status-right",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = " \"#{=21:pane_title}\" %H:%M %d-%b-%y"
	},

	{ .name = "status-right-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 0,
	  .style = "status-right-style"
	},

	{ .name = "status-right-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 8,
	  .style = "status-right-style"
	},

	{ .name = "status-right-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_num = 8,
	  .style = "status-right-style"
	},

	{ .name = "status-right-length",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_SESSION,
	  .minimum = 0,
	  .maximum = SHRT_MAX,
	  .default_num = 40
	},

	{ .name = "status-right-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "default"
	},

	{ .name = "status-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "bg=green,fg=black"
	},

	{ .name = "update-environment",
	  .type = OPTIONS_TABLE_ARRAY,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = "DISPLAY SSH_ASKPASS SSH_AUTH_SOCK SSH_AGENT_PID "
			 "SSH_CONNECTION WINDOWID XAUTHORITY"
	},

	{ .name = "visual-activity",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_visual_bell_list,
	  .default_num = VISUAL_OFF
	},

	{ .name = "visual-bell",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_visual_bell_list,
	  .default_num = VISUAL_OFF
	},

	{ .name = "visual-silence",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_SESSION,
	  .choices = options_table_visual_bell_list,
	  .default_num = VISUAL_OFF
	},

	{ .name = "word-separators",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_SESSION,
	  .default_str = " -_@"
	},

	{ .name = "aggressive-resize",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 0
	},

	{ .name = "allow-rename",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 0
	},

	{ .name = "alternate-screen",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 1
	},

	{ .name = "automatic-rename",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 1
	},

	{ .name = "automatic-rename-format",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "#{?pane_in_mode,[tmux],#{pane_current_command}}"
			 "#{?pane_dead,[dead],}"
	},

	{ .name = "clock-mode-colour",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 4
	},

	{ .name = "clock-mode-style",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .choices = options_table_clock_mode_style_list,
	  .default_num = 1
	},

	{ .name = "force-height",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "force-width",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "main-pane-height",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 1,
	  .maximum = INT_MAX,
	  .default_num = 24
	},

	{ .name = "main-pane-width",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 1,
	  .maximum = INT_MAX,
	  .default_num = 80
	},

	{ .name = "mode-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 0,
	  .style = "mode-style"
	},

	{ .name = "mode-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 3,
	  .style = "mode-style"
	},

	{ .name = "mode-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 0,
	  .style = "mode-style"
	},

	{ .name = "mode-keys",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .choices = options_table_mode_keys_list,
	  .default_num = MODEKEY_EMACS
	},

	{ .name = "mode-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "bg=yellow,fg=black"
	},

	{ .name = "monitor-activity",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 0
	},

	{ .name = "monitor-bell",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 1
	},

	{ .name = "monitor-silence",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "other-pane-height",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "other-pane-width",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "pane-active-border-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 8,
	  .style = "pane-active-border-style"
	},

	{ .name = "pane-active-border-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 2,
	  .style = "pane-active-border-style"
	},

	{ .name = "pane-active-border-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "fg=green"
	},

	{ .name = "pane-base-index",
	  .type = OPTIONS_TABLE_NUMBER,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .minimum = 0,
	  .maximum = USHRT_MAX,
	  .default_num = 0
	},

	{ .name = "pane-border-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 8,
	  .style = "pane-border-style"
	},

	{ .name = "pane-border-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 8,
	  .style = "pane-border-style"
	},

	{ .name = "pane-border-format",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "#{?pane_active,#[reverse],}#{pane_index}#[default] "
			 "\"#{pane_title}\""
	},

	{ .name = "pane-border-status",
	  .type = OPTIONS_TABLE_CHOICE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .choices = options_table_pane_status_list,
	  .default_num = 0
	},

	{ .name = "pane-border-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "default"
	},

	{ .name = "remain-on-exit",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 0
	},

	{ .name = "synchronize-panes",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 0
	},

	{ .name = "window-active-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "default"
	},

	{ .name = "window-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "default"
	},

	{ .name = "window-status-activity-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = GRID_ATTR_REVERSE,
	  .style = "window-status-activity-style"
	},

	{ .name = "window-status-activity-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 8,
	  .style = "window-status-activity-style"
	},

	{ .name = "window-status-activity-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 8,
	  .style = "window-status-activity-style"
	},

	{ .name = "window-status-activity-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "reverse"
	},

	{ .name = "window-status-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 0,
	  .style = "window-status-style"
	},

	{ .name = "window-status-bell-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = GRID_ATTR_REVERSE,
	  .style = "window-status-bell-style"
	},

	{ .name = "window-status-bell-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 8,
	  .style = "window-status-bell-style"
	},

	{ .name = "window-status-bell-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 8,
	  .style = "window-status-bell-style"
	},

	{ .name = "window-status-bell-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "reverse"
	},

	{ .name = "window-status-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 8,
	  .style = "window-status-style"
	},

	{ .name = "window-status-current-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 0,
	  .style = "window-status-current-style"
	},

	{ .name = "window-status-current-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 8,
	  .style = "window-status-current-style"
	},

	{ .name = "window-status-current-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 8,
	  .style = "window-status-current-style"
	},

	{ .name = "window-status-current-format",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "#I:#W#{?window_flags,#{window_flags}, }"
	},

	{ .name = "window-status-current-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "default"
	},

	{ .name = "window-status-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 8,
	  .style = "window-status-style"
	},

	{ .name = "window-status-format",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "#I:#W#{?window_flags,#{window_flags}, }"
	},

	{ .name = "window-status-last-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 0,
	  .style = "window-status-last-style"
	},

	{ .name = "window-status-last-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 8,
	  .style = "window-status-last-style"
	},

	{ .name = "window-status-last-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 8,
	  .style = "window-status-last-style"
	},

	{ .name = "window-status-last-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "default"
	},

	{ .name = "window-status-separator",
	  .type = OPTIONS_TABLE_STRING,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = " "
	},

	{ .name = "window-status-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_str = "default"
	},

	{ .name = "wrap-search",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 1
	},

	{ .name = "xterm-keys",
	  .type = OPTIONS_TABLE_FLAG,
	  .scope = OPTIONS_TABLE_WINDOW,
	  .default_num = 1
	},

	{ .name = NULL }
};
