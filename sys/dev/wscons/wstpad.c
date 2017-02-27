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
 * touchpad input processing
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>
#include <dev/wscons/wseventvar.h>
#include <dev/wscons/wsmouseinput.h>

#define LEFTBTN			(1 << 0)
#define MIDDLEBTN		(1 << 1)
#define RIGHTBTN		(1 << 2)

#define PRIMARYBTN LEFTBTN

#define PRIMARYBTN_CLICKED(tp) ((tp)->btns_sync & PRIMARYBTN & (tp)->btns)
#define PRIMARYBTN_RELEASED(tp) ((tp)->btns_sync & PRIMARYBTN & ~(tp)->btns)

#define IS_MT(tp) ((tp)->features & WSTPAD_MT)
#define DISABLE(tp) ((tp)->features & WSTPAD_DISABLE)

#define EDGERATIO_DEFAULT	(4096 / 16)
#define CENTERWIDTH_DEFAULT	(4096 / 8)

#define FREEZE_MS		100

enum tpad_handlers {
	SOFTBUTTON_HDLR,
	TOPBUTTON_HDLR,
	F2SCROLL_HDLR,
	EDGESCROLL_HDLR,
	CLICK_HDLR,
};

enum tpad_cmd {
	CLEAR_MOTION_DELTAS,
	SOFTBUTTON_DOWN,
	SOFTBUTTON_UP,
	VSCROLL,
	HSCROLL,
};

/*
 * tpad_touch.flags:
 */
#define L_EDGE			(1 << 0)
#define R_EDGE			(1 << 1)
#define T_EDGE			(1 << 2)
#define B_EDGE			(1 << 3)

#define EDGES (L_EDGE | R_EDGE | T_EDGE | B_EDGE)

enum touchstates {
	TOUCH_NONE,
	TOUCH_BEGIN,
	TOUCH_UPDATE,
	TOUCH_END,
};

struct tpad_touch {
	u_int flags;
	enum touchstates state;
	int x;
	int y;
	int dir;
	int matches;
	struct {
		int x;
		int y;
		struct timespec time;
	} orig;
};

/*
 * wstpad.features
 */
#define WSTPAD_SOFTBUTTONS	(1 << (WSMOUSECFG_SOFTBUTTONS & 0xff))
#define WSTPAD_SOFTMBTN		(1 << (WSMOUSECFG_SOFTMBTN & 0xff))
#define WSTPAD_TOPBUTTONS	(1 << (WSMOUSECFG_TOPBUTTONS & 0xff))
#define WSTPAD_F2SCROLL		(1 << (WSMOUSECFG_TWOFINGERSCROLL & 0xff))
#define WSTPAD_EDGESCROLL	(1 << (WSMOUSECFG_EDGESCROLL & 0xff))
#define WSTPAD_HORIZSCROLL	(1 << (WSMOUSECFG_HORIZSCROLL & 0xff))
#define WSTPAD_SWAPSIDES	(1 << (WSMOUSECFG_SWAPSIDES & 0xff))
#define WSTPAD_DISABLE		(1 << (WSMOUSECFG_DISABLE & 0xff))

#define WSTPAD_MT		(1 << 31)


#define WSTPAD_TOUCHPAD_DEFAULTS (WSTPAD_F2SCROLL)
#define WSTPAD_CLICKPAD_DEFAULTS \
    (WSTPAD_SOFTBUTTONS | WSTPAD_F2SCROLL)


struct wstpad {
	u_int features;
	u_int handlers;

	/*
	 * t always points into the tpad_touches array, which has at
	 * least one element. If there is more than one, t selects
	 * the pointer-controlling touch.
	 */
	struct tpad_touch *t;
	struct tpad_touch *tpad_touches;

	u_int mtcycle;

	int dx;
	int dy;
	int contacts;
	int prev_contacts;
	u_int btns;
	u_int btns_sync;
	int ratio;

	struct timespec time;

	u_int freeze;
	struct timespec freeze_ts;

	/* edge coordinates */
	struct {
		int left;
		int right;
		int top;
		int bottom;
		int center;
		int center_left;
		int center_right;
		int middle;
	} edge;

	struct {
		/* ratios to the surface width or height */
		int left_edge;
		int right_edge;
		int top_edge;
		int bottom_edge;
		int center_width;
		/* two-finger contacts */
		int f2pressure;
		int f2width;
	} params;

	/* handler state and configuration: */

	u_int softbutton;
	u_int sbtnswap;

	struct {
		int acc_dx;
		int acc_dy;
		int dz;
		int dw;
		int hdist;
		int vdist;
	} scroll;
};

