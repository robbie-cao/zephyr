/*
 * Copyright (c) 2010-2014 Wind River Systems, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file
 * @brief Nanokernel thread support
 *
 * This module provides general purpose thread support, with applies to both
 * tasks or fibers.
 */

#include <kernel.h>

#include <toolchain.h>
#include <sections.h>

#include <nano_private.h>
#include <misc/printk.h>
#include <sys_clock.h>
#include <drivers/system_timer.h>
#include <sched.h>
#include <wait_q.h>

extern struct _static_thread_data _k_task_list_start[];
extern struct _static_thread_data _k_task_list_end[];

#define _FOREACH_STATIC_THREAD(thread_data)                                \
	for (struct _static_thread_data *thread_data = _k_task_list_start; \
	     thread_data < _k_task_list_end; thread_data++)


/* Legacy API */

int sys_execution_context_type_get(void)
{
	if (k_am_in_isr())
		return NANO_CTX_ISR;

	if (_current->prio < 0)
		return NANO_CTX_FIBER;

	return NANO_CTX_TASK;
}

/**
 *
 * @brief Determine if code is running at interrupt level
 *
 * @return 0 if invoked by a thread, or non-zero if invoked by an ISR
 */
int k_am_in_isr(void)
{
	return _IS_IN_ISR();
}

/**
 *
 * @brief Mark thread as essential to system
 *
 * This function tags the running fiber or task as essential to system
 * operation; exceptions raised by this thread will be treated as a fatal
 * system error.
 *
 * @return N/A
 */
void _thread_essential_set(void)
{
	_current->flags |= ESSENTIAL;
}

/**
 *
 * @brief Mark thread as not essential to system
 *
 * This function tags the running fiber or task as not essential to system
 * operation; exceptions raised by this thread may be recoverable.
 * (This is the default tag for a thread.)
 *
 * @return N/A
 */
void _thread_essential_clear(void)
{
	_current->flags &= ~ESSENTIAL;
}

/**
 *
 * @brief Is the specified thread essential?
 *
 * This routine indicates if the running fiber or task is an essential system
 * thread.
 *
 * @return Non-zero if current thread is essential, zero if it is not
 */
int _is_thread_essential(void)
{
	return _current->flags & ESSENTIAL;
}

void k_busy_wait(uint32_t usec_to_wait)
{
	/* use 64-bit math to prevent overflow when multiplying */
	uint32_t cycles_to_wait = (uint32_t)(
		(uint64_t)usec_to_wait *
		(uint64_t)sys_clock_hw_cycles_per_sec /
		(uint64_t)USEC_PER_SEC
	);
	uint32_t start_cycles = k_cycle_get_32();

	for (;;) {
		uint32_t current_cycles = k_cycle_get_32();

		/* this handles the rollover on an unsigned 32-bit value */
		if ((current_cycles - start_cycles) >= cycles_to_wait) {
			break;
		}
	}
}

#ifdef CONFIG_THREAD_CUSTOM_DATA

/**
 *
 * @brief Set thread's custom data
 *
 * This routine sets the custom data value for the current task or fiber.
 * Custom data is not used by the kernel itself, and is freely available
 * for the thread to use as it sees fit.
 *
 * @param value New to set the thread's custom data to.
 *
 * @return N/A
 */
void k_thread_custom_data_set(void *value)
{
	_current->custom_data = value;
}

/**
 *
 * @brief Get thread's custom data
 *
 * This function returns the custom data value for the current task or fiber.
 *
 * @return current handle value
 */
void *k_thread_custom_data_get(void)
{
	return _current->custom_data;
}

#endif /* CONFIG_THREAD_CUSTOM_DATA */

