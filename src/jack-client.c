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
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <jack/jack.h>
#include "jack-connection.h"
#include "jack-client.h"
#include "settings.h"
#include "disk-record.h"
#include "ringbuffer.h"
#include "peak.h"


/*jack_default_audio_sample_t buff[256]; &buff[record_buffer->write_ptr] */
 	
int process (jack_nframes_t nframes, void *arg)
{
	jack_thread_info_t 						*info			 =	(jack_thread_info_t *) arg;
	int 												index			 = 0;
	int												msg_len		 = 0;
	jack_default_audio_sample_t 	*port_buffer = NULL;
	char												*msg_ptr      = NULL;
	
	/* Do nothing until we're ready to begin. */
	if ((!process_enable) || (!capture_mask))
		return 0;

	for(index = 0; index < FORCE_CHANNELS; index++)
	{
		if (capture_mask & (1 << index))
		{
			port_buffer 	=	jack_port_get_buffer (input_ports[index], nframes);

			/* Calculate a CRC32 directly from audio card output */

			msg_len = (int)nframes << 2;  /*  same as * sizeof(float) */
			msg_ptr = (char *)port_buffer;

			while (msg_len--)
			{
					crc32[index] = (crc32[index] >> 8) ^ crc32_tbl[(crc32[index] ^ *msg_ptr++) & 0xFF]; 		
			}

			calculate_peak((float *)port_buffer, &peak[index], (long int)nframes);

			ringbuffer_from_audio_port(&record_buffers[index], port_buffer, nframes);
		}
	}

	return 0;
}

void jack_shutdown (void *arg)
{
	fprintf (stderr, "JACK shutdown\n");
	// exit (0);
	abort();
}
