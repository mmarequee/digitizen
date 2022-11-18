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
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <jack/jack.h>
#include "atomic.h"
#include "settings.h"
#include "ringbuffer.h"

inline __attribute__((__gnu_inline__)) void ringbuffer_from_audio_port(ring_buffer_t *ringbuffer, jack_default_audio_sample_t *jack_port_buffer, jack_nframes_t nframes)
{
	
#ifdef DEBUG
	printf("Audio thread; Pre-CAS; record ringbuffer: \n\t start \t\t %p end \t\t %p \n\t write_ptr \t %p write_cycles \t %ld \n\t record_buffer \t\t\t %p \n\t &record_buffer->write_ptr \t %p\n", 
	       		ringbuffer->ringbuffer_start, ringbuffer->ringbuffer_end, ringbuffer->write_ptr, ringbuffer->write_cycles, ringbuffer, &ringbuffer->write_ptr);
#endif

		/* Atomically increase number of write cycles and reset write pointer to 		*/
		/*	start of record ringbuffer  if write pointer reached end of record ringbuffer */
#ifdef __x86_64__
	__CMPXCHG_128(&ringbuffer->write_ptr, 	ringbuffer->ringbuffer_end, 	ringbuffer->write_cycles,
	     																ringbuffer->ringbuffer_start,	ringbuffer->write_cycles + 1);
#else
	__CMPXCHG_64(&ringbuffer->write_ptr, 	ringbuffer->ringbuffer_end, 	ringbuffer->write_cycles,
	     																ringbuffer->ringbuffer_start,	ringbuffer->write_cycles + 1);
#endif
	
#ifdef DEBUG
	printf("Audio thread; Post-CAS; record ringbuffer: \n\t start \t\t %p end \t\t %p \n\t write_ptr \t %p write_cycles \t %ld \n\t record_buffer \t\t\t %p \n\t &record_buffer->write_ptr \t %p\n", 
	       		ringbuffer->ringbuffer_start, ringbuffer->ringbuffer_end, ringbuffer->write_ptr, ringbuffer->write_cycles, ringbuffer, &ringbuffer->write_ptr);
#endif

	memcpy (ringbuffer->write_ptr, jack_port_buffer, (size_t)(nframes << 2)); /* << 2 = sizeof(jack_default_audio_sample_t)) */

	__ATOMIC_ADDQ_PTR_64 (&ringbuffer->write_ptr, (ptrdiff_t)(nframes << 2)); 
	/*just to keep in mind - write_ptr++ increments by sizeof type pointed to by write_ptr */
}

