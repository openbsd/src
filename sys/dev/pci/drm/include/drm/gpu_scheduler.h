/*
 * Copyright 2015 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _DRM_GPU_SCHEDULER_H_
#define _DRM_GPU_SCHEDULER_H_

#include <drm/spsc_queue.h>
#include <linux/dma-fence.h>

#define MAX_WAIT_SCHED_ENTITY_Q_EMPTY msecs_to_jiffies(1000)

struct drm_gpu_scheduler;
struct drm_sched_rq;

enum drm_sched_priority {
	DRM_SCHED_PRIORITY_MIN,
	DRM_SCHED_PRIORITY_LOW = DRM_SCHED_PRIORITY_MIN,
	DRM_SCHED_PRIORITY_NORMAL,
	DRM_SCHED_PRIORITY_HIGH_SW,
	DRM_SCHED_PRIORITY_HIGH_HW,
	DRM_SCHED_PRIORITY_KERNEL,
	DRM_SCHED_PRIORITY_MAX,
	DRM_SCHED_PRIORITY_INVALID = -1,
	DRM_SCHED_PRIORITY_UNSET = -2
};

/**
 * struct drm_sched_entity - A wrapper around a job queue (typically
 * attached to the DRM file_priv).
 *
 * @list: used to append this struct to the list of entities in the
 *        runqueue.
 * @rq: runqueue to which this entity belongs.
 * @rq_lock: lock to modify the runqueue to which this entity belongs.
 * @job_queue: the list of jobs of this entity.
 * @fence_seq: a linearly increasing seqno incremented with each
 *             new &drm_sched_fence which is part of the entity.
 * @fence_context: a unique context for all the fences which belong
 *                 to this entity.
 *                 The &drm_sched_fence.scheduled uses the
 *                 fence_context but &drm_sched_fence.finished uses
 *                 fence_context + 1.
 * @dependency: the dependency fence of the job which is on the top
 *              of the job queue.
 * @cb: callback for the dependency fence above.
 * @guilty: points to ctx's guilty.
 * @fini_status: contains the exit status in case the process was signalled.
 * @last_scheduled: points to the finished fence of the last scheduled job.
 * @last_user: last group leader pushing a job into the entity.
 *
 * Entities will emit jobs in order to their corresponding hardware
 * ring, and the scheduler will alternate between entities based on
 * scheduling policy.
 */
struct drm_sched_entity {
	struct list_head		list;
	struct drm_sched_rq		*rq;
	spinlock_t			rq_lock;

	struct spsc_queue		job_queue;

	atomic_t			fence_seq;
	uint64_t			fence_context;

	struct dma_fence		*dependency;
	struct dma_fence_cb		cb;
	atomic_t			*guilty;
	struct dma_fence                *last_scheduled;
#ifdef __linux__
	struct task_struct		*last_user;
#else
	struct proc			*last_user;
#endif
};

/**
 * struct drm_sched_rq - queue of entities to be scheduled.
 *
 * @lock: to modify the entities list.
 * @sched: the scheduler to which this rq belongs to.
 * @entities: list of the entities to be scheduled.
 * @current_entity: the entity which is to be scheduled.
 *
 * Run queue is a set of entities scheduling command submissions for
 * one specific ring. It implements the scheduling policy that selects
 * the next entity to emit commands from.
 */
struct drm_sched_rq {
	spinlock_t			lock;
	struct drm_gpu_scheduler	*sched;
	struct list_head		entities;
	struct drm_sched_entity		*current_entity;
};

/**
 * struct drm_sched_fence - fences corresponding to the scheduling of a job.
 */
struct drm_sched_fence {
        /**
         * @scheduled: this fence is what will be signaled by the scheduler
         * when the job is scheduled.
         */
	struct dma_fence		scheduled;

        /**
         * @finished: this fence is what will be signaled by the scheduler
         * when the job is completed.
         *
         * When setting up an out fence for the job, you should use
         * this, since it's available immediately upon
         * drm_sched_job_init(), and the fence returned by the driver
         * from run_job() won't be created until the dependencies have
         * resolved.
         */
	struct dma_fence		finished;

        /**
         * @cb: the callback for the parent fence below.
         */
	struct dma_fence_cb		cb;
        /**
         * @parent: the fence returned by &drm_sched_backend_ops.run_job
         * when scheduling the job on hardware. We signal the
         * &drm_sched_fence.finished fence once parent is signalled.
         */
	struct dma_fence		*parent;
        /**
         * @sched: the scheduler instance to which the job having this struct
         * belongs to.
         */
	struct drm_gpu_scheduler	*sched;
        /**
         * @lock: the lock used by the scheduled and the finished fences.
         */
	spinlock_t			lock;
        /**
         * @owner: job owner for debugging
         */
	void				*owner;
};

struct drm_sched_fence *to_drm_sched_fence(struct dma_fence *f);

/**
 * struct drm_sched_job - A job to be run by an entity.
 *
 * @queue_node: used to append this struct to the queue of jobs in an entity.
 * @sched: the scheduler instance on which this job is scheduled.
 * @s_fence: contains the fences for the scheduling of job.
 * @finish_cb: the callback for the finished fence.
 * @finish_work: schedules the function @drm_sched_job_finish once the job has
 *               finished to remove the job from the
 *               @drm_gpu_scheduler.ring_mirror_list.
 * @node: used to append this struct to the @drm_gpu_scheduler.ring_mirror_list.
 * @work_tdr: schedules a delayed call to @drm_sched_job_timedout after the timeout
 *            interval is over.
 * @id: a unique id assigned to each job scheduled on the scheduler.
 * @karma: increment on every hang caused by this job. If this exceeds the hang
 *         limit of the scheduler then the job is marked guilty and will not
 *         be scheduled further.
 * @s_priority: the priority of the job.
 * @entity: the entity to which this job belongs.
 *
 * A job is created by the driver using drm_sched_job_init(), and
 * should call drm_sched_entity_push_job() once it wants the scheduler
 * to schedule the job.
 */