static const size_t cfg_tp[] = {
	[WSMOUSECFG_LEFT_EDGE & 0xff] =
	    offsetof(struct wstpad, params.left_edge),
	[WSMOUSECFG_RIGHT_EDGE & 0xff] =
	    offsetof(struct wstpad, params.right_edge),
	[WSMOUSECFG_TOP_EDGE & 0xff] =
	    offsetof(struct wstpad, params.top_edge),
	[WSMOUSECFG_BOTTOM_EDGE & 0xff] =
	    offsetof(struct wstpad, params.bottom_edge),
	[WSMOUSECFG_CENTERWIDTH & 0xff] =
	    offsetof(struct wstpad, params.center_width),
	[WSMOUSECFG_HORIZSCROLLDIST & 0xff] =
            offsetof(struct wstpad, scroll.hdist),
	[WSMOUSECFG_VERTSCROLLDIST & 0xff] =
            offsetof(struct wstpad, scroll.vdist),
	[WSMOUSECFG_F2WIDTH & 0xff] =
	    offsetof(struct wstpad, params.f2width),
	[WSMOUSECFG_F2PRESSURE & 0xff] =
	    offsetof(struct wstpad, params.f2pressure),

	[WSMOUSECFG_TP_MAX & 0xff] = 0
};


/*
 * Coordinates in the wstpad struct are "normalized" device coordinates,
 * the orientation is left-to-right and upward.
 */
static __inline int
normalize_abs(struct axis_filter *filter, int val)
{
	return (filter->inv ? filter->inv - val : val);
}

static __inline int
normalize_rel(struct axis_filter *filter, int val)
{
	return (filter->inv ? -val : val);
}

/*
 * Directions of motion are represented by numbers in the range 0 - 11,
 * corresponding to clockwise counted circle sectors:
 *
 *              11 | 0
 *           10    |    1
 *          9      |      2
 *          -------+-------
 *          8      |      3
 *            7    |    4
 *               6 | 5
 *
 * Two direction values "match" each other if they are equal or adjacent in
 * this ring. Some handlers require that a movement is "stable" and check
 * the number of matches.
 */
/* Tangent constants in [*.12] fixed-point format: */
#define TAN_DEG_60 7094
#define TAN_DEG_30 2365

#define STABLE	3

#define NORTH(d) ((d) == 0 || (d) == 11)
#define SOUTH(d) ((d) == 5 || (d) == 6)
#define EAST(d) ((d) == 2 || (d) == 3)
#define WEST(d) ((d) == 8 || (d) == 9)

static __inline int
direction(int dx, int dy, int ratio)
{
	int rdy, dir = -1;

	if (dx || dy) {
		rdy = abs(dy) * ratio;
		if (abs(dx) * TAN_DEG_60 < rdy)
			dir = 0;
		else if (abs(dx) * TAN_DEG_30 < rdy)
			dir = 1;
		else
			dir = 2;
		if ((dx < 0) != (dy < 0))
			dir = 5 - dir;
		if (dx < 0)
			dir += 6;
	}
	return dir;
}

static __inline int
dircmp(int dir1, int dir2)
{
	int diff = abs(dir1 - dir2);
	return (diff <= 6 ? diff : 12 - diff);
}

void
wstpad_set_direction(struct tpad_touch *t, int dx, int dy, int ratio)
{
	int dir;

	if (t->state != TOUCH_UPDATE) {
		t->dir = -1;
		t->matches = 0;
	} else {
		dir = direction(dx, dy, ratio);
		if (t->dir >= 0 && dir >= 0 && dircmp(t->dir, dir) <= 1)
			t->matches++;
		else
			t->matches = 1;
		t->dir = dir;
	}
}

/*
 * If a touch starts in an edge area that is configured for softbuttons,
 * scrolling, or palm detection, pointer motion will be suppressed as long
 * as it stays in that area.
 */
static __inline u_int
edge_flags(struct wstpad *tp, int x, int y)
{
	u_int flags = 0;

	if (x < tp->edge.left)
		flags |= L_EDGE;
	else if (x >= tp->edge.right)
		flags |= R_EDGE;
	if (y < tp->edge.bottom)
		flags |= B_EDGE;
	else if (y >= tp->edge.top)
		flags |= T_EDGE;

	return (flags);
}

static __inline struct tpad_touch *
get_2nd_touch(struct wsmouseinput *input)
{
	struct wstpad *tp = input->tp;
	int slot;

	if (IS_MT(tp)) {
		slot = ffs(input->mt.touches & ~input->mt.ptr);
		if (slot)
			return &tp->tpad_touches[--slot];
	}
	return NULL;
}

/* Suppress pointer motion for a short period of time. */
static __inline void
set_freeze_ts(struct wstpad *tp, int sec, int ms)
{
	tp->freeze_ts.tv_sec = sec;
	tp->freeze_ts.tv_nsec = ms * 1000000;
	timespecadd(&tp->time, &tp->freeze_ts, &tp->freeze_ts);
}