#if defined(CONFIG_THREAD_MONITOR)
/**
 *
 * @brief Thread exit routine
 *
 * This function is invoked when the specified thread is aborted, either
 * normally or abnormally. It is called for the termination of any thread,
 * (fibers and tasks).
 *
 * This routine must be invoked either from a fiber or from a task with
 * interrupts locked to guarantee that the list of threads does not change in
 * mid-operation. It cannot be called from ISR context.
 *
 * @return N/A
 */
void _thread_exit(struct k_thread *thread)
{
	/*
	 * Remove thread from the list of threads.  This singly linked list of
	 * threads maintains ALL the threads in the system: both tasks and
	 * fibers regardless of whether they are runnable.
	 */

	if (thread == _nanokernel.threads) {
		_nanokernel.threads = _nanokernel.threads->next_thread;
	} else {
		struct k_thread *prev_thread;

		prev_thread = _nanokernel.threads;
		while (thread != prev_thread->next_thread) {
			prev_thread = prev_thread->next_thread;
		}
		prev_thread->next_thread = thread->next_thread;
	}
}
#endif /* CONFIG_THREAD_MONITOR */

/**
 *
 * @brief Common thread entry point function
 *
 * This function serves as the entry point for _all_ threads, i.e. both
 * task and fibers are instantiated such that initial execution starts
 * here.
 *
 * This routine invokes the actual task or fiber entry point function and
 * passes it three arguments.  It also handles graceful termination of the
 * task or fiber if the entry point function ever returns.
 *
 * @param pEntry address of the app entry point function
 * @param parameter1 1st arg to the app entry point function
 * @param parameter2 2nd arg to the app entry point function
 * @param parameter3 3rd arg to the app entry point function
 *
 * @internal
 * The 'noreturn' attribute is applied to this function so that the compiler
 * can dispense with generating the usual preamble that is only required for
 * functions that actually return.
 *
 * @return Does not return
 *
 */
FUNC_NORETURN void _thread_entry(void (*entry)(void *, void *, void *),
				 void *p1, void *p2, void *p3)
{
	entry(p1, p2, p3);

	if (_is_thread_essential()) {
		_NanoFatalErrorHandler(_NANO_ERR_INVALID_TASK_EXIT,
				       &_default_esf);
	}

	k_thread_abort(_current);

	/*
	 * Compiler can't tell that fiber_abort() won't return and issues a
	 * warning unless we explicitly tell it that control never gets this
	 * far.
	 */

	CODE_UNREACHABLE;
}

static void start_thread(struct k_thread *thread)
{
	int key = irq_lock(); /* protect kernel queues */

	_mark_thread_as_started(thread);

	if (_is_thread_ready(thread)) {
		_add_thread_to_ready_q(thread);
		if (_must_switch_threads()) {
			_Swap(key);
			return;
		}
	}

	irq_unlock(key);
}

static void schedule_new_thread(struct k_thread *thread, int32_t delay)
{
#ifdef CONFIG_SYS_CLOCK_EXISTS
	if (delay == 0) {
		start_thread(thread);
	} else {
		_mark_thread_as_timing(thread);
		_add_thread_timeout(thread, NULL, _ms_to_ticks(delay));
	}
#else
	ARG_UNUSED(delay);
	start_thread(thread);
#endif
}

k_tid_t k_thread_spawn(char *stack, unsigned stack_size,
			void (*entry)(void *, void *, void*),
			void *p1, void *p2, void *p3,
			int32_t prio, uint32_t options, int32_t delay)
{
	__ASSERT(!_is_in_isr(), "");

	struct k_thread *new_thread = (struct k_thread *)stack;

	_new_thread(stack, stack_size, NULL, entry, p1, p2, p3, prio, options);

	schedule_new_thread(new_thread, delay);

	return new_thread;
}

int k_thread_cancel(k_tid_t tid)
{
	struct k_thread *thread = tid;

	int key = irq_lock();

	if (_has_thread_started(thread) || !_is_thread_timing(thread)) {
		irq_unlock(key);
		return -EINVAL;
	}

	_abort_thread_timeout(thread);
	_thread_exit(thread);

	irq_unlock(key);

	return 0;
}

