#include <sys/types.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "conf.h"
#include "wav.h"

/*
 * max data of a .wav file. The total file size must be smaller than
 * 2^31, and we also have to leave some space for the headers (around 40
 * bytes)
 */
#define WAV_DATAMAX	(0x7fff0000)

struct fileops wav_ops = {
	"wav",
	sizeof(struct wav),
	wav_close,
	wav_read,
	wav_write,
	NULL, /* start */
	NULL, /* stop */
	pipe_nfds,
	pipe_pollfd,
	pipe_revents
};

struct wav *
wav_new_in(struct fileops *ops, int fd, char *name,
    struct aparams *par, unsigned hdr)
{
	struct wav *f;

	f = (struct wav *)pipe_new(ops, fd, name);
	if (f == NULL)
		return NULL;
	if (hdr == HDR_WAV) {
		if (!wav_readhdr(f->pipe.fd, par, &f->rbytes, NULL))
			exit(1);
		f->hpar = *par;
	} else
		f->rbytes = -1;
	f->hdr = 0;
	return f;
}

struct wav *
wav_new_out(struct fileops *ops, int fd, char *name,
    struct aparams *par, unsigned hdr)
{
	struct wav *f;

	f = (struct wav *)pipe_new(ops, fd, name);
	if (f == NULL)
		return NULL;
	if (hdr == HDR_WAV) {
		par->le = 1;
		par->sig = (par->bits <= 8) ? 0 : 1;
		par->bps = (par->bits + 7) / 8;
		if (!wav_writehdr(f->pipe.fd, par))
			exit(1);
		f->hpar = *par;
		f->wbytes = WAV_DATAMAX;
	} else
		f->wbytes = -1;
	f->hdr = hdr;
	return f;
}

unsigned
wav_read(struct file *file, unsigned char *data, unsigned count)
{
	struct wav *f = (struct wav *)file;
	unsigned n;

	if (f->rbytes >= 0 && count > f->rbytes) {
		count = f->rbytes; /* file->rbytes fits in count */
		if (count == 0) {
			DPRINTFN(2, "wav_read: %s: complete\n", f->pipe.file.name);
			file_eof(&f->pipe.file);
			return 0;
		}
	}
	n = pipe_read(file, data, count);
	if (f->rbytes >= 0)
		f->rbytes -= n;
	return n;
}


unsigned
wav_write(struct file *file, unsigned char *data, unsigned count)
{
	struct wav *f = (struct wav *)file;
	unsigned n;

	if (f->wbytes >= 0 && count > f->wbytes) {
		count = f->wbytes; /* wbytes fits in count */
		if (count == 0) {
			DPRINTFN(2, "wav_write: %s: complete\n",
			    f->pipe.file.name);
			file_hup(&f->pipe.file);
			return 0;
		}
	}
	n = pipe_write(file, data, count);
	if (f->wbytes >= 0)
		f->wbytes -= n;
	return n;
}

void
wav_close(struct file *file)
{
	struct wav *f = (struct wav *)file;

	if (f->hdr == HDR_WAV)
		wav_writehdr(f->pipe.fd, &f->hpar);
	pipe_close(file);
}
