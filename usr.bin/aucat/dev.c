/*	$OpenBSD: dev.c,v 1.1 2008/08/14 09:58:55 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <err.h>

#include "dev.h"
#include "abuf.h"
#include "aproc.h"
#include "file.h"
#include "conf.h"

int quit_flag, pause_flag;
unsigned dev_infr, dev_onfr;
struct aparams dev_ipar, dev_opar;
struct aproc *dev_mix, *dev_sub, *dev_rec, *dev_play;
struct file  *dev_file;
struct devops *devops = &devops_sun;

/*
 * SIGINT handler, it raises the quit flag. If the flag is already set,
 * that means that the last SIGINT was not handled, because the process
 * is blocked somewhere, so exit
 */
void
sigint(int s)
{
	if (quit_flag)
		_exit(1);
	quit_flag = 1;
}

/*
 * called when the user hits ctrl-z
 */
void
sigtstp(int s)
{
	pause_flag = 1;
}

/*
 * SIGCONT is send when resumed after SIGTSTP or SIGSTOP. If the pause
 * flag is not set, that means that the process was not suspended by
 * dev_suspend(), which means that we lost the sync; since we cannot
 * resync, just exit
 */
void
sigcont(int s)
{
	static char msg[] = "can't resume afer SIGSTOP, terminating...\n";
	
	if (!pause_flag) {
		write(STDERR_FILENO, msg, sizeof(msg) - 1);
		_exit(1);
	}
}

/*
 * suicide with SIGTSTP (tty stop) as if the user had hit ctrl-z
 */
void
dev_suspend(void)
{
	struct sigaction sa;

	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_DFL;
	if (sigaction(SIGTSTP, &sa, NULL) < 0)
		err(1, "sigaction");
	DPRINTF("suspended by tty\n");
	kill(getpid(), SIGTSTP);
	pause_flag = 0;
	sa.sa_handler = sigtstp;
	if (sigaction(SIGTSTP, &sa, NULL) < 0)
		err(1, "sigaction");
	DPRINTF("resumed after suspend\n");
}

/*
 * fill playback buffer, so when device is started there
 * are samples to play
 */
void
dev_fill(void)
{
	struct abuf *buf;


	/*
	 * if there are no inputs, zero fill the mixer
	 */
	if (dev_mix && LIST_EMPTY(&dev_mix->ibuflist))
		mix_pushzero(dev_mix);
	DPRINTF("filling play buffers...\n");	
	for (;;) {
		if (!dev_file->wproc) {
			DPRINTF("fill: no writer\n");
			break;
		}
		if (dev_file->events & POLLOUT) {
			/*
			 * kernel buffers are full, but continue
			 * until the play buffer is full too.
			 */
			buf = LIST_FIRST(&dev_file->wproc->ibuflist);
			if (!ABUF_WOK(buf))
				break;		/* buffer full */
			if (!buf->wproc)
				break;		/* will never be filled */
		}
		if (!file_poll())
			break;
		if (pause_flag)
			dev_suspend();
	}
}

/*
 * flush recorded samples once the device is stopped so
 * they aren't lost
 */
void
dev_flush(void)
{
	struct abuf *buf;

	DPRINTF("flushing record buffers...\n");
	for (;;) {
		if (!dev_file->rproc) {
			DPRINTF("flush: no more reader\n");
			break;
		}
		if (dev_file->events & POLLIN) {
			/*
			 * we drained kernel buffers, but continue
			 * until the record buffer is empty.
			 */
			buf = LIST_FIRST(&dev_file->rproc->obuflist);
			if (!ABUF_ROK(buf))
				break;		/* buffer empty */
			if (!buf->rproc)
				break;		/* will never be drained */
		}
		if (!file_poll())
			break;
		if (pause_flag)
			dev_suspend();
	}
}


/*
 * open the device with the given hardware parameters and create a mixer
 * and a multiplexer connected to it with all necessary conversions
 * setup
 */
