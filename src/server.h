#ifndef __SERVER_H__
#define __SERVER_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

struct server;
typedef struct server server_t;

typedef void (*server_connection_fxn_t)( server_t* server, int peer_socket, struct sockaddr_storage* peer_address, void* user_data );

server_t* server_create  ( int connection_queue, void* user_data );
void      server_destroy ( server_t** server );
bool      server_start   ( server_t* server, const char* localhost, int port );
void      server_stop    ( server_t* server );
void      server_run     ( server_t* server, server_connection_fxn_t handle_connection );

#endif /* __SERVER_H__ */
