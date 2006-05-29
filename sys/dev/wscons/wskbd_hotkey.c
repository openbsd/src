/*
 * Copyright (c) 2006 Alexey Vatchenko <avv@mail.zp.ua>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Processing of special key symbols.
 */
#include "audio.h"		/* NAUDIO (mixer tuning) */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kthread.h>
#include <sys/proc.h>

#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#define WSKBD_HOTKEY_MAXEVENTS		8

static keysym_t			ksym_queue[WSKBD_HOTKEY_MAXEVENTS];
static u_int			queue_head, queue_tail;
static struct simplelock	queue_lock;
static int			wskbd_hotkey_initted = 0;

void	init_hotkey_thread(void *);
void	hotkey_thread(void *);
void	process_hotkey(keysym_t);
int	wskbd_hotkey_get(keysym_t *);

#if NAUDIO > 0
extern int wskbd_set_mixervolume(int dir);
#endif

/* ARGSUSED */
void
init_hotkey_thread(void *ctx)
{
	int error;

	error = kthread_create(hotkey_thread, ctx, NULL, "wskbd_hotkey");
#ifdef DIAGNOSTIC
	if (error != 0)
		printf("init_hotkey_thread: cannot create process\n");
#endif
}

/* ARGSUSED */
void
hotkey_thread(void *ctx)
{
	keysym_t sym;
	int error;

	for (;;) {
		error = wskbd_hotkey_get(&sym);
		if (error == 0)
			process_hotkey(sym);
		else
			(void) tsleep(ksym_queue, PZERO, "wait", 0);
	}

	/* NOTREACHED */
}

void
process_hotkey(keysym_t sym)
{
	/* Process Audio tuning keys */
	switch (sym) {
#if NAUDIO > 0
	case KS_AudioMute:
		(void) wskbd_set_mixervolume(0);
		break;
	case KS_AudioLower:
		(void) wskbd_set_mixervolume(-1);
		break;
	case KS_AudioRaise:
		(void) wskbd_set_mixervolume(1);
		break;
#endif
	default:
#ifdef DEBUG
		printf("process_hotkey: unsupported hotkey\n");
#endif
		break;
	}
}

void
wskbd_hotkey_init(void)
{

	if (wskbd_hotkey_initted == 0) {
		simple_lock_init(&queue_lock);
		queue_head = queue_tail = 0;
		kthread_create_deferred(init_hotkey_thread, NULL);
		wskbd_hotkey_initted = 1;
	}
}

void
wskbd_hotkey_put(keysym_t sym)
{
	int s, changed;
	u_int nxtpos;

	changed = 0;

	s = spltty();
	simple_lock(&queue_lock);

	nxtpos = (queue_head + 1) % WSKBD_HOTKEY_MAXEVENTS;
	if (nxtpos != queue_tail) {
		ksym_queue[queue_head] = sym;
		queue_head = nxtpos;
		changed = 1;
	}
#ifdef DEBUG
	else
		printf("wskbd_hotkey_put: losing hotkey\n");
#endif

	simple_unlock(&queue_lock);
	splx(s);

	if (changed != 0)
		wakeup(ksym_queue);
}

int
wskbd_hotkey_get(keysym_t *sym)
{
	int s, error;

	s = spltty();
	simple_lock(&queue_lock);

	error = 0;

	if (queue_head != queue_tail) {
		*sym = ksym_queue[queue_tail];
		queue_tail = (queue_tail + 1) % WSKBD_HOTKEY_MAXEVENTS;
	} else
		error = EAGAIN;

	simple_unlock(&queue_lock);
	splx(s);

	return (error);
}
