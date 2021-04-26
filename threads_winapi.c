/*
 * daemonlib
 * Copyright (C) 2012-2014, 2021 Matthias Bolte <matthias@tinkerforge.com>
 *
 * threads_winapi.c: WinAPI based thread and locking implementation
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
#include <stdint.h>

#include "threads_winapi.h"

void mutex_create(Mutex *mutex) {
	InitializeCriticalSection(&mutex->handle);
}

void mutex_destroy(Mutex *mutex) {
	DeleteCriticalSection(&mutex->handle);
}

void mutex_lock(Mutex *mutex) {
	EnterCriticalSection(&mutex->handle);
}

void mutex_unlock(Mutex *mutex) {
	LeaveCriticalSection(&mutex->handle);
}

void condition_create(Condition *condition) {
	InitializeConditionVariable(&condition->handle);
}

void condition_destroy(Condition *condition) {
	(void)condition;
}

void condition_wait(Condition *condition, Mutex *mutex) {
	if (!SleepConditionVariableCS(&condition->handle, &mutex->handle, INFINITE)) {
		abort();
	}
}

void condition_broadcast(Condition *condition) {
	WakeAllConditionVariable(&condition->handle);
}

void semaphore_create(Semaphore *semaphore) {
	semaphore->handle = CreateSemaphore(NULL, 0, INT32_MAX, NULL);

	if (semaphore->handle == NULL) {
		abort();
	}
}

void semaphore_destroy(Semaphore *semaphore) {
	if (!CloseHandle(semaphore->handle)) {
		abort();
	}
}

void semaphore_acquire(Semaphore *semaphore) {
	if (WaitForSingleObject(semaphore->handle, INFINITE) != WAIT_OBJECT_0) {
		abort();
	}
}

void semaphore_release(Semaphore *semaphore) {
	if (!ReleaseSemaphore(semaphore->handle, 1, NULL)) {
		abort();
	}
}

static DWORD WINAPI thread_wrapper(void *opaque) {
	Thread *thread = opaque;

	thread->function(thread->opaque);

	return 0;
}

void thread_create(Thread *thread, ThreadFunction function, void *opaque) {
	thread->function = function;
	thread->opaque = opaque;

	thread->handle = CreateThread(NULL, 0, thread_wrapper, thread, 0, &thread->id);

	if (thread->handle == NULL) {
		abort();
	}
}

void thread_destroy(Thread *thread) {
	if (!CloseHandle(thread->handle)) {
		abort();
	}
}

void thread_join(Thread *thread) {
	if (thread->id == GetCurrentThreadId()) {
		abort();
	}

	if (WaitForSingleObject(thread->handle, INFINITE) != WAIT_OBJECT_0) {
		abort();
	}
}
