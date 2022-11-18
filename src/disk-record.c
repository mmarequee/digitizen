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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <fcntl.h>
#include <jack/jack.h>
#include "disk-record.h"
#include "settings.h"
#include "atomic.h"
#include "ringbuffer.h"


void * record_thread (void *arg)
{
	jack_thread_info_t 						*info	= (jack_thread_info_t *) arg;
	int 												index	= 0;

	while (!quit) 
	{ 
		for(index = 0; index < FORCE_CHANNELS; index++)
		{
			if (capture_mask & (1 << index)) 
				ringbuffer_to_file(&record_buffers[index], &write_info[index], file[index]);
	else 
			if (completed 		& (1 << index)) 	/* record_buffers[index].write_ptr == record_buffers[index].read_ptr  					*/
				usleep(100);								/* we cannot use the _ptrs directly if we want to reset the ringbuffers					*/
	else															/* the only way would be to reset both write_ptr + read_ptr atomically at once 	*/ 
			{
				/* tail writing should be done in one pass but it can't be done here because a										*/
				/* while(!ringbuffer_tail_to_file(&record_buffers[index], file[index])) would hold off the entire loop		*/
				/* on the other hand the ringbuffers get reused for recording another file so if tail writing isn't			*/
				/* finished in one pass it might never finish if record started on that given channel is triggered again	*/
				if(ringbuffer_tail_to_file(&record_buffers[index], file[index]))
				{
					ringbuffer_reset (&record_buffers[index]);
					
					__asm__ __volatile__ (
	    					"lock 								\n\t"
	    					"orb			%1, (%0)			\n\t"
							:
							:"r" (&completed), "r" ((unsigned char)(1 << index))
							);
				}
			}
		}
		/* we need to be as fast as (nframes) /  ((sample_rate in Hz) / 1000ms) in ms */ 
	}

		return 0;
}
 
void run_record_thread (jack_thread_info_t *info)
{
	pthread_join (info->thread_id, NULL);
	
	if (overruns > 0) 
	{
		fprintf (stderr, "DiGitiZEN encountered %ld overruns.\n", overruns);
		fprintf (stderr, "You should try a bigger buffer than %"PRIu32 ".\n", DEFAULT_RINGBUFFER_SIZE);
	}
}
