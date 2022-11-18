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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <netdb.h>
#include <jack/jack.h>
#include "disk-record.h"
#include "settings.h"
#include "atomic.h"
#include "ringbuffer.h"
#include "control.h"


void * control_thread (void *arg)
{
	control_thread_info_t 	*server 				= (control_thread_info_t *) arg;
	int								rval						= 0;
	int								machine_index 	= 0;
	char								*param				= NULL;
	char								*pos					= NULL;
	char								buff[64];	
	/* max filename length for EXT2 is 255*/

	if (listen(server->sock, 1) != 0)
	{ 
		perror("listening for connections");
		exit(1);
	}

	server->msg_sock =  accept(server->sock, (struct sockaddr *)0, (unsigned int *)0);
		
		if (server->msg_sock == -1) perror("accept");
else 
		do 
		{
			memset(server->msg_buffer, 0, sizeof(server->msg_buffer));

					if	((rval  =  read(server->msg_sock, server->msg_buffer, sizeof(server->msg_buffer))) < 0) 
							perror("reading stream message");
			else if	(rval == 0) 
							printf("Ending connection\n");
			else 
			{
					if(server->msg_buffer[0] == 0) continue;
					/* dubious skip if msg_buffer empty */

					
				
					param = strtok_r(server->msg_buffer, 	" ", &pos);
				
					if (strncasecmp (param, "QUIT", 4) == 0)
					{
						if(capture_mask != 0)
						{
							memset(buff, 0, sizeof(buff));
	
							strcpy(buff, "Still recording\n");
							send(server->msg_sock, buff, sizeof(buff), 0);
							
							continue;
						}

						memset(buff, 0, sizeof(buff));
	
						strcpy(buff, "Exiting\n");
						send(server->msg_sock, buff, sizeof(buff), 0);

						quit 	= 1;
						break;
					}
			else if (strcasecmp (param, "START") == 0) 
					{
						param = strtok_r(NULL, " ", &pos);

						if (param == NULL)
							continue;

						machine_index = atoi(param); 
						
						if (machine_index <= 0 || machine_index > (FORCE_CHANNELS >> 1))
						{
							memset(buff, 0, sizeof(buff));
							
							strcpy(buff, "Wrong machine\n");
							send(server->msg_sock, buff, sizeof(buff), 0);

							continue;
						}

						machine_index--; 
						machine_index <<= 1;

						/*Check if we are recording on requested channels */

						if((capture_mask & (3 << machine_index)) == (3 << machine_index))
						{
							memset(buff, 0, sizeof(buff));
							
							sprintf(buff, "REC enabled %d\n", (machine_index >> 1) + 1);
							send(server->msg_sock, buff, sizeof(buff), 0);

							continue;
						}

						param = strtok_r(NULL, " ", &pos);

						if (param == NULL)
						{
							memset(buff, 0, sizeof(buff));
							
							strcpy(buff, "No filename\n");
							send(server->msg_sock, buff, sizeof(buff), 0);

							continue;
						}

						memset(buff, 0, sizeof(buff));
						
						sprintf(buff, "Channel %d %d\n", machine_index, machine_index + 1);
						send(server->msg_sock, buff, sizeof(buff), 0);

						memset(filename[machine_index].name, 0, sizeof(filename[machine_index].name));  
																							/* could be substituted for #define MAX_FILENAME_LENGTH */
						if((pos = strrchr(param, 0x0D)) != NULL)  	/* search for carriage return or "enter" char 							*/
							strncpy(filename[machine_index].name, param, pos - param);
						else
							strcpy(filename[machine_index].name, param);
						strcat(filename[machine_index].name, "_01"); 
		
						if ((file[machine_index] 		= open (filename[machine_index].name, O_RDWR|O_TRUNC|O_CREAT, 0666)) == 0)
						{
							printf("Couldn't open file %s\n", filename[machine_index + 1].name);
							continue;
						}

						crc32[machine_index		] = 0xFFFFFFFF;	/* initialize the CRC32 remainder value before computing		*/

						memset(filename[machine_index + 1].name, 0, sizeof(filename[machine_index + 1].name));  					
																							/* could be substituted for #define MAX_FILENAME_LENGTH 	*/
						if((pos = strrchr(param, 0x0D)) != NULL)  	/* search for newline																*/
							strncpy(filename[machine_index + 1].name, param, pos - param);
						else
							strcpy(filename[machine_index + 1].name, param);
						strcat(filename[machine_index + 1].name, "_02");

						if ((file[machine_index + 1]	= open (filename[machine_index + 1].name, O_RDWR|O_TRUNC|O_CREAT, 0666)) == 0)
						{
							printf("Couldn't open file %s\n", filename[machine_index + 1].name);
							continue;
						}

						crc32[machine_index	+ 1] = 0xFFFFFFFF;	/* initialize the CRC32 remainder value before computing		*/

						/* Needs to be the LAST step if everything is prepared for recording  */

						__asm__ __volatile__ (
	    				"lock 								\n\t"
	    				"orb			%1, (%0)			\n\t"
						:
						:"r" (&capture_mask), "r" ((unsigned char)(3 << machine_index))
						);

						/* an atomic OR version of capture_mask |= (3 << machine_index); 	*/
						/* or: 	capture_mask |= (1 <<	 machine_index 		);						*/ 
						/*			capture_mask |= (1 <<	(machine_index + 1));						*/

						__asm__ __volatile__ (
	    				"lock 								\n\t"
	    				"andb		%1, (%0)			\n\t"
						:
						:"r" (&completed), "r" ((unsigned char)((3 << machine_index) ^ 0xFF))
						);
					}
			else if (strcasecmp (param, "STOP") == 0) 
					{
						param = strtok_r(NULL, " ", &pos);

						if (param == NULL)
							continue;

						machine_index = atoi(param); 
						
						if (machine_index <= 0 || machine_index > (FORCE_CHANNELS >> 1))
						{
							memset(buff, 0, sizeof(buff));
							
							strcpy(buff, "Wrong machine\n");
							send(server->msg_sock, buff, sizeof(buff), 0);

							continue;
						}

						machine_index--; 
						machine_index <<= 1;

						/*Check if we are recording on requested channels */

						if((capture_mask & (3 << machine_index)) != (3 << machine_index))
						{
							memset(buff, 0, sizeof(buff));
							
							sprintf(buff, "REC already disabled %d\n", (machine_index >> 1) + 1);
							send(server->msg_sock, buff, sizeof(buff), 0);

							continue;
						}

						/* Needs to be the FIRST step if everything is prepared for stop */

						__asm__ __volatile__ (
	    				"lock 								\n\t"
	    				"andb		%1, (%0)			\n\t"
						:
						:"r" (&capture_mask), "r" ((unsigned char)((3 << machine_index) ^ 0xFF))
						);

						/* an atomic AND version of capture_mask &= (3 << machine_index) ^ 0xFF; 	*/
						/* or: 	capture_mask &= (1 <<	 machine_index 		) ^ 0xFF;							*/ 
						/*			capture_mask &= (1 <<	(machine_index + 1)) ^ 0xFF;							*/
						
						/* we need to wait until tail has been written! */ 
						/* record_buffers[machine_index      ].write_ptr != record_buffers[machine_index      ].read_ptr */
						/* record_buffers[machine_index + 1].write_ptr != record_buffers[machine_index + 1].read_ptr */

						while((completed & (3 << machine_index)) != (3 << machine_index))
						{
							usleep(100);
						}

#ifdef __DEBUG_CONTROL_
						printf("\n--------------------- CRC32  machine %d -----------------------\n", ((machine_index+ 1) >> 1) +1);
						printf(" CRC32 of channel 01: %.8x\n", ~crc32[machine_index]);
						printf(" read_ptr \t %p \n read_cycles \t %ld \n write_ptr \t %p \n write_cycles \t %ld \n diff \t\t %ld\n\n", 
						       		record_buffers[machine_index].read_ptr, 
						       		record_buffers[machine_index].read_cycles, 
						       		record_buffers[machine_index].write_ptr, 
						       		record_buffers[machine_index].write_cycles,
						       		(ptrdiff_t)(record_buffers[machine_index].write_ptr - record_buffers[machine_index].read_ptr));
						printf("--------------------- CRC32 end ------------------------------\n");
#endif

						fsync (file[machine_index     ]);
						close (file[machine_index     ]);
						
#ifdef __DEBUG_CONTROL_
						printf("\n--------------------- CRC32  machine %d -----------------------\n", ((machine_index+ 1) >> 1) +1);
						printf(" CRC32 of channel 02: %.8x\n", ~crc32[machine_index + 1]);
						printf(" read_ptr \t %p \n read_cycles \t %ld \n write_ptr \t %p \n write_cycles \t %ld \n diff \t\t %ld\n\n", 
						       		record_buffers[machine_index + 1].read_ptr, 
						       		record_buffers[machine_index + 1].read_cycles, 
						       		record_buffers[machine_index + 1].write_ptr, 
						       		record_buffers[machine_index + 1].write_cycles,
						       		(ptrdiff_t)(record_buffers[machine_index + 1].write_ptr - record_buffers[machine_index + 1].read_ptr));
						printf("--------------------- CRC32 end ------------------------------\n");
#endif

						fsync (file[machine_index +1]);
						close (file[machine_index +1]);

						memset(buff, 0, sizeof(buff));
						sprintf(buff, "O: %ld L: %.3f R: %.3f\n", overruns, peak[machine_index], peak[machine_index + 1]);
						send(server->msg_sock, buff, sizeof(buff), 0);

						peak[machine_index] = peak[machine_index + 1] = 0.0;

						memset(buff, 0, sizeof(buff));
						sprintf(buff, "%s %.8x\n%s %.8x\n", filename[machine_index].name, ~crc32[machine_index], filename[machine_index + 1].name, ~crc32[machine_index + 1]); 

						write(sfv, buff, strlen(buff));
					}
			else if (strcasecmp (param, "RQOPK") == 0) /* request overruns and peak values for given channels */
					{
						param = strtok_r(NULL, " ", &pos);

						if (param == NULL)
							continue;

						machine_index = atoi(param); 
						
						if (machine_index <= 0 || machine_index > (FORCE_CHANNELS >> 1))
						{
							memset(buff, 0, sizeof(buff));
							
							strcpy(buff, "Wrong machine\n");
							send(server->msg_sock, buff, sizeof(buff), 0);

							continue;
						}

						machine_index--; 
						machine_index <<= 1;

						/*Check if we are recording on requested channels */

						if((capture_mask & (3 << machine_index)) != (3 << machine_index))
						{
							memset(buff, 0, sizeof(buff));
							
							sprintf(buff, "REC disabled %d\n", (machine_index >> 1) + 1);
							send(server->msg_sock, buff, sizeof(buff), 0);

							continue;
						}
						
						memset(buff, 0, sizeof(buff));
						sprintf(buff, "O: %ld L: %.3f R: %.3f\n", overruns, peak[machine_index], peak[machine_index + 1]);
						send(server->msg_sock, buff, sizeof(buff), 0);
					}
			else 
					{
						memset(buff, 0, sizeof(buff));
							
						sprintf(buff, "Wrong command\n");
						send(server->msg_sock, buff, sizeof(buff), 0);

						continue;
					}			
			}
		} while (rval > 0);

	close(server->msg_sock);

	return 0;
}


void init_control_thread(void *s_addr, int *sock)
{
	struct 					sockaddr_in *server = (struct sockaddr_in *)s_addr;
	unsigned int 		length;

	*sock = socket(AF_INET, SOCK_STREAM, 0);
	
	if (*sock < 0) 
	{ 
		perror("opening stream socket");
		exit(1);
	}
	
	server->sin_family 			= AF_INET; 
	server->sin_addr.s_addr	= INADDR_ANY;
	server->sin_port				= 0; 

	length = sizeof(struct sockaddr_in); 

	if (bind(*sock, (struct sockaddr *)server, length)) 
	{
		perror("binding stream socket");
		exit(1);
	}

	/* Find out assigned port number */
	
	if (getsockname(*sock, (struct sockaddr *)server, &length) != 0) 
	{ 
		perror("getting socket name");
		exit(1);
	}
	
 	printf("Socket has port #%d\n", ntohs(server->sin_port));
}