inline __attribute__((__gnu_inline__)) void ringbuffer_to_file(ring_buffer_t *ringbuffer, ring_buffer_pos_t *ringbuffer_pos, int file_handle)
{
	
	long block_size 					= 0;
	long ringbuffer_alignment 	= 0;
 
		/* sample_size - we use RAW MONO streams exclusively 	*/

#ifdef DEBUG
	printf("Record thread; Pre-CAS; record ringbuffer: \n\t start \t\t %p end \t\t %p \n\t read_ptr \t %p read_cycles \t %ld \n\t record_buffer \t\t\t %p \n\t &record_buffer->read_ptr \t %p\n", 
	     		ringbuffer->ringbuffer_start, ringbuffer->ringbuffer_end, ringbuffer->read_ptr, ringbuffer->read_cycles, ringbuffer, &ringbuffer->read_ptr);
#endif

		/* Atomically increase number of read cycles and reset read pointer to 		*/
		/*	start of record ringbuffer  if read pointer reached end of record ringbuffer */

#ifdef __x86_64__
		__CMPXCHG_128(&ringbuffer->read_ptr, 	ringbuffer->ringbuffer_end, 	ringbuffer->read_cycles,
	     																	ringbuffer->ringbuffer_start,  	ringbuffer->read_cycles + 1);
#else
		__CMPXCHG_64(&ringbuffer->read_ptr, 	ringbuffer->ringbuffer_end, 	ringbuffer->read_cycles,
		     																ringbuffer->ringbuffer_start,  	ringbuffer->read_cycles + 1);
#endif

#ifdef DEBUG
	printf("Record thread; Post-CAS; record ringbuffer: \n\t start \t\t %p end \t\t %p \n\t read_ptr \t %p read_cycles \t %ld \n\t record_buffer \t\t\t %p \n\t &record_buffer->read_ptr \t %p\n", 
	     		ringbuffer->ringbuffer_start, ringbuffer->ringbuffer_end, ringbuffer->read_ptr, ringbuffer->read_cycles, ringbuffer, &ringbuffer->read_ptr);
#endif

		/* cache the write_ptr and write_cycles atomically, as it might have changed during JACK process() execution */

#ifdef __DEBUG_RECORD_
		printf("------------------------------------------------------\n ringbuffer_pos->ptr \t %p \n ringbuffer->cycles \t\t %ld\n-\n", 
		       		ringbuffer_pos->ptr, 
		       		ringbuffer_pos->cycles);
#endif

		__CMPXCHG_128(ringbuffer_pos, 		ringbuffer_pos->ptr,	ringbuffer_pos->cycles,
	    															ringbuffer->write_ptr,	ringbuffer->write_cycles);

		usleep(100); 
		/*why do we need this? makes things a lot faster probably because of "loop as fast as possible - 100% cpu load" issue	*/
		/* without it, it appears as if cmpxchg16b was executed towards the end of thread loop, thus block_size gets calculated 	*/
		/* from old or invalid values. Position of usleep() within code seems to be important too...													*/
	 
		block_size = (long)(ringbuffer_pos->ptr - ringbuffer->read_ptr) + ((ringbuffer_pos->cycles - ringbuffer->read_cycles) << 18);
		/* << 18 equals DEFAULT_RINGBUFFER_SIZE * (w_cycles - r_cycles) 			*/

	
		ringbuffer_alignment = (long)(ringbuffer->ringbuffer_end - ringbuffer->read_ptr);

	
#ifdef __DEBUG_RECORD_
			printf(" ringbuffer_pos->ptr \t %p \n write_ptr \t\t %p \n write_cycles \t\t\t %ld \n read_cycles \t\t\t %ld \n block_size (not masked): \t %ld \n ringbuffer alignment \t\t %ld\n-\n", 
			       		ringbuffer_pos->ptr, 
			       		ringbuffer->write_ptr, 
			       		ringbuffer_pos->cycles,
			       		ringbuffer->read_cycles,
			       		block_size, 
			       		ringbuffer_alignment);
#endif
	
		/* the following if() statement should require only cmp and jle instructions	*/
		if (block_size > 0)
		/* if (record_buffer->read_ptr < record_buffer->write_ptr || record_buffer->read_cycles < record_buffer->write_cycles) */
		{

#ifdef __DEBUG_RECORD_
			printf("block_size not masked before alignment: \t %ld\n", block_size);
#endif
			
			if (block_size > DEFAULT_RINGBUFFER_SIZE) overruns++;
			
			if (block_size > ringbuffer_alignment) 
				block_size = ringbuffer_alignment;

#ifdef __DEBUG_RECORD_
			printf("block_size not masked after alignment: \t\t %ld\n", block_size);
#endif
			
			/* make blocksize power of 2 */
			block_size &= 0xFFFF0000; 

#ifdef __DEBUG_RECORD_
			printf("block_size masked: \t\t\t\t %ld\n", block_size);
#endif
			
			/* the following if() statement should require only cmp and jnz instructions */
			if (block_size != 0)
			{
				
#ifdef __DEBUG_RECORD_
				if ( ringbuffer->read_cycles < ringbuffer->write_cycles)
						printf("-\n delta \t\t\t\t %ld \n read_ptr \t %p \n write_ptr \t %p \n read_cycles \t %ld \n write_cycles \t %ld \n block_size \t\t\t %ld \n block_size in-place computation %ld\n",
						       		((	ringbuffer->ringbuffer_end - ringbuffer->read_ptr) + 
								 	 (	ringbuffer_pos->ptr - ringbuffer->ringbuffer_start)), 
						       			ringbuffer->read_ptr, 
										ringbuffer_pos->ptr, 
										ringbuffer->read_cycles, 
										ringbuffer_pos->cycles,
						       			block_size,
						       			((long)(ringbuffer_pos->ptr - ringbuffer->read_ptr) + ((ringbuffer_pos->cycles - ringbuffer->read_cycles) << 18)));
#endif
				
				write(file_handle, ringbuffer->read_ptr, block_size << 2);
				
				__ATOMIC_ADDQ_PTR_64 (&ringbuffer->read_ptr, block_size << 2);

				/* we keep this until we know the cause for usleep(100) after cmpxchg() */
				if (block_size == ((long)(ringbuffer_pos->ptr - ringbuffer->read_ptr) + ((ringbuffer_pos->cycles - ringbuffer->read_cycles) << 18))) overruns++;
			}
		} 
}

