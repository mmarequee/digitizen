/*
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

#ifndef __RINGBUFFER_H_
#define __RINGBUFFER_H_

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <jack/jack.h>
#include "atomic.h"
#include "settings.h"

/* __attribute__((aligned(16))); needs to be #ifdef'd to x86_64, on 32-bit systems this would be (8) */

typedef struct _ring_buffer {
     
    jack_default_audio_sample_t *   volatile    ringbuffer_start;                                                           /* always points at start of ring buffer        */

    jack_default_audio_sample_t *   volatile    read_ptr            __attribute__((aligned(16)));           /* gets changed by reading thread ONLY! */
    long                            volatile    read_cycles;                                                                    

    jack_default_audio_sample_t *   volatile    write_ptr           __attribute__((aligned(16)));           /* gets changed by writing thread ONLY! */
    long                            volatile    write_cycles;

    jack_default_audio_sample_t *   volatile    ringbuffer_end;
     
} ring_buffer_t;


typedef struct _ring_buffer_pos
{
    jack_default_audio_sample_t  *  ptr;
    long                            cycles;
    
} ring_buffer_pos_t;
    

ring_buffer_t           *record_buffers;
ring_buffer_pos_t       *write_info;


inline __attribute__((__gnu_inline__)) void ringbuffer_from_audio_port  (ring_buffer_t *ringbuffer, jack_default_audio_sample_t *jack_port_buffer, jack_nframes_t nframes);
inline __attribute__((__gnu_inline__)) void ringbuffer_to_file          (ring_buffer_t *ringbuffer, ring_buffer_pos_t *ringbuffer_pos, int file_handle);
inline __attribute__((__gnu_inline__)) char ringbuffer_tail_to_file     (ring_buffer_t *ringbuffer,  int file_handle);
inline __attribute__((__gnu_inline__)) void ringbuffer_reset            (ring_buffer_t *ringbuffer);

#endif