/* Return TRUE if f2-/edge-scrolling would be valid. */
static __inline int
chk_scroll_state(struct wstpad *tp)
{
	if (tp->contacts != tp->prev_contacts || tp->btns || tp->btns_sync) {
		tp->scroll.acc_dx = 0;
		tp->scroll.acc_dy = 0;
		return (0);
	}
	return ((tp->dx || tp->dy) && tp->t->matches >= STABLE);
}

void
wstpad_scroll(struct wstpad *tp, int dx, int dy, u_int *cmds)
{
	int sign = 0;

	/* Scrolling is either horizontal or vertical, but not both. */
	if (dy) {
		tp->scroll.acc_dy += dy;
		if (tp->scroll.acc_dy <= -tp->scroll.vdist)
			sign = 1;
		else if (tp->scroll.acc_dy >= tp->scroll.vdist)
			sign = -1;
		if (sign) {
			tp->scroll.acc_dy += sign * tp->scroll.vdist;
			tp->scroll.dz = sign;
			*cmds |= 1 << VSCROLL;
		}
	} else if (dx) {
		tp->scroll.acc_dx += dx;
		if (tp->scroll.acc_dx <= -tp->scroll.hdist)
			sign = -1;
		else if (tp->scroll.acc_dx >= tp->scroll.hdist)
			sign = 1;
		if (sign) {
			tp->scroll.acc_dx -= sign * tp->scroll.hdist;
			tp->scroll.dw = sign;
			*cmds |= 1 << HSCROLL;
		}
	}
}

void
wstpad_f2scroll(struct wsmouseinput *input, u_int *cmds)
{
	struct wstpad *tp = input->tp;
	struct tpad_touch *t2;
	int dir, dx, dy;

	if (tp->contacts != 2 || !chk_scroll_state(tp))
		return;

	dir = tp->t->dir;
	dy = NORTH(dir) || SOUTH(dir) ? tp->dy : 0;
	dx = EAST(dir) || WEST(dir) ? tp->dx : 0;

	if ((dx || dy) && IS_MT(tp)) {
		t2 = get_2nd_touch(input);
		if (t2 == NULL || t2->matches < STABLE)
			return;
		dir = t2->dir;
		if ((dy > 0 && !NORTH(dir)) || (dy < 0 && !SOUTH(dir)))
			return;
		if ((dx > 0 && !EAST(dir)) || (dx < 0 && !WEST(dir)))
			return;
	}

	wstpad_scroll(tp, dx, dy, cmds);
	set_freeze_ts(tp, 0, FREEZE_MS);
}

void
wstpad_edgescroll(struct wsmouseinput *input, u_int *cmds)
{
	struct wstpad *tp = input->tp;
	struct tpad_touch *t = tp->t;
	int dx = 0, dy = 0;

	if (tp->contacts != 1 || !chk_scroll_state(tp))
		return;

	if (tp->features & WSTPAD_SWAPSIDES) {
		if (t->flags & L_EDGE)
			dy = tp->dy;
	} else if (t->flags & R_EDGE) {
		dy = tp->dy;
	} else if ((t->flags & B_EDGE) &&
	    (tp->features & WSTPAD_HORIZSCROLL)) {
		dx = tp->dx;
	}
	if (dx || dy)
		wstpad_scroll(tp, dx, dy, cmds);
}

static __inline u_int
sbtn(struct wstpad *tp, int x, int y)
{
	if (y >= tp->edge.bottom)
		return (0);
	if ((tp->features & WSTPAD_SOFTMBTN)
	    && x >= tp->edge.center_left
	    && x < tp->edge.center_right)
		return (MIDDLEBTN);
	return ((x < tp->edge.center ? LEFTBTN : RIGHTBTN) ^ tp->sbtnswap);
}

static __inline u_int
top_sbtn(struct wstpad *tp, int x, int y)
{
	if (y < tp->edge.top)
		return (0);
	if (x < tp->edge.center_left)
		return (LEFTBTN ^ tp->sbtnswap);
	return (x > tp->edge.center_right
	    ? (RIGHTBTN ^ tp->sbtnswap) : MIDDLEBTN);
}

u_int
wstpad_get_sbtn(struct wsmouseinput *input, int top)
{
	struct wstpad *tp = input->tp;
	struct tpad_touch *t = tp->t;
	u_int btn;

	btn = 0;
	if (tp->contacts) {
		btn = top ? top_sbtn(tp, t->x, t->y) : sbtn(tp, t->x, t->y);
		/*
		 * If there is no middle-button area, but contacts in both
		 * halves of the edge zone, generate a middle-button event:
		 */
		if (btn && IS_MT(tp) && tp->contacts == 2
		    && !top && !(tp->features & WSTPAD_SOFTMBTN)) {
			if ((t = get_2nd_touch(input)) != NULL)
				btn |= sbtn(tp, t->x, t->y);
			if (btn == (LEFTBTN | RIGHTBTN))
				btn = MIDDLEBTN;
		}
	}
	return (btn != PRIMARYBTN ? btn : 0);
}