inline __attribute__((__gnu_inline__)) char ringbuffer_tail_to_file(ring_buffer_t *ringbuffer,  int file_handle)
{
	
	long block_size 					= 0;
	long tail								= 0;
	long ringbuffer_alignment 	= 0;
 
		/* sample_size - we use RAW MONO streams exclusively 	*/

#ifdef DEBUG
	printf("Record thread; Pre-CAS; record ringbuffer: \n\t start \t\t %p end \t\t %p \n\t read_ptr \t %p read_cycles \t %ld \n\t record_buffer \t\t\t %p \n\t &record_buffer->read_ptr \t %p\n", 
	     		ringbuffer->ringbuffer_start, ringbuffer->ringbuffer_end, ringbuffer->read_ptr, ringbuffer->read_cycles, ringbuffer, &ringbuffer->read_ptr);
#endif

		/* Atomically increase number of read cycles and reset read pointer to 		*/
		/*	start of record ringbuffer  if read pointer reached end of record ringbuffer */

#ifdef __x86_64__
		__CMPXCHG_128(&ringbuffer->read_ptr, 	ringbuffer->ringbuffer_end, 	ringbuffer->read_cycles,
	     																	ringbuffer->ringbuffer_start,  	ringbuffer->read_cycles + 1);
#else
		__CMPXCHG_64(&ringbuffer->read_ptr, 	ringbuffer->ringbuffer_end, 	ringbuffer->read_cycles,
		     																ringbuffer->ringbuffer_start,  	ringbuffer->read_cycles + 1);
#endif

#ifdef DEBUG
	printf("Record thread; Post-CAS; record ringbuffer: \n\t start \t\t %p end \t\t %p \n\t read_ptr \t %p read_cycles \t %ld \n\t record_buffer \t\t\t %p \n\t &record_buffer->read_ptr \t %p\n", 
	     		ringbuffer->ringbuffer_start, ringbuffer->ringbuffer_end, ringbuffer->read_ptr, ringbuffer->read_cycles, ringbuffer, &ringbuffer->read_ptr);
#endif

		/* No need to cache the write_ptr and write_cycles here, as recording has already stopped */

		usleep(100); 
		
		block_size = (long)(ringbuffer->write_ptr - ringbuffer->read_ptr) + ((ringbuffer->write_cycles - ringbuffer->read_cycles) << 18);
		/* << 18 equals DEFAULT_RINGBUFFER_SIZE * (w_cycles - r_cycles) 			*/
	
		ringbuffer_alignment = (long)(ringbuffer->ringbuffer_end - ringbuffer->read_ptr);

	
#ifdef __DEBUG_TAIL_
			printf(" write_ptr \t\t\t %p \n write_cycles \t\t\t %ld \n read_cycles \t\t\t %ld \n block_size (not masked): \t %ld \n ringbuffer alignment \t\t %ld\n-\n", 
			       		ringbuffer->write_ptr, 
			       		ringbuffer->write_cycles,
			       		ringbuffer->read_cycles,
			       		block_size, 
			       		ringbuffer_alignment);
#endif
	
		if (block_size > 0)
		/* if (record_buffer->read_ptr < record_buffer->write_ptr || record_buffer->read_cycles < record_buffer->write_cycles) */
		{

#ifdef __DEBUG_TAIL_
			printf("block_size not masked before alignment: \t %ld\n", block_size);
#endif
			
			if (block_size > DEFAULT_RINGBUFFER_SIZE) overruns++;
			
			if (block_size > ringbuffer_alignment) 
				block_size = ringbuffer_alignment;

#ifdef __DEBUG_TAIL_
			printf("block_size not masked after alignment: \t\t %ld\n", block_size);
#endif
			tail = block_size;
			/* make blocksize power of 2 */
			block_size &= 0xFFFF0000; 

#ifdef __DEBUG_TAIL_
			printf("block_size masked: \t\t\t\t %ld\n", block_size);
#endif
			
			/* the following if() statement should require only cmp and jnz instructions */
			if (block_size != 0)
			{
				
#ifdef __DEBUG_TAIL_
				if ( ringbuffer->read_cycles < ringbuffer->write_cycles)
						printf("-\n delta \t\t\t\t %ld \n read_ptr \t %p \n write_ptr \t %p \n read_cycles \t %ld \n write_cycles \t %ld \n block_size \t\t\t %ld \n block_size in-place computation %ld\n",
						       		((	ringbuffer->ringbuffer_end - ringbuffer->read_ptr) + 
								 	 (	ringbuffer->write_ptr - ringbuffer->ringbuffer_start)), 
						       			ringbuffer->read_ptr, 
										ringbuffer->write_ptr, 
										ringbuffer->read_cycles, 
										ringbuffer->write_cycles,
						       			block_size,
						       			((long)(ringbuffer->write_ptr - ringbuffer->read_ptr) + ((ringbuffer->write_cycles - ringbuffer->read_cycles) << 18)));
#endif
				
				write(file_handle, ringbuffer->read_ptr, block_size << 2);
				
				__ATOMIC_ADDQ_PTR_64 (&ringbuffer->read_ptr, block_size << 2);

				return 0;
			}
			else
			{
#ifdef __DEBUG_TAIL_
			printf("tail: \t\t\t\t\t\t %ld\n", tail);
#endif
				write(file_handle, ringbuffer->read_ptr, tail << 2);
				
				__ATOMIC_ADDQ_PTR_64 (&ringbuffer->read_ptr, tail << 2);

				return 1;
			}
		} 

		return 1;
}

