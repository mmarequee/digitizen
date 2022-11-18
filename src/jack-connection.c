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

void setup_ports (jack_port_t **ports, int nports, jack_thread_info_t *info) 
{
	size_t i;

	char name[64];

	for (i = 0; i < nports; i++) 
	{
		memset(name, 0, 64);
		
		sprintf (name, "digitizen_input%ld", (long)(i + 1));  /* pass name as function parameter */

		if ((ports[i] = jack_port_register (info->client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0)) == 0) 
		{
			fprintf (stderr, "Unable to register input port \"%s\"!\n", name);
			jack_client_close (info->client);
			exit (1);
		}
	}
	
	process_enable = 1;		/* process() can start now */
}

	
/*
 * We don't connect to anything at this time as this might be later given by a special profile describing audio hardware
 *	for (i = 0; i < nports; i++) 
 *	{
 *		if (jack_connect (info->client, source_names[i], jack_port_name (ports[i]))) 
 *		{
 *			fprintf (stderr, "cannot connect input port %s to %s\n", jack_port_name (ports[i]), source_names[i]);
 *			jack_client_close (info->client);
 *			exit (1);
 *		}
 *	}
 */