void
dev_init(char *devpath, struct aparams *dipar, struct aparams *dopar)
{
	int fd;
	struct sigaction sa;
	unsigned infr, onfr;
	struct aparams ipar, opar;
	struct aproc *conv;
	struct abuf *buf;

	quit_flag = 0;
	pause_flag = 0;

	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sigint;
	if (sigaction(SIGINT, &sa, NULL) < 0)
		err(1, "sigaction");
	sa.sa_handler = sigtstp;
	if (sigaction(SIGTSTP, &sa, NULL) < 0)
		err(1, "sigaction");
	sa.sa_handler = sigcont;
	if (sigaction(SIGCONT, &sa, NULL) < 0)
		err(1, "sigaction");

	fd = devops->open(devpath, dipar, dopar, &infr, &onfr);
	if (fd < 0)
		exit(1);
	dev_file = file_new(fd, devpath);

	/*
	 * create record chain
	 */
	if (dipar) {
		aparams_init(&ipar, dipar->cmin, dipar->cmax, dipar->rate);
		infr *= DEFAULT_NBLK;

		/*
		 * create the read end
		 */
		dev_rec = rpipe_new(dev_file);
		buf = abuf_new(infr, aparams_bpf(dipar));
		aproc_setout(dev_rec, buf);

		/*
		 * append a converter, if needed
		 */
		if (!aparams_eq(dipar, &ipar)) {
			if (debug_level > 0) {
				fprintf(stderr, "%s: ", devpath);
				aparams_print2(dipar, &ipar);
				fprintf(stderr, "\n");
			}
			conv = conv_new("subconv", dipar, &ipar);
			aproc_setin(conv, buf);
			buf = abuf_new(infr, aparams_bpf(&ipar));
			aproc_setout(conv, buf);
		}
		dev_ipar = ipar;
		dev_infr = infr;

		/*
		 * append a "sub" to which clients will connect
		 */
		dev_sub = sub_new();
		aproc_setin(dev_sub, buf);
	} else {
		dev_rec = NULL;
		dev_sub = NULL;
	}

	/*
	 * create play chain
	 */
	if (dopar) {
		aparams_init(&opar, dopar->cmin, dopar->cmax, dopar->rate);
		onfr *= DEFAULT_NBLK;	

		/*
		 * create the write end
		 */
		dev_play = wpipe_new(dev_file);
		buf = abuf_new(onfr, aparams_bpf(dopar));
		aproc_setin(dev_play, buf);

		/*
		 * append a converter, if needed
		 */
		if (!aparams_eq(&opar, dopar)) {
			if (debug_level > 0) {
				fprintf(stderr, "%s: ", devpath);
				aparams_print2(&opar, dopar);
				fprintf(stderr, "\n");
			}
			conv = conv_new("mixconv", &opar, dopar);
			aproc_setout(conv, buf);
			buf = abuf_new(onfr, aparams_bpf(&opar));
			aproc_setin(conv, buf);
			*dopar = opar;
		}
		dev_opar = opar;
		dev_onfr = onfr;

		/*
		 * append a "mix" to which clients will connect
		 */
		dev_mix = mix_new();
		aproc_setout(dev_mix, buf);
	} else {
		dev_play = NULL;
		dev_mix = NULL;
	}
}

/*
 * cleanly stop and drain everything and close the device
 * once both play chain and record chain are gone
 */
void
dev_done(void)
{
	struct sigaction sa;
	struct file *f;

	/*
	 * generate EOF on all inputs (including device), so once
	 * buffers are drained, everything will be cleaned
	 */
	LIST_FOREACH(f, &file_list, entry) {
		if (f->rproc)
			file_eof(f);
	}
	/*
	 * destroy automatically mixe instead
	 * of generating silence
	 */
	if (dev_mix)
		dev_mix->u.mix.flags |= MIX_AUTOQUIT;
	if (dev_sub)
		dev_sub->u.sub.flags |= SUB_AUTOQUIT;
	/*
	 * drain buffers of terminated inputs.
	 */
	for (;;) {
		if (!file_poll())
			break;
	}
	devops->close(dev_file->fd);

	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_DFL;
	if (sigaction(SIGINT, &sa, NULL) < 0)
		err(1, "sigaction");
	if (sigaction(SIGTSTP, &sa, NULL) < 0)
		err(1, "sigaction");
	if (sigaction(SIGCONT, &sa, NULL) < 0)
		err(1, "sigaction");
}

/*
 * start the (paused) device. By default it's paused
 */
void
dev_start(void)
{
	dev_fill();
	if (dev_mix)
		dev_mix->u.mix.flags |= MIX_DROP;
	if (dev_sub)
		dev_sub->u.sub.flags |= SUB_DROP;
	devops->start(dev_file->fd);
}

/*
 * pause the device
 */
void
dev_stop(void)
{
	devops->stop(dev_file->fd);
	if (dev_mix)
		dev_mix->u.mix.flags &= ~MIX_DROP;
	if (dev_sub)
		dev_sub->u.sub.flags &= ~SUB_DROP;
	dev_flush();
}