inline __attribute__((__gnu_inline__)) void ringbuffer_reset(ring_buffer_t *ringbuffer)
{
#ifdef __DEBUG_RINGBUFFER_RESET_
		printf("- Before -\n read_ptr \t %p \n write_ptr \t %p \n read_cycles \t %ld \n write_cycles \t %ld \n",
				ringbuffer->read_ptr, 
				ringbuffer->write_ptr, 
				ringbuffer->read_cycles, 
				ringbuffer->write_cycles);
#endif
	
		__CMPXCHG_128(&ringbuffer->write_ptr, 	ringbuffer->write_ptr, 				ringbuffer->write_cycles,
	     																	ringbuffer->ringbuffer_start,  	0);

		__CMPXCHG_128(&ringbuffer->read_ptr, 	ringbuffer->read_ptr, 				ringbuffer->read_cycles,
	     																	ringbuffer->ringbuffer_start,  	0);

#ifdef __DEBUG_RINGBUFFER_RESET_
		printf("- After -\n read_ptr \t %p \n write_ptr \t %p \n read_cycles \t %ld \n write_cycles \t %ld \n",
				ringbuffer->read_ptr, 
				ringbuffer->write_ptr, 
				ringbuffer->read_cycles, 
				ringbuffer->write_cycles);
#endif
}
