/* Copyright (C) 2016 by Joseph A. Marrero, http://www.joemarrero.com/
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <collections/vector.h>
#include "server.h"

#define SERVER_NON_BLOCKING  0

struct server {
	volatile bool running;
	int socket;
	bool use_ip4;
	int connection_queue;
	int* peers;
	void* user_data;
};

server_t* server_create( bool use_ip4, int connection_queue, void* user_data )
{
	server_t* server = malloc( sizeof(server_t) );

	if( server )
	{
		server->use_ip4          = use_ip4;
		server->running          = false;
		server->socket           = 0;
		server->connection_queue = connection_queue;
		server->user_data        = user_data;
		server->peers            = NULL;

		lc_vector_create(server->peers, 1);
	}

	return server;
}

void server_destroy( server_t** server )
{
	if( server && *server )
	{
		lc_vector_destroy((*server)->peers);
		free( *server );
		*server = NULL;
	}
}

int server_socket( server_t* server )
{
	return server ? server->socket : -1;
}

bool server_is_running( server_t* server )
{
	return server ? server->running : false;
}

bool server_start( server_t* server, const char* address_string, int port )
{
	server->running = false;
	server->socket = socket( server->use_ip4 ? PF_INET : PF_INET6, SOCK_STREAM, 0 );

	if( !server->use_ip4 )
	{
		int option_ipv6_only = 0;
		if( setsockopt( server->socket, IPPROTO_IPV6, IPV6_V6ONLY, &option_ipv6_only, sizeof(option_ipv6_only) ) < 0 )
		{
			fprintf( stderr, "ERROR: Unable to set socket option IPV6_V6ONLY.\n" );
			perror( "Problem" );
			close( server->socket );
			server->socket = 0;
			return false;
		}
	}

	int option_reuse_address = 1;
	if( setsockopt( server->socket, SOL_SOCKET, SO_REUSEADDR, &option_reuse_address, sizeof(option_reuse_address) ) < 0 )
	{
		fprintf( stderr, "ERROR: Unable to set socket option SO_REUSEADDR.\n" );
		perror( "Problem" );
		close( server->socket );
		server->socket = 0;
		return false;
	}

	void* address = NULL;
	size_t address_size = 0;

	if( server->use_ip4 )
	{
		struct sockaddr_in address4 = {
			.sin_addr   = INADDR_ANY, // default to bind to all addresses
			.sin_family = AF_INET,
			.sin_port   = htons(port)
		};
		address = &address4;
		address_size = sizeof(address4);

		if( address_string && inet_pton( AF_INET, address_string, &address4.sin_addr ) < 0 )
		{
			fprintf( stderr, "ERROR: Unable to get address_string address.\n");
			perror( "Problem" );
			close( server->socket );
			server->socket = 0;
			return false;
		}
	}
	else
	{
		struct sockaddr_in6 address6 = {
			.sin6_addr   = INADDR_ANY, // default to bind to all addresses
			.sin6_family = AF_INET6,
			.sin6_port   = htons(port)
		};
		address = &address6;
		address_size = sizeof(address6);

		if( address_string && inet_pton( AF_INET6, address_string, &address6.sin6_addr ) < 0 )
		{
			fprintf( stderr, "ERROR: Unable to get address_string address.\n");
			perror( "Problem" );
			close( server->socket );
			server->socket = 0;
			return false;
		}
	}

#if SERVER_NON_BLOCKING
	int status = fcntl(server->socket, F_SETFL, fcntl(server->socket, F_GETFL, 0) | O_NONBLOCK);
	if (status == -1)
	{
		fprintf( stderr, "ERROR: Unable to set socket to be non-blocking.\n" );
		perror( "Problem" );
	}
#endif

	if( bind( server->socket, (const struct sockaddr*) address, address_size ) < 0 )
	{
		fprintf( stderr, "ERROR: Unable to bind to %s:%d.\n", address_string, port );
		perror( "Problem" );
		close( server->socket );
		server->socket = 0;
		return false;
	}

	if( listen( server->socket, server->connection_queue ) < 0 )
	{
		fprintf( stderr, "ERROR: Unable to listen.\n" );
		perror( "Problem" );
		close( server->socket );
		server->socket = 0;
		return false;
	}

	return true;
}

void server_stop( server_t* server )
{
	server->running = false;


	for( int i = 0; i < lc_vector_size(server->peers); i++ )
	{
		close(server->peers[ i ]);
	}
	lc_vector_clear(server->peers);


	close( server->socket );
	server->socket = -1;
}

void server_run( server_t* server, server_connection_fxn_t handle_connection )
{
	server->running = true;

	while( server->running )
	{
		struct sockaddr_storage peer_address;
		socklen_t peer_address_len = sizeof(peer_address);

		int peer_socket = accept( server->socket, (struct sockaddr *) &peer_address, &peer_address_len );
#if SERVER_NON_BLOCKING
		if( peer_socket == -1)
		{
			if (errno == EAGAIN)
			{
				printf("Waiting...\n");
			}
		}
		else
#endif
		{
			lc_vector_push( server->peers, peer_socket );

			handle_connection( server, peer_socket, &peer_address, server->user_data );


			for( int i = 0; i < lc_vector_size(server->peers); i++ )
			{
				if( server->peers[ i ] == peer_socket )
				{
					server->peers[ i ] = lc_vector_last(server->peers);
					lc_vector_pop(server->peers);

				}
			}

			close( peer_socket );
		}
	}

	close( server->socket );
}

