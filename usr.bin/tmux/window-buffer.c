/* $OpenBSD: window-buffer.c,v 1.13 2018/02/28 08:55:44 nicm Exp $ */

/*
 * Copyright (c) 2017 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vis.h>

#include "tmux.h"

static struct screen	*window_buffer_init(struct window_pane *,
			     struct cmd_find_state *, struct args *);
static void		 window_buffer_free(struct window_pane *);
static void		 window_buffer_resize(struct window_pane *, u_int,
			     u_int);
static void		 window_buffer_key(struct window_pane *,
			     struct client *, struct session *, key_code,
			     struct mouse_event *);

#define WINDOW_BUFFER_DEFAULT_COMMAND "paste-buffer -b '%%'"

#define WINDOW_BUFFER_DEFAULT_FORMAT \
	"#{buffer_size} bytes (#{t:buffer_created})"

const struct window_mode window_buffer_mode = {
	.name = "buffer-mode",

	.init = window_buffer_init,
	.free = window_buffer_free,
	.resize = window_buffer_resize,
	.key = window_buffer_key,
};

enum window_buffer_sort_type {
	WINDOW_BUFFER_BY_TIME,
	WINDOW_BUFFER_BY_NAME,
	WINDOW_BUFFER_BY_SIZE,
};
static const char *window_buffer_sort_list[] = {
	"time",
	"name",
	"size"
};

struct window_buffer_itemdata {
	const char	*name;
	u_int		 order;
	size_t		 size;
};

struct window_buffer_modedata {
	struct mode_tree_data		 *data;
	char				 *command;
	char				 *format;

	struct window_buffer_itemdata	**item_list;
	u_int				  item_size;
};

static struct window_buffer_itemdata *
window_buffer_add_item(struct window_buffer_modedata *data)
{
	struct window_buffer_itemdata	*item;

	data->item_list = xreallocarray(data->item_list, data->item_size + 1,
	    sizeof *data->item_list);
	item = data->item_list[data->item_size++] = xcalloc(1, sizeof *item);
	return (item);
}

static void
window_buffer_free_item(struct window_buffer_itemdata *item)
{
	free((void *)item->name);
	free(item);
}

static int
window_buffer_cmp_name(const void *a0, const void *b0)
{
	const struct window_buffer_itemdata *const *a = a0;
	const struct window_buffer_itemdata *const *b = b0;

	return (strcmp((*a)->name, (*b)->name));
}

static int
window_buffer_cmp_time(const void *a0, const void *b0)
{
	const struct window_buffer_itemdata *const *a = a0;
	const struct window_buffer_itemdata *const *b = b0;

	if ((*a)->order > (*b)->order)
		return (-1);
	if ((*a)->order < (*b)->order)
		return (1);
	return (strcmp((*a)->name, (*b)->name));
}

static int
window_buffer_cmp_size(const void *a0, const void *b0)
{
	const struct window_buffer_itemdata *const *a = a0;
	const struct window_buffer_itemdata *const *b = b0;

	if ((*a)->size > (*b)->size)
		return (-1);
	if ((*a)->size < (*b)->size)
		return (1);
	return (strcmp((*a)->name, (*b)->name));
}

static void
window_buffer_build(void *modedata, u_int sort_type, __unused uint64_t *tag,
    const char *filter)
{
	struct window_buffer_modedata	*data = modedata;
	struct window_buffer_itemdata	*item;
	u_int				 i;
	struct paste_buffer		*pb;
	char				*text, *cp;
	struct format_tree		*ft;

	for (i = 0; i < data->item_size; i++)
		window_buffer_free_item(data->item_list[i]);
	free(data->item_list);
	data->item_list = NULL;
	data->item_size = 0;

	pb = NULL;
	while ((pb = paste_walk(pb)) != NULL) {
		item = window_buffer_add_item(data);
		item->name = xstrdup(paste_buffer_name(pb));
		paste_buffer_data(pb, &item->size);
		item->order = paste_buffer_order(pb);
	}

	switch (sort_type) {
	case WINDOW_BUFFER_BY_NAME:
		qsort(data->item_list, data->item_size, sizeof *data->item_list,
		    window_buffer_cmp_name);
		break;
	case WINDOW_BUFFER_BY_TIME:
		qsort(data->item_list, data->item_size, sizeof *data->item_list,
		    window_buffer_cmp_time);
		break;
	case WINDOW_BUFFER_BY_SIZE:
		qsort(data->item_list, data->item_size, sizeof *data->item_list,
		    window_buffer_cmp_size);
		break;
	}

	for (i = 0; i < data->item_size; i++) {
		item = data->item_list[i];

		pb = paste_get_name(item->name);
		if (pb == NULL)
			continue;
		ft = format_create(NULL, NULL, FORMAT_NONE, 0);
		format_defaults_paste_buffer(ft, pb);

		if (filter != NULL) {
			cp = format_expand(ft, filter);
			if (!format_true(cp)) {
				free(cp);
				format_free(ft);
				continue;
			}
			free(cp);
		}

		text = format_expand(ft, data->format);
		mode_tree_add(data->data, NULL, item, item->order, item->name,
		    text, -1);
		free(text);

		format_free(ft);
	}

}

static void
window_buffer_draw(__unused void *modedata, void *itemdata,
    struct screen_write_ctx *ctx, u_int sx, u_int sy)
{
	struct window_buffer_itemdata	*item = itemdata;
	struct paste_buffer		*pb;
	char				 line[1024];
	const char			*pdata, *end, *cp;
	size_t				 psize, at;
	u_int				 i, cx = ctx->s->cx, cy = ctx->s->cy;

	pb = paste_get_name(item->name);
	if (pb == NULL)
		return;

	pdata = end = paste_buffer_data(pb, &psize);
	for (i = 0; i < sy; i++) {
		at = 0;
		while (end != pdata + psize && *end != '\n') {
			if ((sizeof line) - at > 5) {
				cp = vis(line + at, *end, VIS_TAB|VIS_OCTAL, 0);
				at = cp - line;
			}
			end++;
		}
		if (at > sx)
			at = sx;
		line[at] = '\0';

		if (*line != '\0') {
			screen_write_cursormove(ctx, cx, cy + i);
			screen_write_puts(ctx, &grid_default_cell, "%s", line);
		}

		if (end == pdata + psize)
			break;
		end++;
	}
}

static int
window_buffer_search(__unused void *modedata, void *itemdata, const char *ss)
{
	struct window_buffer_itemdata	*item = itemdata;
	struct paste_buffer		*pb;
	const char			*bufdata;
	size_t				 bufsize;

	if ((pb = paste_get_name(item->name)) == NULL)
		return (0);
	if (strstr(item->name, ss) != NULL)
		return (1);
	bufdata = paste_buffer_data(pb, &bufsize);
	return (memmem(bufdata, bufsize, ss, strlen(ss)) != NULL);
}

static struct screen *
window_buffer_init(struct window_pane *wp, __unused struct cmd_find_state *fs,
    struct args *args)
{
	struct window_buffer_modedata	*data;
	struct screen			*s;

	wp->modedata = data = xcalloc(1, sizeof *data);

	if (args == NULL || !args_has(args, 'F'))
		data->format = xstrdup(WINDOW_BUFFER_DEFAULT_FORMAT);
	else
		data->format = xstrdup(args_get(args, 'F'));
	if (args == NULL || args->argc == 0)
		data->command = xstrdup(WINDOW_BUFFER_DEFAULT_COMMAND);
	else
		data->command = xstrdup(args->argv[0]);

	data->data = mode_tree_start(wp, args, window_buffer_build,
	    window_buffer_draw, window_buffer_search, data,
	    window_buffer_sort_list, nitems(window_buffer_sort_list), &s);
	mode_tree_zoom(data->data, args);

	mode_tree_build(data->data);
	mode_tree_draw(data->data);

	return (s);
}

static void
window_buffer_free(struct window_pane *wp)
{
	struct window_buffer_modedata	*data = wp->modedata;
	u_int				 i;

	if (data == NULL)
		return;

	mode_tree_free(data->data);

	for (i = 0; i < data->item_size; i++)
		window_buffer_free_item(data->item_list[i]);
	free(data->item_list);

	free(data->format);
	free(data->command);

	free(data);
}

static void
window_buffer_resize(struct window_pane *wp, u_int sx, u_int sy)
{
	struct window_buffer_modedata	*data = wp->modedata;

	mode_tree_resize(data->data, sx, sy);
}

static void
window_buffer_do_delete(void* modedata, void *itemdata,
    __unused struct client *c, __unused key_code key)
{
	struct window_buffer_modedata	*data = modedata;
	struct window_buffer_itemdata	*item = itemdata;
	struct paste_buffer		*pb;

	if (item == mode_tree_get_current(data->data))
		mode_tree_down(data->data, 0);
	if ((pb = paste_get_name(item->name)) != NULL)
		paste_free(pb);
}

static void
window_buffer_do_paste(void* modedata, void *itemdata, struct client *c,
    __unused key_code key)
{
	struct window_buffer_modedata	*data = modedata;
	struct window_buffer_itemdata	*item = itemdata;
	struct paste_buffer		*pb;

	if ((pb = paste_get_name(item->name)) != NULL)
		mode_tree_run_command(c, NULL, data->command, item->name);
}

static void
window_buffer_key(struct window_pane *wp, struct client *c,
    __unused struct session *s, key_code key, struct mouse_event *m)
{
	struct window_buffer_modedata	*data = wp->modedata;
	struct mode_tree_data		*mtd = data->data;
	struct window_buffer_itemdata	*item;
	int				 finished;

	finished = mode_tree_key(mtd, c, &key, m, NULL, NULL);
	switch (key) {
	case 'd':
		item = mode_tree_get_current(mtd);
		window_buffer_do_delete(data, item, c, key);
		mode_tree_build(mtd);
		break;
	case 'D':
		mode_tree_each_tagged(mtd, window_buffer_do_delete, c, key, 0);
		mode_tree_build(mtd);
		break;
	case 'P':
		mode_tree_each_tagged(mtd, window_buffer_do_paste, c, key, 0);
		finished = 1;
		break;
	case 'p':
	case '\r':
		item = mode_tree_get_current(mtd);
		window_buffer_do_paste(data, item, c, key);
		finished = 1;
		break;
	}
	if (finished || paste_get_top(NULL) == NULL)
		window_pane_reset_mode(wp);
	else {
		mode_tree_draw(mtd);
		wp->flags |= PANE_REDRAW;
	}
}
