/* stubs.c
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * This file is part of wolfssl-examples.
 *
 * wolfssl-examples is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfssl-examples is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/*
 * Syscall stubs for newlib-nano (nosys.specs).
 * Provides _sbrk for malloc, a minimal time() for wolfCrypt and the file
 * syscalls stdio pulls in. _write is the UART retarget in hw_init.c.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>

#include "board.h"

/* The heap grows up from the end of .bss to the bottom of the stack */
void *_sbrk(ptrdiff_t incr)
{
    static uintptr_t heap_end;
    uintptr_t prev;
    uintptr_t stack_limit;

    if (heap_end == 0) {
        heap_end = (uintptr_t)_end;
    }

    stack_limit = (uintptr_t)_estack - (uintptr_t)_Min_Stack_Size;

    if (incr > 0 && (uintptr_t)incr > stack_limit - heap_end) {
        errno = ENOMEM;
        return (void *)-1;
    }

    prev = heap_end;
    heap_end += incr;
    return (void *)prev;
}

/* No RTC: a monotonic counter starting at 2026-01-01 */
static volatile time_t fake_time_counter = 1767225600;

time_t time(time_t *t)
{
    time_t v = fake_time_counter++;

    if (t != NULL) {
        *t = v;
    }
    return v;
}

/* stdin / stdout / stderr are the UART, there are no files */
int _close(int file)
{
    (void)file;
    return -1;
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}