void
wstpad_softbuttons(struct wsmouseinput *input, u_int *cmds, int hdlr)
{
	struct wstpad *tp = input->tp;
	int top = (hdlr == TOPBUTTON_HDLR);

	if (tp->softbutton && PRIMARYBTN_RELEASED(tp)) {
		*cmds |= 1 << SOFTBUTTON_UP;
		return;
	}

	if (tp->softbutton == 0 && PRIMARYBTN_CLICKED(tp)) {
		tp->softbutton = wstpad_get_sbtn(input, top);
		if (tp->softbutton)
			*cmds |= 1 << SOFTBUTTON_DOWN;
	}
}

/*
 * Suppress accidental pointer movements after a click on a clickpad.
 */
void
wstpad_click(struct wsmouseinput *input)
{
	struct wstpad *tp = input->tp;

	if (tp->contacts == 1 &&
	    (PRIMARYBTN_CLICKED(tp) || PRIMARYBTN_RELEASED(tp)))
		set_freeze_ts(tp, 0, FREEZE_MS);
}

/*
 * Translate the "command" bits into the sync-state of wsmouse, or into
 * wscons events.
 */
void
wstpad_cmds(struct wsmouseinput *input, struct evq_access *evq, u_int cmds)
{
	struct wstpad *tp = input->tp;
	u_int sbtns_dn = 0, sbtns_up = 0;
	int n;

	FOREACHBIT(cmds, n) {
		switch (n) {
		case CLEAR_MOTION_DELTAS:
			input->motion.dx = input->motion.dy = 0;
			if (input->motion.dz == 0 && input->motion.dw == 0)
				input->motion.sync &= ~SYNC_DELTAS;
			continue;
		case SOFTBUTTON_DOWN:
			input->btn.sync &= ~PRIMARYBTN;
			sbtns_dn |= tp->softbutton;
			continue;
		case SOFTBUTTON_UP:
			input->btn.sync &= ~PRIMARYBTN;
			sbtns_up |= tp->softbutton;
			tp->softbutton = 0;
			continue;
		case HSCROLL:
			input->motion.dw = tp->scroll.dw;
			input->motion.sync |= SYNC_DELTAS;
			continue;
		case VSCROLL:
			input->motion.dz = tp->scroll.dz;
			input->motion.sync |= SYNC_DELTAS;
			continue;
		default:
			printf("[wstpad] invalid cmd %d\n", n);
			break;
		}
	}
	if (sbtns_dn || sbtns_up) {
		input->sbtn.buttons |= sbtns_dn;
		input->sbtn.buttons &= ~sbtns_up;
		input->sbtn.sync |= (sbtns_dn | sbtns_up);
	}
}


/*
 * Set the state of touches that have ended. TOUCH_END is a transitional
 * state and will be changed to TOUCH_NONE before process_input() returns.
 */
static __inline void
clear_touchstates(struct wsmouseinput *input, enum touchstates state)
{
	u_int touches;
	int slot;

	touches = input->mt.sync[MTS_TOUCH] & ~input->mt.touches;
	FOREACHBIT(touches, slot)
		input->tp->tpad_touches[slot].state = state;
}

void
wstpad_mt_inputs(struct wsmouseinput *input)
{
	struct wstpad *tp = input->tp;
	struct tpad_touch *t;
	struct mt_slot *mts;
	int slot, dx, dy;
	u_int touches, inactive;

	/* TOUCH_BEGIN */
	touches = input->mt.touches & input->mt.sync[MTS_TOUCH];
	FOREACHBIT(touches, slot) {
		t = &tp->tpad_touches[slot];
		t->state = TOUCH_BEGIN;
		mts = &input->mt.slots[slot];
		t->x = normalize_abs(&input->filter.h, mts->x);
		t->y = normalize_abs(&input->filter.v, mts->y);
		t->orig.x = t->x;
		t->orig.y = t->y;
		memcpy(&t->orig.time, &tp->time, sizeof(struct timespec));
		t->flags = edge_flags(tp, t->x, t->y);
		wstpad_set_direction(t, 0, 0, tp->ratio);
	}

	/* TOUCH_UPDATE */
	touches = input->mt.touches & input->mt.frame;
	if (touches & tp->mtcycle) {
		/*
		 * Slot data may be synchronized separately, in any order,
		 * or not at all if they are "inactive" and there is no delta.
		 */
		inactive = input->mt.touches & ~tp->mtcycle;
		tp->mtcycle = touches;
	} else {
		inactive = 0;
		tp->mtcycle |= touches;
	}
	touches = input->mt.touches & ~input->mt.sync[MTS_TOUCH];
	FOREACHBIT(touches, slot) {
		t = &tp->tpad_touches[slot];
		t->state = TOUCH_UPDATE;
		if ((1 << slot) & input->mt.frame) {
			mts = &input->mt.slots[slot];
			dx = normalize_abs(&input->filter.h, mts->x) - t->x;
			t->x += dx;
			dy = normalize_abs(&input->filter.v, mts->y) - t->y;
			t->y += dy;
			t->flags &= (~EDGES | edge_flags(tp, t->x, t->y));
			wstpad_set_direction(t, dx, dy, tp->ratio);
		} else if ((1 << slot) & inactive) {
			wstpad_set_direction(t, 0, 0, tp->ratio);
		}
	}

	clear_touchstates(input, TOUCH_END);
}