static inline int is_in_any_group(struct _static_thread_data *thread_data,
				  uint32_t groups)
{
	return !!(thread_data->init_groups & groups);
}

void _k_thread_group_op(uint32_t groups, void (*func)(struct k_thread *))
{
	unsigned int  key;

	__ASSERT(!_is_in_isr(), "");

	k_sched_lock();

	/* Invoke func() on each static thread in the specified group set. */

	_FOREACH_STATIC_THREAD(thread_data) {
		if (is_in_any_group(thread_data, groups)) {
			key = irq_lock();
			func(thread_data->thread);
			irq_unlock(key);
		}
	}

	/*
	 * If the current thread is still in a ready state, then let the
	 * "unlock scheduler" code determine if any rescheduling is needed.
	 */
	if (_is_thread_ready(_current)) {
		k_sched_unlock();
		return;
	}

	/* The current thread is no longer in a ready state--reschedule. */
	key = irq_lock();
	_sched_unlock_no_reschedule();
	_Swap(key);
}

void _k_thread_single_start(struct k_thread *thread)
{
	_mark_thread_as_started(thread);

	if (_is_thread_ready(thread)) {
		_add_thread_to_ready_q(thread);
	}
}

void _k_thread_single_suspend(struct k_thread *thread)
{
	if (_is_thread_ready(thread)) {
		_remove_thread_from_ready_q(thread);
	}

	_mark_thread_as_suspended(thread);
}

void k_thread_suspend(struct k_thread *thread)
{
	unsigned int  key = irq_lock();

	_k_thread_single_suspend(thread);

	if (thread == _current) {
		_Swap(key);
	} else {
		irq_unlock(key);
	}
}

void _k_thread_single_resume(struct k_thread *thread)
{
	_mark_thread_as_not_suspended(thread);

	if (_is_thread_ready(thread)) {
		_add_thread_to_ready_q(thread);
	}
}

void k_thread_resume(struct k_thread *thread)
{
	unsigned int  key = irq_lock();

	_k_thread_single_resume(thread);

	_reschedule_threads(key);
}

void _k_thread_single_abort(struct k_thread *thread)
{
	if (thread->fn_abort != NULL) {
		thread->fn_abort();
	}

	if (_is_thread_ready(thread)) {
		_remove_thread_from_ready_q(thread);
	} else {
		if (_is_thread_pending(thread)) {
			_unpend_thread(thread);
		}
		if (_is_thread_timing(thread)) {
			_abort_thread_timeout(thread);
			_mark_thread_as_not_timing(thread);
		}
	}
	_mark_thread_as_dead(thread);
}

void _init_static_threads(void)
{
	_FOREACH_STATIC_THREAD(thread_data) {
		_new_thread(
			thread_data->init_stack,
			thread_data->init_stack_size,
			NULL,
			thread_data->init_entry,
			thread_data->init_p1,
			thread_data->init_p2,
			thread_data->init_p3,
			thread_data->init_prio,
			0);

		thread_data->thread->init_data = thread_data;
	}
	_k_thread_group_op(K_THREAD_GROUP_EXE, _k_thread_single_start);
}

uint32_t _k_thread_group_mask_get(struct k_thread *thread)
{
	struct _static_thread_data *thread_data = thread->init_data;

	return thread_data->init_groups;
}

void _k_thread_group_join(uint32_t groups, struct k_thread *thread)
{
	struct _static_thread_data *thread_data = thread->init_data;

	thread_data->init_groups |= groups;
}

void _k_thread_group_leave(uint32_t groups, struct k_thread *thread)
{
	struct _static_thread_data *thread_data = thread->init_data;

	thread_data->init_groups &= groups;
}

/* legacy API */

void task_start(ktask_t task)
{
	int key = irq_lock();

	_k_thread_single_start(task);
	_reschedule_threads(key);
}
