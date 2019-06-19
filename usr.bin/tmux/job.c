/* $OpenBSD: job.c,v 1.54 2018/11/19 13:35:41 nicm Exp $ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <sys/socket.h>

#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Job scheduling. Run queued commands in the background and record their
 * output.
 */

static void	job_read_callback(struct bufferevent *, void *);
static void	job_write_callback(struct bufferevent *, void *);
static void	job_error_callback(struct bufferevent *, short, void *);

/* A single job. */
struct job {
	enum {
		JOB_RUNNING,
		JOB_DEAD,
		JOB_CLOSED
	} state;

	int			 flags;

	char			*cmd;
	pid_t			 pid;
	int			 status;

	int			 fd;
	struct bufferevent	*event;

	job_update_cb		 updatecb;
	job_complete_cb		 completecb;
	job_free_cb		 freecb;
	void			*data;

	LIST_ENTRY(job)		 entry;
};

/* All jobs list. */
static LIST_HEAD(joblist, job) all_jobs = LIST_HEAD_INITIALIZER(all_jobs);

/* Start a job running, if it isn't already. */
struct job *
job_run(const char *cmd, struct session *s, const char *cwd,
    job_update_cb updatecb, job_complete_cb completecb, job_free_cb freecb,
    void *data, int flags)
{
	struct job	*job;
	struct environ	*env;
	pid_t		 pid;
	int		 nullfd, out[2];
	const char	*home;
	sigset_t	 set, oldset;

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, out) != 0)
		return (NULL);
	log_debug("%s: cmd=%s, cwd=%s", __func__, cmd, cwd == NULL ? "" : cwd);

	/*
	 * Do not set TERM during .tmux.conf, it is nice to be able to use
	 * if-shell to decide on default-terminal based on outside TERM.
	 */
	env = environ_for_session(s, !cfg_finished);

	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, &oldset);
	switch (pid = fork()) {
	case -1:
		sigprocmask(SIG_SETMASK, &oldset, NULL);
		environ_free(env);
		close(out[0]);
		close(out[1]);
		return (NULL);
	case 0:
		proc_clear_signals(server_proc, 1);
		sigprocmask(SIG_SETMASK, &oldset, NULL);

		if (cwd == NULL || chdir(cwd) != 0) {
			if ((home = find_home()) == NULL || chdir(home) != 0)
				chdir("/");
		}

		environ_push(env);
		environ_free(env);

		if (dup2(out[1], STDIN_FILENO) == -1)
			fatal("dup2 failed");
		if (dup2(out[1], STDOUT_FILENO) == -1)
			fatal("dup2 failed");
		if (out[1] != STDIN_FILENO && out[1] != STDOUT_FILENO)
			close(out[1]);
		close(out[0]);

		nullfd = open(_PATH_DEVNULL, O_RDWR, 0);
		if (nullfd < 0)
			fatal("open failed");
		if (dup2(nullfd, STDERR_FILENO) == -1)
			fatal("dup2 failed");
		if (nullfd != STDERR_FILENO)
			close(nullfd);

		closefrom(STDERR_FILENO + 1);

		execl(_PATH_BSHELL, "sh", "-c", cmd, (char *) NULL);
		fatal("execl failed");
	}

	sigprocmask(SIG_SETMASK, &oldset, NULL);
	environ_free(env);
	close(out[1]);

	job = xmalloc(sizeof *job);
	job->state = JOB_RUNNING;
	job->flags = flags;

	job->cmd = xstrdup(cmd);
	job->pid = pid;
	job->status = 0;

	LIST_INSERT_HEAD(&all_jobs, job, entry);

	job->updatecb = updatecb;
	job->completecb = completecb;
	job->freecb = freecb;
	job->data = data;

	job->fd = out[0];
	setblocking(job->fd, 0);

	job->event = bufferevent_new(job->fd, job_read_callback,
	    job_write_callback, job_error_callback, job);
	if (job->event == NULL)
		fatalx("out of memory");
	bufferevent_enable(job->event, EV_READ|EV_WRITE);

	log_debug("run job %p: %s, pid %ld", job, job->cmd, (long) job->pid);
	return (job);
}