void
wstpad_touch_inputs(struct wsmouseinput *input)
{
	struct wstpad *tp = input->tp;
	struct tpad_touch *t;
	int slot;

	tp->dx = normalize_rel(&input->filter.h, input->motion.dx);
	tp->dy = normalize_rel(&input->filter.v, input->motion.dy);
	tp->btns = input->btn.buttons;
	tp->btns_sync = input->btn.sync;

	tp->prev_contacts = tp->contacts;
	tp->contacts = input->touch.contacts;

	if (tp->contacts == 1 &&
	    ((tp->params.f2width &&
	    input->touch.width >= tp->params.f2width)
	    || (tp->params.f2pressure &&
	    input->touch.pressure >= tp->params.f2pressure)))
		tp->contacts = 2;

	if (IS_MT(tp)) {
		wstpad_mt_inputs(input);
		if (input->mt.ptr) {
			slot = ffs(input->mt.ptr) - 1;
			tp->t = &tp->tpad_touches[slot];
		}
	} else {
		t = tp->t;
		t->x = normalize_abs(&input->filter.h, input->motion.x);
		t->y = normalize_abs(&input->filter.v, input->motion.y);
		if (tp->contacts)
			t->state = (tp->prev_contacts ?
			    TOUCH_UPDATE : TOUCH_BEGIN);
		else
			t->state = (tp->prev_contacts ?
			    TOUCH_END : TOUCH_NONE);

		if (t->state == TOUCH_BEGIN) {
			t->orig.x = t->x;
			t->orig.y = t->y;
			memcpy(&t->orig.time, &tp->time,
			    sizeof(struct timespec));
			t->flags = edge_flags(tp, t->x, t->y);
		} else {
			t->flags &= (~EDGES | edge_flags(tp, t->x, t->y));
		}

		/* Use unfiltered deltas for determining directions: */
		wstpad_set_direction(t,
		    normalize_rel(&input->filter.h, input->motion.x_delta),
		    normalize_rel(&input->filter.v, input->motion.y_delta),
		    input->filter.ratio);
	}
}

void
wstpad_process_input(struct wsmouseinput *input, struct evq_access *evq)
{
	struct wstpad *tp = input->tp;
	u_int handlers, hdlr, cmds;

	memcpy(&tp->time, &evq->ts, sizeof(struct timespec));
	wstpad_touch_inputs(input);

	cmds = 0;
	handlers = tp->handlers;
	if (DISABLE(tp))
		handlers &= (1 << TOPBUTTON_HDLR);

	FOREACHBIT(handlers, hdlr) {
		switch (hdlr) {
		case SOFTBUTTON_HDLR:
		case TOPBUTTON_HDLR:
			wstpad_softbuttons(input, &cmds, hdlr);
			continue;
		case F2SCROLL_HDLR:
			wstpad_f2scroll(input, &cmds);
			continue;
		case EDGESCROLL_HDLR:
			wstpad_edgescroll(input, &cmds);
			continue;
		case CLICK_HDLR:
			wstpad_click(input);
			continue;
		}
	}

	/* Check whether the motion deltas should be cleared. */
	if (tp->dx || tp->dy) {
		if (DISABLE(tp)
		    || (tp->t->flags & tp->freeze)
		    || timespeccmp(&tp->time, &tp->freeze_ts, <)
		    /* Is there more than one contact, and no click-and-drag? */
		    || tp->contacts > 2
		    || (tp->contacts == 2 && !(tp->btns & PRIMARYBTN))) {

			cmds |= 1 << CLEAR_MOTION_DELTAS;
		}
	}

	wstpad_cmds(input, evq, cmds);

	if (IS_MT(tp))
		clear_touchstates(input, TOUCH_NONE);
}

/*
 * Try to determine the average interval between two updates. Various
 * conditions are checked in order to ensure that only valid samples enter
 * into the calculation. Above all, it is restricted to motion events
 * occuring when there is only one contact. MT devices may need more than
 * one packet to transmit their state if there are multiple touches, and
 * the update frequency may be higher in this case.
 */
