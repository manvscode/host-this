#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "server.h"

struct server {
    volatile bool running;
    int socket;
    int connection_queue;
    void* user_data;
};

server_t* server_create( int connection_queue, void* user_data )
{
    server_t* server = malloc( sizeof(server_t) );

    if( server )
    {
        server->running          = false;
        server->socket           = 0;
        server->connection_queue = connection_queue;
        server->user_data        = user_data;
    }

    return server;
}

void server_destroy( server_t** server )
{
    if( server && *server )
    {
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

bool server_start( server_t* server, const char* localhost, int port )
{
    server->running = false;
    server->socket = socket( PF_INET6, SOCK_STREAM, 0 );

    struct sockaddr_in6 address = {
        .sin6_family = AF_INET6,
        .sin6_port   = htons(port)
    };

    if( inet_pton( AF_INET6, localhost, &address.sin6_addr ) < 0 )
    {
        fprintf( stderr, "ERROR: Unable to get localhost address\n");
        perror( "Problem" );
        close( server->socket );
        server->socket = 0;
        return false;
    }

    int option_ipv6_only = 0;
    if( setsockopt( server->socket, IPPROTO_IPV6, IPV6_V6ONLY, &option_ipv6_only, sizeof(option_ipv6_only) ) < 0 )
    {
        fprintf( stderr, "ERROR: Unable to set socket option IPV6_V6ONLY\n" );
        perror( "Problem" );
        close( server->socket );
        server->socket = 0;
        return false;
    }

    if( bind( server->socket, (const struct sockaddr *) &address, sizeof(address) ) < 0 )
    {
        fprintf( stderr, "ERROR: Unable to bind to %s:%d\n", localhost, port );
        perror( "Problem" );
        close( server->socket );
        server->socket = 0;
        return false;
    }

    if( listen( server->socket, server->connection_queue ) < 0 )
    {
        fprintf( stderr, "ERROR: Unable to listen\n" );
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
        handle_connection( server, peer_socket, &peer_address, server->user_data );
        close( peer_socket );
    }

    close( server->socket );
}

