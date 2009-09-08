/* POSIX.1 sigaction call for Linux/SPARC.
   Copyright (C) 1997-2000,2002,2003,2005 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Miguel de Icaza (miguel@nuclecu.unam.mx), 1997.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.

       Ported to uClibc from glibc: 090520:
               Jan Buchholz, KIP, Uni Heidelberg <jan.buchholz@kip.uni-heidelberg.de>
               Austin Foxley, Ceton Corporation <austinf@cetoncorp.com>
*/

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>
#include <bits/kernel_sigaction.h>


_syscall5(int, rt_sigaction, int, a, int, b, int, c, int, d, int, e);
static void __rt_sigreturn_stub(void);
static void __sigreturn_stub(void);

int __libc_sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
	int ret;
	struct old_kernel_sigaction kact, koact;
	unsigned long stub = 0;
	int saved_errno = errno;

	if (act) {
		kact.k_sa_handler = act->sa_handler;
		// BUG?! kact.sa_mask is a long, but sigset_t is a vector
		// of longs and it may be bigger (in glibc, it _is_ bigger).
		// Should we do this instead?
		// kact.sa_mask = act->sa_mask.__val[0];
		memcpy(&kact.sa_mask, &act->sa_mask, sizeof(sigset_t));
		kact.sa_flags = act->sa_flags;
		if (kact.sa_flags & SA_SIGINFO)
			stub = (unsigned long) &__rt_sigreturn_stub;
		else
			stub = (unsigned long) &__sigreturn_stub;
		stub -= 8;
		kact.sa_restorer = NULL;
	}
	/* NB: kernel (as of 2.6.25) will return EINVAL
	 * if _NSIG / 8 does not match kernel's sizeof(sigset_t) */
	ret = rt_sigaction(sig,
		(int)(act ? &kact : NULL),
		(int)(oact ? &koact : NULL),
		stub,
		_NSIG / 8);

	// BUG?! if ret == -1, we return -1 but do not set errno?!
	if (ret >= 0 || errno != ENOSYS) {
		if (oact && ret >= 0) {
			oact->sa_handler = koact.k_sa_handler;
			// maybe oact->sa_mask.__val[0] = koact.sa_mask; ?
			memcpy(&oact->sa_mask, &koact.sa_mask, sizeof(sigset_t));
			oact->sa_flags = koact.sa_flags;
			oact->sa_restorer = koact.sa_restorer;
		}
		return ret;
	}
	__set_errno(saved_errno);
	return -1;
}

#ifndef LIBC_SIGACTION
weak_alias(__libc_sigaction,sigaction)
libc_hidden_weak(sigaction)
#endif

static void
__rt_sigreturn_stub(void)
{
	__asm__(
		"mov %0, %%g1\n\t"
		"ta  0x10\n\t"
		: /* no outputs */
		: "i" (__NR_rt_sigreturn)
	);
}
static void
__sigreturn_stub(void)
{
	__asm__(
		"mov %0, %%g1\n\t"
		"ta  0x10\n\t"
		: /* no outputs */
		: "i" (__NR_sigreturn)
	);
}