void
wstpad_track_interval(struct wsmouseinput *input, struct timespec *time)
{
	static const struct timespec limit = { 0, 30 * 1000000L };
	struct timespec ts;
	int samples;

	if (input->motion.sync == 0
	    || (input->touch.sync & SYNC_CONTACTS)
	    || (input->touch.contacts > 1)) {
		input->intv.track = 0;
		return;
	}
	if (input->intv.track) {
		timespecsub(time, &input->intv.ts, &ts);
		if (timespeccmp(&ts, &limit, <)) {
			/* The unit of the sum is 4096 nanoseconds. */
			input->intv.sum += ts.tv_nsec >> 12;
			samples = ++input->intv.samples;
			/*
			 * Make the first calculation quickly and later
			 * a more reliable one:
			 */
			if (samples == 8) {
				input->intv.avg = input->intv.sum << 9;
				wstpad_init_deceleration(input);
			} else if (samples == 128) {
				input->intv.avg = input->intv.sum << 5;
				wstpad_init_deceleration(input);
				input->intv.samples = 0;
				input->intv.sum = 0;
				input->flags &= ~TRACK_INTERVAL;
			}
		}
	}
	memcpy(&input->intv.ts, time, sizeof(struct timespec));
	input->intv.track = 1;
}

/*
 * If hysteresis values are set, the sum of the output motion deltas will
 * lag behind the input deltas within a window bounded by +[hysteresis]
 * and -[hysteresis]. It may suppress accidental movements when a touch
 * starts or ends, or when small corrections with changing directions are
 * being made. In the synaptics driver the default threshold is 0.5 percent
 * of the diagonal of the touchpad surface, the wstpad default is about
 * 0.36 percent. The fields filter.h.acc and filter.v.acc accumulate the
 * deltas up to the threshold values.
 */
void
wstpad_hysteresis(int *acc, int *delta, int hysteresis)
{
	*acc += *delta;
	if (*acc > hysteresis)
		*delta = *acc - hysteresis;
	else if (*acc < -hysteresis)
		*delta = *acc + hysteresis;
	else
		*delta = 0;
	*acc -= *delta;
}

/*
 * The default acceleration options of X don't work convincingly with
 * touchpads (the synaptics driver installs its own "acceleration
 * profile" and callback function). As a preliminary workaround, this
 * filter applies a simple deceleration scheme to small deltas. Based
 * on an "alpha-max-plus-beta-min" approximation to the distance, it
 * assigns a "magnitude" to a delta pair. A value of 8 corresponds,
 * roughly, to a speed of (filter.dclr / 12.5) device units per milli-
 * second. If its magnitude is smaller than 7 a delta will be downscaled
 * by the factor 2/8, deltas with magnitudes from 7 to 11 by factors
 * ranging from 3/8 to 7/8.
 */
void
wstpad_decelerate(struct wsmouseinput *input, int *dx, int *dy)
{
	int h = abs(*dx) * input->filter.h.mag_scale;
	int v = abs(*dy) * input->filter.v.mag_scale;
	int mag = (h >= v ? h + 3 * v / 8 : v + 3 * h / 8);
	int n;

	/* Don't change deceleration levels abruptly. */
	mag = (mag + 7 * input->filter.mag) / 8;
	/* Don't use arbitrarily high values. */
	input->filter.mag = imin(mag, 24 << 12);

	n = imax((mag >> 12) - 4, 2);
	if (n < 8) {
		/* Scale by (n / 8). */
		h = *dx * n + input->filter.h.dclr_rmdr;
		v = *dy * n + input->filter.v.dclr_rmdr;
		input->filter.h.dclr_rmdr = (h >= 0 ? h & 7 : -(-h & 7));
		input->filter.v.dclr_rmdr = (v >= 0 ? v & 7 : -(-v & 7));
		*dx = h / 8;
		*dy = v / 8;
	}
}

/*
 * Compatibility-mode conversions. This function transforms and filters
 * the coordinate inputs, extended functionality is provided by
 * wstpad_process_input.
 */