/*
 * loop until there's either input or output to process
 */
void
dev_run(int autoquit)
{
	while (!quit_flag) {
		if ((!dev_mix || LIST_EMPTY(&dev_mix->ibuflist)) &&
		    (!dev_sub || LIST_EMPTY(&dev_sub->obuflist)) && autoquit)
			break;
		if (!file_poll())
			break;
		if (pause_flag) {
			devops->stop(dev_file->fd);
			dev_flush();
			dev_suspend();
			dev_fill();
			devops->start(dev_file->fd);
		}
	}
}

/*
 * attach the given input and output buffers to the mixer and the
 * multiplexer respectively. The operation is done synchronously, so
 * both buffers enter in sync. If buffers do not match play
 * and rec
 */
void
dev_attach(char *name, 
    struct abuf *ibuf, struct aparams *ipar, unsigned underrun, 
    struct abuf *obuf, struct aparams *opar, unsigned overrun)
{
	int delta;
	struct abuf *pbuf = NULL, *rbuf = NULL;
	struct aproc *conv;
	
	if (ibuf) {
		pbuf = LIST_FIRST(&dev_mix->obuflist);		
		if (!aparams_eq(ipar, &dev_opar)) {
			if (debug_level > 1) {
				fprintf(stderr, "dev_attach: %s: ", name);
				aparams_print2(ipar, &dev_opar);
				fprintf(stderr, "\n");
			}
			conv = conv_new(name, ipar, &dev_opar);
			aproc_setin(conv, ibuf);
			ibuf = abuf_new(dev_onfr, aparams_bpf(&dev_opar));
			aproc_setout(conv, ibuf);
		}
		aproc_setin(dev_mix, ibuf);
		ibuf->xrun = underrun;
		mix_setmaster(dev_mix);
	}
	if (obuf) {
		rbuf = LIST_FIRST(&dev_sub->ibuflist);
		if (!aparams_eq(opar, &dev_ipar)) {
			if (debug_level > 1) {
				fprintf(stderr, "dev_attach: %s: ", name);
				aparams_print2(&dev_ipar, opar);
				fprintf(stderr, "\n");
			}
			conv = conv_new(name, &dev_ipar, opar);
			aproc_setout(conv, obuf);
			obuf = abuf_new(dev_infr, aparams_bpf(&dev_ipar));
			aproc_setin(conv, obuf);
		}
		aproc_setout(dev_sub, obuf);
		obuf->xrun = overrun;
	}

	/*
	 * calculate delta, the number of frames the play chain is ahead
	 * of the record chain. It's necessary to schedule silences (or
	 * drops) in order to start playback and record in sync.
	 */
	if (ibuf && obuf) {
		delta = 
		    rbuf->bpf * (pbuf->abspos + pbuf->used) - 
		    pbuf->bpf *  rbuf->abspos;
		delta /= pbuf->bpf * rbuf->bpf;
		DPRINTF("dev_attach: ppos = %u, pused = %u, rpos = %u\n",
		    pbuf->abspos, pbuf->used, rbuf->abspos);
	} else
		delta = 0;
	DPRINTF("dev_attach: delta = %u\n", delta);

	if (delta > 0) {
		/*
		 * if the play chain is ahead (most cases) drop some of
		 * the recorded input, to get both in sync
		 */
		obuf->drop += delta * obuf->bpf;
	} else if (delta < 0) {
		/*
		 * if record chain is ahead (should never happen,
		 * right?) then insert silence to play
		 */
		ibuf->silence += -delta * ibuf->bpf;
	}
	if (ibuf && (dev_mix->u.mix.flags & MIX_DROP)) {
		DPRINTF("lmkqsjdlkqsjklqsd\n");
		/*
		 * fill the play buffer with silence to avoid underruns,
		 * drop samples on the input to keep play/record in sync
		 * after the silence insertion
		 */
		ibuf->silence += dev_onfr * ibuf->bpf;
		if (obuf)
			obuf->drop += dev_onfr * obuf->bpf;
		/*
		 * force data to propagate
		 */
		abuf_run(ibuf);
		DPRINTF("dev_attach: ibuf: used = %u, silence = %u\n", 
		    ibuf->used, ibuf->silence);
	}
	if (obuf && (dev_sub->u.mix.flags & SUB_DROP)) {
		abuf_run(obuf);	
		DPRINTF("dev_attach: ibuf: used = %u, drop = %u\n",
		    obuf->used, obuf->drop);
	}
}
