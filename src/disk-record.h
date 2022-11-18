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

#include "thread.h"
#include "fifo-buffer.h"

int                                                 num_frames;

void*   record_thread               (void *arg);
void    initialize_record_thread    (jack_thread_info_t *info);
void    run_record_thread           (jack_thread_info_t *info);
