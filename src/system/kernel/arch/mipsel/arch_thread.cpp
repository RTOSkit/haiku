/*
 * Copyright 2003-2010 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 * 		Ingo Weinhold <bonefish@cs.tu-berlin.de>
 * 		François Revol <revol@free.fr>
 *		Jonas Sundström <jonas@kirilla.com
 *
 * Copyright 2001, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */


#include <arch_thread.h>

#include <arch_cpu.h>
#include <arch/thread.h>
#include <boot/stage2.h>
#include <kernel.h>
#include <thread.h>
#include <vm/vm_types.h>
#include <vm/VMAddressSpace.h>
#include <arch_vm.h>

#include <string.h>


static struct arch_thread sInitialState;


void
mipsel_push_iframe(struct iframe_stack *stack, struct iframe *frame)
{
#warning IMPLEMENT mipsel_push_iframe
}


void
mipsel_pop_iframe(struct iframe_stack *stack)
{
#warning IMPLEMENT mipsel_pop_iframe
}


static struct iframe *
mipsel_get_current_iframe(void)
{
#warning IMPLEMENT mipsel_get_current_iframe
	return NULL;
}


struct iframe *
mipsel_get_user_iframe(void)
{
#warning IMPLEMENT mipsel_get_user_iframe
	return NULL;
}


// #pragma mark -


status_t
arch_thread_init(struct kernel_args *args)
{
#warning IMPLEMENT arch_thread_init
	return B_ERROR;
}


status_t
arch_team_init_team_struct(struct team *team, bool kernel)
{
#warning IMPLEMENT arch_team_init_team_struct
	return B_ERROR;
}


status_t
arch_thread_init_thread_struct(struct thread *thread)
{
#warning IMPLEMENT arch_thread_init_thread_struct
	return B_ERROR;
}


status_t
arch_thread_init_kthread_stack(struct thread *t, int (*start_func)(void),
	void (*entry_func)(void), void (*exit_func)(void))
{
#warning IMPLEMENT arch_thread_init_kthread_stack
	return B_ERROR;
}


status_t
arch_thread_init_tls(struct thread *thread)
{
#warning IMPLEMENT arch_thread_init_tls
	return B_ERROR;
}


void
arch_thread_context_switch(struct thread *from, struct thread *to)
{
#warning IMPLEMENT arch_thread_context_switch
}


void
arch_thread_dump_info(void *info)
{
#warning IMPLEMENT arch_thread_dump_info
}


status_t
arch_thread_enter_userspace(struct thread *thread, addr_t entry, void *arg1,
	void *arg2)
{
#warning IMPLEMENT arch_thread_enter_userspace
	panic("arch_thread_enter_uspace(): not yet implemented\n");
	return B_ERROR;
}


bool
arch_on_signal_stack(struct thread *thread)
{
#warning IMPLEMENT arch_on_signal_stack
	return false;
}


status_t
arch_setup_signal_frame(struct thread *thread, struct sigaction *sa, int sig,
	int sigMask)
{
#warning IMPLEMENT arch_setup_signal_frame
	return B_ERROR;
}


int64
arch_restore_signal_frame(void)
{
#warning IMPLEMENT arch_restore_signal_frame
	return 0;
}


void
arch_check_syscall_restart(struct thread *thread)
{
#warning IMPLEMENT arch_check_syscall_restart
}


void
arch_store_fork_frame(struct arch_fork_arg *arg)
{
#warning IMPLEMENT arch_store_fork_frame
}


void
arch_restore_fork_frame(struct arch_fork_arg *arg)
{
#warning IMPLEMENT arch_restore_fork_frame
}