void
wstpad_compat_convert(struct wsmouseinput *input, struct evq_access *evq)
{
	struct axis_filter *h = &input->filter.h;
	struct axis_filter *v = &input->filter.v;
	int dx, dy;

	if (input->flags & TRACK_INTERVAL)
		wstpad_track_interval(input, &evq->ts);

	dx = (input->motion.sync & SYNC_X) ? input->motion.x_delta : 0;
	dy = (input->motion.sync & SYNC_Y) ? input->motion.y_delta : 0;

	if ((h->dmax && (abs(dx) > h->dmax))
	    || (v->dmax && (abs(dy) > v->dmax)))
		dx = dy = 0;

	if (dx || dy) {
		if (input->touch.sync & SYNC_CONTACTS)
			h->acc = v->acc = 0;
		if (h->hysteresis)
			wstpad_hysteresis(&h->acc, &dx, h->hysteresis);
		if (v->hysteresis)
			wstpad_hysteresis(&v->acc, &dy, v->hysteresis);
		if (input->filter.dclr)
			wstpad_decelerate(input, &dx, &dy);
		input->motion.dx = dx;
		input->motion.dy = dy;
		if ((dx || dy) && !(input->motion.sync & SYNC_DELTAS)) {
			input->motion.dz = input->motion.dw = 0;
			input->motion.sync |= SYNC_DELTAS;
		}
	}

	if (input->tp != NULL)
		wstpad_process_input(input, evq);

	input->motion.sync &= ~SYNC_POSITION;
	input->touch.sync = 0;
}

int
wstpad_init(struct wsmouseinput *input)
{
	struct wstpad *tp = input->tp;
	int slots;

	if (tp != NULL)
		return (0);

	input->tp = tp = malloc(sizeof(struct wstpad),
	    M_DEVBUF, M_WAITOK | M_ZERO);
	if (tp == NULL)
		return (-1);

	slots = imax(input->mt.num_slots, 1);
	tp->tpad_touches = malloc(slots * sizeof(struct tpad_touch),
	    M_DEVBUF, M_WAITOK | M_ZERO);
	if (tp->tpad_touches == NULL) {
		free(tp, M_DEVBUF, sizeof(struct wstpad));
		return (-1);
	}

	tp->t = &tp->tpad_touches[0];
	if (input->mt.num_slots)
		tp->features |= WSTPAD_MT;

	tp->ratio = input->filter.ratio;

	return (0);
}

/*
 * Integer square root (Halleck's method)
 *
 * An adaption of code from John B. Halleck (from
 * http://www.cc.utah.edu/~nahaj/factoring/code.html). This version is
 * used and published under the OpenBSD license terms with his permission.
 *
 * Cf. also Martin Guy's "Square root by abacus" method.
 */
static __inline u_int
isqrt(u_int n)
{
	u_int root, sqbit;

	root = 0;
	sqbit = 1 << (sizeof(u_int) * 8 - 2);
	while (sqbit) {
		if (n >= (sqbit | root)) {
			n -= (sqbit | root);
			root = (root >> 1) | sqbit;
		} else {
			root >>= 1;
		}
		sqbit >>= 2;
	}
	return (root);
}

void
wstpad_init_deceleration(struct wsmouseinput *input)
{
	int n, dclr;

	if ((dclr = input->filter.dclr) == 0)
		return;

	dclr = imax(dclr, 4);

	/*
	 * For a standard update rate of about 80Hz, (dclr) units
	 * will be mapped to a magnitude of 8. If the average rate
	 * is significantly higher or lower, adjust the coefficient
	 * accordingly:
	 */
	if (input->intv.avg == 0) {
		n = 8;
	} else {
		n = 8 * 13000000 / input->intv.avg;
		n = imax(imin(n, 32), 4);
	}
	input->filter.h.mag_scale = (n << 12) / dclr;
	input->filter.v.mag_scale = (input->filter.ratio ?
	    n * input->filter.ratio : n << 12) / dclr;
	input->filter.h.dclr_rmdr = 0;
	input->filter.v.dclr_rmdr = 0;
	input->flags |= TRACK_INTERVAL;
}

