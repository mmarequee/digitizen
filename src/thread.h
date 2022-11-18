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

#ifndef __THREAD_H_
#define __THREAD_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

 typedef struct _jack_thread_info 
{
    pthread_t thread_id;
     
    jack_client_t *client;
     
} jack_thread_info_t;

typedef struct _control_thread_info 
{
    pthread_t thread_id;

    int         sock;
    int     msg_sock;
    char        msg_buffer[32];
    struct  sockaddr_in server_addr;
     
} control_thread_info_t;

#endif
