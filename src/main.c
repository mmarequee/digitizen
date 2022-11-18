/*
 * Copyright (C) Marek Peteraj 2009 <marek.peteraj@pm.me>
 *
 * main.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * main.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include "disk-record.h"
#include "jack-client.h"
#include "jack-connection.h"
#include "settings.h"
#include "thread.h"
#include "ringbuffer.h"
#include "control.h"

int main()
{

	jack_client_t 				*client;
	jack_thread_info_t 		record_thread_info;
	control_thread_info_t	control_thread_info;

	overruns 			= 0; 
	sample_size 	= sizeof(jack_default_audio_sample_t);

	memset (&record_thread_info, 0, sizeof (record_thread_info));

	/* When JACK runs in realtime, jack_activate() 
	 * calls mlockall() to lock pages of a JACK client into memory.  
	 * Its probably necessary to touch any newly allocated pages before
	 * process() starts using them, otherwise a page fault could
	 * create a delay that would force JACK to shut the client down. 
	 */
	
	record_buffers								= 								malloc(sizeof(ring_buffer_t) 			* FORCE_CHANNELS);
	write_info 									= 								malloc(sizeof(ring_buffer_pos_t)	* FORCE_CHANNELS);
	input_ports 									= (jack_port_t **) 	malloc (sizeof (jack_port_t *) 			* FORCE_CHANNELS);

	completed = 0xFF;

	/* reset peak values */
	memset (&peak[0], 0, sizeof (peak));

	file = malloc(sizeof(int) * FORCE_CHANNELS);

	int index;

	for(index = 0; index < FORCE_CHANNELS; index++)
	{
		record_buffers[index].ringbuffer_start	= malloc(sizeof(jack_default_audio_sample_t) * DEFAULT_RINGBUFFER_SIZE);

		memset(record_buffers[index].ringbuffer_start, 0, DEFAULT_RINGBUFFER_SIZE * sizeof(jack_default_audio_sample_t));
	
		record_buffers[index].read_ptr			= record_buffers[index].write_ptr 	= record_buffers[index].ringbuffer_start;
		record_buffers[index].read_cycles 		= record_buffers[index].write_cycles = 0;
		record_buffers[index].ringbuffer_end	= record_buffers[index].ringbuffer_start + DEFAULT_RINGBUFFER_SIZE;
	}
	
#ifdef DEBUG
	printf("ringbuffer  starting position %p ending position %p read_ptr %p write_ptr %p\n", record_buffers[index].ringbuffer_start, record_buffers[index].ringbuffer_end, record_buffers[index].read_ptr, record_buffers[index].write_ptr);

#endif
	/* open session.sfv file containing filenames anc CRC32 values for a given session */
	
	if ((sfv	= open ("session.sfv", O_RDWR|O_TRUNC|O_CREAT, 0666)) == 0)
	{
		printf("Couldn't open file session.sfv\n");
	}

	/* Initialize socket based IPC for server */
	
	init_control_thread(&control_thread_info.server_addr, &control_thread_info.sock);
	pthread_create 	(&control_thread_info.thread_id, NULL, control_thread, &control_thread_info);
	pthread_join 		(&control_thread_info.thread_id, NULL);
	
	if ((client = jack_client_new ("DiGitiZEN")) == 0) 
	{
		fprintf (stderr, "It seems that JACK server is not running.\n");
		exit (1);
	}

	record_thread_info.client = client;

	process_enable 	= 0;
	capture_mask	 	= 0;
	quit						= 0;

	pthread_create (&record_thread_info.thread_id, NULL, record_thread, &record_thread_info);

	num_frames = jack_get_buffer_size (client); 

	jack_set_process_callback (client, process, &record_thread_info);
	jack_on_shutdown (client, jack_shutdown, &record_thread_info);

	if (jack_activate (client)) 
	{
		fprintf (stderr, "cannot activate client");
	}	
		
	setup_ports (input_ports, FORCE_CHANNELS, &record_thread_info);

	run_record_thread (&record_thread_info);

	jack_client_close (client);

	close(sfv);
	
	free(write_info);
	
	for(index = 0; index < FORCE_CHANNELS; index++)
		free(record_buffers[index].ringbuffer_start);

	free(input_ports);
	free(file);

	exit (0); 
};