int
wstpad_configure(struct wsmouseinput *input)
{
	struct wstpad *tp;
	int width, height, diag, h_unit, v_unit;

	/*
	 * TODO: The default configurations don't use the resolution values
	 * and the X/Y ratio yet. The parameters are derived from the length
	 * of the diagonal in device units, with some magic constants which
	 * are partly adapted from libinput or synaptics code, or are based
	 * on tests and guess work.
	 */
	width = abs(input->hw.x_max - input->hw.x_min);
	height = abs(input->hw.y_max - input->hw.y_min);
	if (width == 0 || height == 0)
		return (-1);	/* We can't do anything. */

	diag = isqrt(width * width + height * height);
	h_unit = v_unit = imax(diag / 280, 3);

	if (input->tp == NULL && wstpad_init(input))
		return (-1);
	tp = input->tp;

	if (!(input->flags & CONFIGURED)) {
		input->filter.h.scale = (imin(920, diag) << 12) / diag;
		input->filter.v.scale = input->filter.h.scale;
		input->filter.h.hysteresis = h_unit;
		input->filter.v.hysteresis = v_unit;
		input->filter.dclr = h_unit - h_unit / 5;
		wstpad_init_deceleration(input);

		tp->params.top_edge = EDGERATIO_DEFAULT;
		tp->params.bottom_edge = EDGERATIO_DEFAULT;
		tp->params.left_edge = EDGERATIO_DEFAULT;
		tp->params.right_edge = EDGERATIO_DEFAULT;
		tp->params.center_width = CENTERWIDTH_DEFAULT;

		tp->features &= (WSTPAD_MT | WSTPAD_DISABLE);
		if (input->hw.hw_type == WSMOUSEHW_TOUCHPAD)
			tp->features |= WSTPAD_TOUCHPAD_DEFAULTS;
		else
			tp->features |= WSTPAD_CLICKPAD_DEFAULTS;
		if (input->hw.contacts_max == 1) {
			tp->features &= ~WSTPAD_F2SCROLL;
			tp->features |= WSTPAD_EDGESCROLL;
		}
		tp->scroll.hdist = 5 * h_unit;
		tp->scroll.vdist = 5 * v_unit;
	}

	tp->edge.left = input->hw.x_min +
	    width * tp->params.left_edge / 4096;
	tp->edge.right = input->hw.x_max -
	    width * tp->params.right_edge / 4096;
	tp->edge.bottom = input->hw.y_min +
	    height * tp->params.bottom_edge / 4096;
	tp->edge.top = input->hw.y_max -
	    height * tp->params.top_edge / 4096;
	tp->edge.center = (input->hw.x_min + input->hw.x_max) / 2;
	tp->edge.center_left = tp->edge.center -
	    width * tp->params.center_width / 8192;
	tp->edge.center_right = tp->edge.center +
	    width * tp->params.center_width / 8192;
	tp->edge.middle = (input->hw.y_max - input->hw.y_min) / 2;

	tp->handlers = 0;
	tp->freeze = 0;

	if (tp->features & WSTPAD_SOFTBUTTONS) {
		tp->handlers |= 1 << SOFTBUTTON_HDLR;
		tp->freeze |= B_EDGE;
	}
	if (tp->features & WSTPAD_TOPBUTTONS) {
		tp->handlers |= 1 << TOPBUTTON_HDLR;
		tp->freeze |= T_EDGE;
	}

	if (tp->features & WSTPAD_F2SCROLL) {
		tp->handlers |= 1 << F2SCROLL_HDLR;
	} else if (tp->features & WSTPAD_EDGESCROLL) {
		tp->handlers |= 1 << EDGESCROLL_HDLR;
		tp->freeze |=
		    ((tp->features & WSTPAD_SWAPSIDES) ? L_EDGE : R_EDGE);
	}

	if (input->hw.hw_type == WSMOUSEHW_CLICKPAD)
		tp->handlers |= 1 << CLICK_HDLR;

	tp->sbtnswap = ((tp->features & WSTPAD_SWAPSIDES)
	    ? (LEFTBTN | RIGHTBTN) : 0);

	return (0);
}

void
wstpad_reset(struct wsmouseinput *input)
{
	struct wstpad *tp = input->tp;

	if (tp == NULL)
		return;
	if (input->sbtn.buttons) {
		input->sbtn.sync = input->sbtn.buttons;
		input->sbtn.buttons = 0;
	}
}

int
wstpad_set_params(struct wsmouseinput *input,
    const struct wsmouse_param *params, u_int nparams)
{
	struct wstpad *tp = input->tp;
	int i, key, val;
	u_int flag;
	void *p;

	if (tp == NULL)
		return (-1);

	for (i = 0; i < nparams; i++) {
		key = params[i].key;
		val = params[i].value;
		if (WSMOUSECFG_MATCH(key, TP_OPTS)) {
			flag = 1 << (key & 0xff);
			if (val)
				tp->features |= flag;
			else
				tp->features &= ~flag;
		} else { /* WSMOUSECFG_MATCH(k, TP) */
			p = tp;
			p += cfg_tp[key & 0xff];
			if (p != tp) {
				*((int *) p) = val;
			} else {
				printf("wstpad_set_params: "
				    "ignoring key %d\n", key);
			}
		}
	}
	wstpad_reset(input);
	return wstpad_configure(input);
}

int
wstpad_get_params(struct wsmouseinput *input,
    struct wsmouse_param *params, u_int nparams)
{
	struct wstpad *tp = input->tp;
	int i, key;
	u_int flag;
	void *p;

	if (tp == NULL)
		return (-1);
	for (i = 0; i < nparams; i++) {
		key = params[i].key;
		if (WSMOUSECFG_MATCH(key, TP_OPTS)) {
			flag = 1 << (key & 0xff);
			params[i].value = !!(tp->features & flag);
		} else { /* WSMOUSECFG_MATCH(k, TP) */
			p = tp;
			p += cfg_tp[key & 0xff];
			if (p != tp)
				params[i].value = *((int *) p);
			else
				printf("wstpad_get_params: "
				    "ignoring key %d\n", key);
		}
	}

	return (0);
}