struct drm_sched_job {
	struct spsc_node		queue_node;
	struct drm_gpu_scheduler	*sched;
	struct drm_sched_fence		*s_fence;
	struct dma_fence_cb		finish_cb;
	struct work_struct		finish_work;
	struct list_head		node;
	struct delayed_work		work_tdr;
	uint64_t			id;
	atomic_t			karma;
	enum drm_sched_priority		s_priority;
	struct drm_sched_entity  *entity;
};

static inline bool drm_sched_invalidate_job(struct drm_sched_job *s_job,
					    int threshold)
{
	return (s_job && atomic_inc_return(&s_job->karma) > threshold);
}

/**
 * struct drm_sched_backend_ops
 *
 * Define the backend operations called by the scheduler,
 * these functions should be implemented in driver side.
 */
struct drm_sched_backend_ops {
	/**
         * @dependency: Called when the scheduler is considering scheduling
         * this job next, to get another struct dma_fence for this job to
	 * block on.  Once it returns NULL, run_job() may be called.
	 */
	struct dma_fence *(*dependency)(struct drm_sched_job *sched_job,
					struct drm_sched_entity *s_entity);

	/**
         * @run_job: Called to execute the job once all of the dependencies
         * have been resolved.  This may be called multiple times, if
	 * timedout_job() has happened and drm_sched_job_recovery()
	 * decides to try it again.
	 */
	struct dma_fence *(*run_job)(struct drm_sched_job *sched_job);

	/**
         * @timedout_job: Called when a job has taken too long to execute,
         * to trigger GPU recovery.
	 */
	void (*timedout_job)(struct drm_sched_job *sched_job);

	/**
         * @free_job: Called once the job's finished fence has been signaled
         * and it's time to clean it up.
	 */
	void (*free_job)(struct drm_sched_job *sched_job);
};

/**
 * struct drm_gpu_scheduler
 *
 * @ops: backend operations provided by the driver.
 * @hw_submission_limit: the max size of the hardware queue.
 * @timeout: the time after which a job is removed from the scheduler.
 * @name: name of the ring for which this scheduler is being used.
 * @sched_rq: priority wise array of run queues.
 * @wake_up_worker: the wait queue on which the scheduler sleeps until a job
 *                  is ready to be scheduled.
 * @job_scheduled: once @drm_sched_entity_do_release is called the scheduler
 *                 waits on this wait queue until all the scheduled jobs are
 *                 finished.
 * @hw_rq_count: the number of jobs currently in the hardware queue.
 * @job_id_count: used to assign unique id to the each job.
 * @thread: the kthread on which the scheduler which run.
 * @ring_mirror_list: the list of jobs which are currently in the job queue.
 * @job_list_lock: lock to protect the ring_mirror_list.
 * @hang_limit: once the hangs by a job crosses this limit then it is marked
 *              guilty and it will be considered for scheduling further.
 *
 * One scheduler is implemented for each hardware ring.
 */
struct drm_gpu_scheduler {
	const struct drm_sched_backend_ops	*ops;
	uint32_t			hw_submission_limit;
	long				timeout;
	const char			*name;
	struct drm_sched_rq		sched_rq[DRM_SCHED_PRIORITY_MAX];
	wait_queue_head_t		wake_up_worker;
	wait_queue_head_t		job_scheduled;
	atomic_t			hw_rq_count;
	atomic64_t			job_id_count;
#ifdef __linux__
	struct task_struct		*thread;
#else
	struct proc			*thread;
#endif
	struct list_head		ring_mirror_list;
	spinlock_t			job_list_lock;
	int				hang_limit;
};

int drm_sched_init(struct drm_gpu_scheduler *sched,
		   const struct drm_sched_backend_ops *ops,
		   uint32_t hw_submission, unsigned hang_limit, long timeout,
		   const char *name);
void drm_sched_fini(struct drm_gpu_scheduler *sched);

int drm_sched_entity_init(struct drm_sched_entity *entity,
			  struct drm_sched_rq **rq_list,
			  unsigned int num_rq_list,
			  atomic_t *guilty);
long drm_sched_entity_flush(struct drm_sched_entity *entity, long timeout);
void drm_sched_entity_fini(struct drm_sched_entity *entity);
void drm_sched_entity_destroy(struct drm_sched_entity *entity);
void drm_sched_entity_push_job(struct drm_sched_job *sched_job,
			       struct drm_sched_entity *entity);
void drm_sched_entity_set_rq(struct drm_sched_entity *entity,
			     struct drm_sched_rq *rq);

struct drm_sched_fence *drm_sched_fence_create(
	struct drm_sched_entity *s_entity, void *owner);
void drm_sched_fence_scheduled(struct drm_sched_fence *fence);
void drm_sched_fence_finished(struct drm_sched_fence *fence);
int drm_sched_job_init(struct drm_sched_job *job,
		       struct drm_sched_entity *entity,
		       void *owner);
void drm_sched_hw_job_reset(struct drm_gpu_scheduler *sched,
			    struct drm_sched_job *job);
void drm_sched_job_recovery(struct drm_gpu_scheduler *sched);
bool drm_sched_dependency_optimized(struct dma_fence* fence,
				    struct drm_sched_entity *entity);
void drm_sched_job_kickout(struct drm_sched_job *s_job);

#endif
