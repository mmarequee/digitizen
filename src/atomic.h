/*
 * atomic.c
 * Copyright (C) Marek Peteraj 2009 <marek.peteraj@pm.me>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#ifndef __ATOMIC_H_
#define __ATOMIC_H_

static inline char __CMPXCHG_64 (volatile void * addr, volatile void * v1, volatile long v2, void * n1, long n2) 
{
    register char ret;
    __asm__ __volatile__ (
        "lock                       \n\t"
        "cmpxchg8b (%1)     \n\t"
        "sete %0                    \n\t"
        :"=a" (ret)
        :"D" (addr), "d" (v2), "a" (v1), "c" (n2), "b" (n1)
    );
    return ret;
}

static inline char __CMPXCHG_128 (volatile void * addr, volatile void * v1, volatile long v2, void * n1, long n2) 
{
    register char ret;
    __asm__ __volatile__ (
        "lock                       \n\t"
        "cmpxchg16b (%1)    \n\t"
        "sete %0                    \n\t"
        :"=a" (ret)
        :"D" (addr), "d" (v2), "a" (v1), "c" (n2), "b" (n1)
    );
    return ret;
}

static __inline__ void __ATOMIC_ADDQ_PTR_64(void *v, long i)
{
    __asm__ __volatile__(
        "lock                       \n\t"
        "addq %1,   (%0)    \n\t"
        :
        :"D" (v), "ir" (i)
    );
}

#endif
