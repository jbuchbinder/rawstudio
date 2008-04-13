/*
 * Copyright (C) 2006-2008 Anders Brander <anders@brander.dk> and 
 * Anders Kvist <akv@lnxbx.dk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <glib.h>
#include "rs-job.h"
#include "rawstudio.h"
#include "rs-image.h"

typedef enum {
	JOB_DEMOSAIC,
	JOB_SHARPEN,
} JOB_TYPE;

struct _RS_JOB {
	JOB_TYPE type;
	gboolean abort;
	gpointer arg1;
	gpointer arg2;
	gdouble dou1;
};

static GMutex *job_lock;
static GQueue *job_queue = NULL; /* Protected by job_lock */
static RS_JOB *current_job = NULL; /* Protected by job_lock */
static GCond *job_cond; /* Protected by job_lock */

static void job_end(RS_JOB *job);
static gpointer job_consumer(gpointer unused);
static void rs_job_init();
static void rs_job_add(RS_JOB *job);

/**
 * Clean up after a job has been run or cancelled
 * @param job A RS_JOB
 */
static void
job_end(RS_JOB *job)
{
	switch (job->type)
	{
		case JOB_DEMOSAIC:
			g_object_unref(G_OBJECT(job->arg1));
			break;
		case JOB_SHARPEN:
			g_object_unref(G_OBJECT(job->arg1));
			g_object_unref(G_OBJECT(job->arg2));
			break;
	}
	g_free(job);
}

/**
 * A function to consume jobs, this should run in it's own thread
 * @note Will never return
 */
static gpointer
job_consumer(gpointer unused)
{
	RS_JOB *job;

	while (1) /* Loop forever */
	{
		g_mutex_lock(job_lock);
		while (!(job = g_queue_pop_head(job_queue)))
			g_cond_wait(job_cond, job_lock);
		current_job = job;
		g_mutex_unlock(job_lock);

		switch (job->type)
		{
			case JOB_DEMOSAIC:
				rs_image16_demosaic(RS_IMAGE16(job->arg1), RS_DEMOSAIC_PPG);
				break;
			case JOB_SHARPEN:
				rs_image16_sharpen(RS_IMAGE16(job->arg1), RS_IMAGE16(job->arg2), job->dou1, &job->abort);
				break;
		}
		job_end(job);

		g_mutex_lock(job_lock);
		current_job = NULL;
		g_mutex_unlock(job_lock);
	}

	return NULL; /* Shut up gcc! */
}

/**
 * Initialize static data
 */
static void
rs_job_init()
{
	static GStaticMutex init_lock = G_STATIC_MUTEX_INIT;
	g_static_mutex_lock(&init_lock);
	if (!job_queue)
	{
		job_queue = g_queue_new();
		job_lock = g_mutex_new();
		job_cond = g_cond_new();
		g_thread_create_full(job_consumer, NULL, 0, FALSE, FALSE, G_THREAD_PRIORITY_LOW, NULL);
	}
	g_static_mutex_unlock(&init_lock);
}

/**
 * Add a new job to the queue
 * @param job A RS_JOB
 */
static void
rs_job_add(RS_JOB *job)
{
	rs_job_init();

	g_mutex_lock(job_lock);
	g_queue_push_tail(job_queue, job);
	g_cond_signal(job_cond);
	g_mutex_unlock(job_lock);
}

/**
 * Cancels a job. If the job is in queue it will be removed and job_end() will be called. If the job is currently
 * running, the consumer thread will be signalled to abort
 * @param job A RS_JOB
 */
void
rs_job_cancel(RS_JOB *job)
{
	gint n;
	rs_job_init();

	if (!job)
		return;

	g_mutex_lock(job_lock);
	if (current_job == job)
		current_job->abort = TRUE;
	else
	{
		n = g_queue_index(job_queue, job);
		if (n>-1)
		{
			g_queue_pop_nth(job_queue, n);
			job_end(job);
		}
	}
	g_mutex_unlock(job_lock);
}

/**
 * Adds a new demosaic job
 * @param image An RS_IMAGE16 to demosaic
 * @return A new RS_JOB, this should NOT be freed by caller
 */
RS_JOB *
rs_job_add_demosaic(RS_IMAGE16 *image)
{
	RS_JOB *job = g_new0(RS_JOB, 1);

	g_assert(RS_IS_IMAGE16(image));

	g_object_ref(image);

	job->type = JOB_DEMOSAIC;
	job->arg1 = image;

	rs_job_add(job);

	return job;
}

/**
 * Adds a new sharpen job
 * @param in An RS_IMAGE16 to sharpen
 * @param out An RS_IMAGE16 for output after sharpen
 * @return A new RS_JOB, this should NOT be freed by caller
 */
RS_JOB *
rs_job_add_sharpen(RS_IMAGE16 *in, RS_IMAGE16 *out, gdouble amount)
{
	RS_JOB *job = g_new0(RS_JOB, 1);

	g_assert(RS_IS_IMAGE16(in));
	g_assert(RS_IS_IMAGE16(out));

	g_object_ref(in);
	g_object_ref(out);

	job->type = JOB_SHARPEN;
	job->arg1 = in;
	job->arg2 = out;
	job->dou1 = amount;

	rs_job_add(job);

	return job;
}
