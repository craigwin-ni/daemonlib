/*
 * daemonlib
 * Copyright (C) 2012-2014, 2018, 2021 Matthias Bolte <matthias@tinkerforge.com>
 *
 * threads_posix.c: PThread based thread and locking implementation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>

#include "threads_posix.h"

void mutex_create(Mutex *mutex) {
	if (pthread_mutex_init(&mutex->handle, NULL) != 0) {
		abort();
	}
}

void mutex_destroy(Mutex *mutex) {
	if (pthread_mutex_destroy(&mutex->handle) != 0) {
		abort();
	}
}

void mutex_lock(Mutex *mutex) {
	if (pthread_mutex_lock(&mutex->handle) != 0) {
		abort();
	}
}

void mutex_unlock(Mutex *mutex) {
	if (pthread_mutex_unlock(&mutex->handle) != 0) {
		abort();
	}
}

void condition_create(Condition *condition) {
	if (pthread_cond_init(&condition->handle, NULL) != 0) {
		abort();
	}
}

void condition_destroy(Condition *condition) {
	if (pthread_cond_destroy(&condition->handle) != 0) {
		abort();
	}
}

void condition_wait(Condition *condition, Mutex *mutex) {
	if (pthread_cond_wait(&condition->handle, &mutex->handle) != 0) {
		abort();
	}
}

void condition_broadcast(Condition *condition) {
	if (pthread_cond_broadcast(&condition->handle) != 0) {
		abort();
	}
}

void semaphore_create(Semaphore *semaphore) {
#ifdef __APPLE__
	// macOS does not support unnamed semaphores, so we fake them. unlink
	// first to ensure that there is no existing semaphore with that name.
	// then open the semaphore to create a new one. finally unlink it again to
	// avoid leaking the name. the semaphore will work fine without a name
	char name[100];

	snprintf(name, sizeof(name), "tf-daemonlib-%p", semaphore);

	sem_unlink(name);
	semaphore->pointer = sem_open(name, O_CREAT | O_EXCL, S_IRWXU, 0);
	sem_unlink(name);

	if (semaphore->pointer == SEM_FAILED) {
		abort();
	}
#else
	semaphore->pointer = &semaphore->object;

	if (sem_init(semaphore->pointer, 0, 0) < 0) {
		abort();
	}
#endif
}

void semaphore_destroy(Semaphore *semaphore) {
#ifdef __APPLE__
	if (sem_close(semaphore->pointer) < 0) {
		abort();
	}
#else
	if (sem_destroy(semaphore->pointer) < 0) {
		abort();
	}
#endif
}

void semaphore_acquire(Semaphore *semaphore) {
	if (sem_wait(semaphore->pointer) < 0) {
		abort();
	}
}

void semaphore_release(Semaphore *semaphore) {
	if (sem_post(semaphore->pointer) < 0) {
		abort();
	}
}

static void *thread_wrapper(void *opaque) {
	Thread *thread = opaque;

	thread->function(thread->opaque);

	return NULL;
}

void thread_create(Thread *thread, ThreadFunction function, void *opaque) {
	thread->function = function;
	thread->opaque = opaque;

	if (pthread_create(&thread->handle, NULL, thread_wrapper, thread) != 0) {
		abort();
	}
}

void thread_destroy(Thread *thread) {
	(void)thread;
}

void thread_join(Thread *thread) {
	if (pthread_equal(thread->handle, pthread_self())) {
		abort();
	}

	if (pthread_join(thread->handle, NULL) != 0) {
		abort();
	}
}
