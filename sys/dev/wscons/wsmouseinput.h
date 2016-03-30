/* $OpenBSD: wsmouseinput.h,v 1.1 2016/03/30 23:34:12 bru Exp $ */

/*
 * Copyright (c) 2015, 2016 Ulf Brosziewski
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
 * wsmouse input processing - private header
 */

#ifndef _WSMOUSEINPUT_H_
#define _WSMOUSEINPUT_H_


struct btn_state {
	u_int buttons;
	u_int sync;
};

struct motion_state {
	int dx;
	int dy;
	int dz;
	int dw;
	int x;
	int y;
	u_int sync;

	/* deltas of absolute coordinates */
	int x_delta;
	int y_delta;
};
#define SYNC_DELTAS		(1 << 0)
#define SYNC_X			(1 << 1)
#define SYNC_Y			(1 << 2)
#define SYNC_POSITION		(SYNC_X | SYNC_Y)

struct touch_state {
	int pressure;
	int contacts;
	int width;
	u_int sync;

	int min_pressure;
};
#define SYNC_PRESSURE		(1 << 0)
#define SYNC_CONTACTS		(1 << 1)
#define SYNC_TOUCH_WIDTH	(1 << 2)

struct mt_slot {
	int x;
	int y;
	int pressure;
	int id;		/* tracking ID */
};
#define MTS_TOUCH	0
#define MTS_X		1
#define MTS_Y		2
#define MTS_PRESSURE	3

#define MTS_SIZE	4

struct mt_state {
	/* the set of slots with active touches */
	u_int touches;
	/* the set of slots with unsynchronized state */
	u_int frame;

	int num_slots;
	struct mt_slot *slots;
	/* the sets of changes per slot axis */
	u_int sync[MTS_SIZE];

	int num_touches;

	/* pointer control */
	u_int ptr;
	u_int ptr_cycle;
	u_int prev_ptr;

	/* a buffer for the MT tracking function */
	int *matrix;
};


struct axis_filter {
	/* scale factor in [*.12] fixed-point format */
	int scale;
	int rmdr;
};

struct wsmouseinput {
	u_int flags;

	struct btn_state btn;
	struct motion_state motion;
	struct touch_state touch;
	struct mt_state mt;

	struct wsmouseparams params;
	struct {
		struct axis_filter h;
		struct axis_filter v;
	} fltr;

	struct wseventvar **evar;
};
/* wsmouseinput.flags */
#define TPAD_COMPAT_MODE	(1 << 0)
#define TPAD_NATIVE_MODE	(1 << 1)
#define SCALE_DELTAS		(1 << 2)
#define MT_TRACKING		(1 << 3)
#define SWAPXY			(1 << 4)
#define RESYNC			(1 << 16)

struct evq_access {
	struct wseventvar *evar;
	struct timespec ts;
	int put;
	int result;
};
#define EVQ_RESULT_OVERFLOW	-1
#define EVQ_RESULT_NONE		0
#define EVQ_RESULT_SUCCESS	1


void wsmouse_evq_put(struct evq_access *, int, int);
void wsmouse_init_scaling(struct wsmouseinput *);

void wsmouse_input_init(struct wsmouseinput *, struct wseventvar **);
void wsmouse_input_cleanup(struct wsmouseinput *);


#define FOREACHBIT(v, i) \
    for ((i) = ffs(v) - 1; (i) != -1; (i) = ffs((v) & (~1 << (i))) - 1)


#define DELTA_X_EV(flags) (((flags) & SWAPXY) ? \
    WSCONS_EVENT_MOUSE_DELTA_Y : WSCONS_EVENT_MOUSE_DELTA_X)
#define DELTA_Y_EV(flags) (((flags) & SWAPXY) ? \
    WSCONS_EVENT_MOUSE_DELTA_X : WSCONS_EVENT_MOUSE_DELTA_Y)
#define ABS_X_EV(flags) (((flags) & SWAPXY) ? \
    WSCONS_EVENT_MOUSE_ABSOLUTE_Y : WSCONS_EVENT_MOUSE_ABSOLUTE_X)
#define ABS_Y_EV(flags) (((flags) & SWAPXY) ? \
    WSCONS_EVENT_MOUSE_ABSOLUTE_X : WSCONS_EVENT_MOUSE_ABSOLUTE_Y)
#define DELTA_Z_EV	WSCONS_EVENT_MOUSE_DELTA_Z
#define DELTA_W_EV	WSCONS_EVENT_MOUSE_DELTA_W
#define ABS_Z_EV	WSCONS_EVENT_TOUCH_PRESSURE
#define ABS_W_EV	WSCONS_EVENT_TOUCH_CONTACTS
#define BTN_DOWN_EV	WSCONS_EVENT_MOUSE_DOWN
#define BTN_UP_EV	WSCONS_EVENT_MOUSE_UP
#define SYNC_EV		WSCONS_EVENT_SYNC

/* buffer size for wsmouse_matching */
#define MATRIX_SIZE(slots) (((slots) + 7) * (slots) * sizeof(int))

#endif /* _WSMOUSEINPUT_H_ */