/* Kill and free an individual job. */
void
job_free(struct job *job)
{
	log_debug("free job %p: %s", job, job->cmd);

	LIST_REMOVE(job, entry);
	free(job->cmd);

	if (job->freecb != NULL && job->data != NULL)
		job->freecb(job->data);

	if (job->pid != -1)
		kill(job->pid, SIGTERM);
	if (job->event != NULL)
		bufferevent_free(job->event);
	if (job->fd != -1)
		close(job->fd);

	free(job);
}

/* Job buffer read callback. */
static void
job_read_callback(__unused struct bufferevent *bufev, void *data)
{
	struct job	*job = data;

	if (job->updatecb != NULL)
		job->updatecb(job);
}

/*
 * Job buffer write callback. Fired when the buffer falls below watermark
 * (default is empty). If all the data has been written, disable the write
 * event.
 */
static void
job_write_callback(__unused struct bufferevent *bufev, void *data)
{
	struct job	*job = data;
	size_t		 len = EVBUFFER_LENGTH(EVBUFFER_OUTPUT(job->event));

	log_debug("job write %p: %s, pid %ld, output left %zu", job, job->cmd,
	    (long) job->pid, len);

	if (len == 0) {
		shutdown(job->fd, SHUT_WR);
		bufferevent_disable(job->event, EV_WRITE);
	}
}

/* Job buffer error callback. */
static void
job_error_callback(__unused struct bufferevent *bufev, __unused short events,
    void *data)
{
	struct job	*job = data;

	log_debug("job error %p: %s, pid %ld", job, job->cmd, (long) job->pid);

	if (job->state == JOB_DEAD) {
		if (job->completecb != NULL)
			job->completecb(job);
		job_free(job);
	} else {
		bufferevent_disable(job->event, EV_READ);
		job->state = JOB_CLOSED;
	}
}

/* Job died (waitpid() returned its pid). */
void
job_check_died(pid_t pid, int status)
{
	struct job	*job;

	LIST_FOREACH(job, &all_jobs, entry) {
		if (pid == job->pid)
			break;
	}
	if (job == NULL)
		return;
	log_debug("job died %p: %s, pid %ld", job, job->cmd, (long) job->pid);

	job->status = status;

	if (job->state == JOB_CLOSED) {
		if (job->completecb != NULL)
			job->completecb(job);
		job_free(job);
	} else {
		job->pid = -1;
		job->state = JOB_DEAD;
	}
}

/* Get job status. */
int
job_get_status(struct job *job)
{
	return (job->status);
}

/* Get job data. */
void *
job_get_data(struct job *job)
{
	return (job->data);
}

/* Get job event. */
struct bufferevent *
job_get_event(struct job *job)
{
	return (job->event);
}

/* Kill all jobs. */
void
job_kill_all(void)
{
	struct job	*job;

	LIST_FOREACH(job, &all_jobs, entry) {
		if (job->pid != -1)
			kill(job->pid, SIGTERM);
	}
}

/* Are any jobs still running? */
int
job_still_running(void)
{
	struct job	*job;

	LIST_FOREACH(job, &all_jobs, entry) {
		if ((~job->flags & JOB_NOWAIT) && job->state == JOB_RUNNING)
			return (1);
	}
	return (0);
}

/* Print job summary. */
void
job_print_summary(struct cmdq_item *item, int blank)
{
	struct job	*job;
	u_int		 n = 0;

	LIST_FOREACH(job, &all_jobs, entry) {
		if (blank) {
			cmdq_print(item, "%s", "");
			blank = 0;
		}
		cmdq_print(item, "Job %u: %s [fd=%d, pid=%ld, status=%d]",
		    n, job->cmd, job->fd, (long)job->pid, job->status);
		n++;
	}
}
