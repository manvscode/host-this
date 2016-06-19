#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#define VECTOR_GROW_AMOUNT(array)      (10)
#include <libcollections/vector.h>
#include <libcollections/buffer.h>
#include <libutility/utility.h>
#include "server.h"
#include "textbuffer.h"

#define CONNECTION_QUEUE 5
#ifndef MAX_PATH
#define MAX_PATH   256
#endif

#define VERSION "0.5"

struct {
    server_t* server;
    bool verbose;
    const char* title;
    const char* path;
    const char* address;
    short port;
} app_state = {
    .server  = NULL,
    .verbose = false,
    .title   = "Hosting Files",
    .path    = ".",
    .address = "::1",
    .port    = 8080,
};

typedef struct file_entry {
    const char* path;
    int64_t size;
} file_entry_t;


static void about( int argc, char* argv[] );
static void on_connection( server_t* server, int peer_socket, struct sockaddr_storage* peer_address, void* user_data );
static void process_directory_content( const char* path, void* args );
static void url_decode( char *s );


static void signal_interrupt_handler( int dummy )
{
    printf( "\n" );
    printf( "Stopping server...\n" );
    server_stop( app_state.server );
}

static void signal_quit_handler( int dummy )
{
    printf( "\n" );
    printf( "Stopping server...\n" );
    server_stop( app_state.server );
}

int main( int argc, char* argv[] )
{
    signal( SIGINT, signal_interrupt_handler );
    signal( SIGQUIT, signal_quit_handler );

    if( argc < 2 )
    {
        about( argc, argv );
        return -1;
    }
    else
    {
        for( int arg = 1; arg < argc; arg++ )
        {
            if( strcmp( "-v", argv[arg] ) == 0 || strcmp( "--verbose", argv[arg] ) == 0 )
            {
                app_state.verbose = true;
            }
            else if( strcmp( "-a", argv[arg] ) == 0 || strcmp( "--address", argv[arg] ) == 0 )
            {
                app_state.address = argv[ arg + 1 ];
                arg++;
            }
            else if( strcmp( "-p", argv[arg] ) == 0 || strcmp( "--port", argv[arg] ) == 0 )
            {
                app_state.port = atoi(argv[ arg + 1 ] );
                arg++;
            }
            else if( strcmp( "-t", argv[arg] ) == 0 || strcmp( "--title", argv[arg] ) == 0 )
            {
                app_state.title = argv[ arg + 1 ];
                arg++;
            }
            else if( argv[arg][0] != '-' )
            {
                app_state.path = argv[arg];
            }
            else
            {
                fprintf( stderr, "ERROR: Unrecognized command line option '%s'\n", argv[arg] );
                about( argc, argv );
                return -2;
            }
        }
    }

    app_state.server = server_create( CONNECTION_QUEUE, NULL );

    if( !server_start( app_state.server, app_state.address, app_state.port ) )
    {
        return -3;
    }

    server_run( app_state.server, on_connection );
    server_destroy( &app_state.server );

    return 0;
}

void on_connection( server_t* server, int peer_socket, struct sockaddr_storage* peer_address, void* user_data )
{
    char request_buffer[ 512 ] = { '\0' };

    //recv( peer_socket, request_buffer, sizeof(request_buffer), 0 );

    FILE* request_stream = fdopen( peer_socket, "rb" );
    char* requested_file = NULL;

    if( request_stream )
    {
        setvbuf( request_stream, NULL, _IONBF, 0 );
#if 1
        readline( request_buffer, sizeof(request_buffer), request_stream );

        requested_file = strchr( request_buffer, ' ' ) + 1;
        char* requested_file_end = strrchr( request_buffer, ' ' );

        if( requested_file && requested_file_end )
        {
            *requested_file_end = '\0';
            url_decode( requested_file );
            printf( "File: %s\n", requested_file );
        }
#else
        char* request_line = NULL;
        size_t request_line_size = 0;
        getline( &request_line, &request_line_size, request_stream );
        printf( "Request: %s\n", request_line );
#endif
    }

    if( !requested_file )
    {
        return;
    }

    memmove( requested_file, requested_file + 1, strlen(requested_file + 1) + 1 );


    if( strcmp(requested_file, "favicon.ico" ) == 0 )
    {
        return;
    }

    char absolute_path[ 256 ];

    if( *requested_file == '\0' )
    {
        snprintf( absolute_path, sizeof(absolute_path), "%s", app_state.path );
    }
    else
    {
        snprintf( absolute_path, sizeof(absolute_path), "%s/%s", app_state.path, requested_file );
    }
    absolute_path[ sizeof(absolute_path) - 1 ] = '\0';

    printf( "Path: %s\n", absolute_path );

    if( is_directory( absolute_path ) )
    {
        textbuffer_t body_buffer;

        textbuffer_create( &body_buffer );
        textbuffer_printf( &body_buffer, "<!DOCTYPE html>\n" );
        textbuffer_printf( &body_buffer, "<html>\n" );
        textbuffer_printf( &body_buffer, "<header>\n" );
        textbuffer_printf( &body_buffer, "    <title> %s </title>\n", app_state.title );
        textbuffer_printf( &body_buffer, "    <link rel='stylesheet' href='http://yui.yahooapis.com/pure/0.6.0/pure-min.css'>\n" );
        textbuffer_printf( &body_buffer, "    <style>\n" );
        textbuffer_printf( &body_buffer, "    body {\n" );
        textbuffer_printf( &body_buffer, "        background: #ececec;\n" );
        textbuffer_printf( &body_buffer, "        color: #333;\n" );
        textbuffer_printf( &body_buffer, "        font-family: Arial, Helvetica, sans-serif;\n" );
        textbuffer_printf( &body_buffer, "    }\n" );
        textbuffer_printf( &body_buffer, "     {\n" );
        textbuffer_printf( &body_buffer, "        width: 800px;\n" );
        textbuffer_printf( &body_buffer, "    }\n" );
        textbuffer_printf( &body_buffer, "    .content {\n" );
        textbuffer_printf( &body_buffer, "        background: #ececec;\n" );
        textbuffer_printf( &body_buffer, "        color: #333;\n" );
        textbuffer_printf( &body_buffer, "        margin: auto;\n" );
        textbuffer_printf( &body_buffer, "        /* width: 1024px;*/\n" );
        textbuffer_printf( &body_buffer, "        /* border: 1px solid #333;*/\n" );
        textbuffer_printf( &body_buffer, "        padding: 10px;\n" );
        textbuffer_printf( &body_buffer, "    }\n" );
        textbuffer_printf( &body_buffer, "    .small {\n" );
        textbuffer_printf( &body_buffer, "        font-size: 0.7em;\n" );
        textbuffer_printf( &body_buffer, "    }\n" );
        textbuffer_printf( &body_buffer, "    </style>\n" );
        textbuffer_printf( &body_buffer, "</header>\n" );
        textbuffer_printf( &body_buffer, "<body>\n" );
        textbuffer_printf( &body_buffer, "<div class='content'>\n" );
        textbuffer_printf( &body_buffer, "    <h1> %s </h1>\n", app_state.title );
        textbuffer_printf( &body_buffer, "    <p><a href='/' title='Return to the parent directory'> Parent Directory </a></p>\n" );

        file_entry_t* files = NULL;
        vector_create( files, 1 );

        printf( "%s is a dir\n", absolute_path );
        directory_enumerate( absolute_path, false, ENUMERATE_ALL, process_directory_content, &files );

        if( vector_size(files) > 0 )
        {
            textbuffer_printf( &body_buffer, "    <table class='pure-table pure-table-horizontal'>\n" );
            textbuffer_printf( &body_buffer, "         <tr><thead><th>Filename</th><th>Size</th></tr></thead><tbody>\n" );
            char download_path[ MAX_PATH ];

            for( int i = 0; i < vector_size(files); i++ )
            {
                const file_entry_t* entry = &files[i];
                const char* base_name     = file_basename( entry->path );
                const char* file_size_str = size_in_best_unit( entry->size, true, 2 );

                if( *absolute_path != '\0' )
                {
                    snprintf( download_path, sizeof(download_path), "%s/%s", absolute_path, base_name );
                }
                else
                {
                    snprintf( download_path, sizeof(download_path), "%s", base_name );
                }
                download_path[ sizeof(download_path) - 1 ] = '\0';

                textbuffer_printf( &body_buffer, "        <tr><td><a href='%s' title='Download %s'>%s</a></td><td>%s</td></tr>\n", download_path, base_name, base_name, file_size_str );
            }
            textbuffer_printf( &body_buffer, "    </tbody></table>\n" );
        }
        else
        {
            textbuffer_printf( &body_buffer, "    <p>No files in this path.</p>\n" );
        }

        vector_destroy( files );

        textbuffer_printf( &body_buffer, "<p class='small'>Coded by Joe Marrero. <a href='http://www.manvscode.com/'>http://www.manvscode.com/</a></p>\n" );
        textbuffer_printf( &body_buffer, "</div>\n" );

        textbuffer_printf( &body_buffer, "</body>\n" );
        textbuffer_printf( &body_buffer, "</html>\n" );

        int content_len = body_buffer.count;

        textbuffer_t headers_buffer;

        textbuffer_create( &headers_buffer );
        textbuffer_printf( &headers_buffer, "HTTP/1.1 200 OK\r\n" );
        textbuffer_printf( &headers_buffer, "Content-Type: text/html\r\n" );
        textbuffer_printf( &headers_buffer, "Content-Length: %d\r\n", content_len );
        textbuffer_printf( &headers_buffer, "Cache-Control: no-cache, no-store, must-revalidate\r\n" );
        textbuffer_printf( &headers_buffer, "Pragma: no-cache\r\n" );
        textbuffer_printf( &headers_buffer, "Expires: 0\r\n" );
        textbuffer_printf( &headers_buffer, "\r\n" );

        send( peer_socket, buffer_data(headers_buffer.buffer), headers_buffer.count, 0 );
        send( peer_socket, buffer_data(body_buffer.buffer), body_buffer.count, 0 );

        textbuffer_destroy( &body_buffer );
        textbuffer_destroy( &headers_buffer );
    }
    else if( file_exists(absolute_path) )
    {
        printf( "%s is a file\n", absolute_path );
        int content_len = file_size( absolute_path );

        textbuffer_t headers_buffer;
        textbuffer_create( &headers_buffer );

        textbuffer_printf( &headers_buffer, "HTTP/1.1 200 OK\r\n" );
        textbuffer_printf( &headers_buffer, "Content-Type: application/octet-stream" );
        textbuffer_printf( &headers_buffer, "Content-Disposition: attachment; filename=\"picture.png\"" );
        textbuffer_printf( &headers_buffer, "Content-Length: %d\r\n", content_len );
        textbuffer_printf( &headers_buffer, "Cache-Control: no-cache, no-store, must-revalidate\r\n" );
        textbuffer_printf( &headers_buffer, "Pragma: no-cache\r\n" );
        textbuffer_printf( &headers_buffer, "Expires: 0\r\n" );
        textbuffer_printf( &headers_buffer, "\r\n" );

        send( peer_socket, buffer_data(headers_buffer.buffer), headers_buffer.count, 0 );

        FILE* file = fopen( absolute_path, "rb" );

        if( file )
        {
            unsigned char buffer[ 4096 ];
            size_t bytes_sent = 0;

            while( !feof(file) && content_len > 0 )
            {
                size_t bytes_read = fread( buffer, sizeof(char), sizeof(buffer), file );
                ssize_t bytes_sent = 0;

                while( bytes_read > 0 )
                {
                    bytes_sent = send( peer_socket, buffer + bytes_sent, bytes_read, 0 );
                    bytes_read -= bytes_sent;
                    content_len -= bytes_sent;
                }
            }

            fclose( file );
        }

        textbuffer_destroy( &headers_buffer );
    }
}

void process_directory_content( const char* path, void* args )
{
    file_entry_t** entries = (file_entry_t**) args;
    char* path_copy = strdup( path );

    file_entry_t entry = {
        .path = path_copy,
        .size = file_size(path_copy)
    };
    vector_push( *entries, entry );
}

/* Convert an ASCII hex digit to the corresponding number between 0
   and 15.  H should be a hexadecimal digit that satisfies isxdigit;
   otherwise, the result is undefined.  */
#define XDIGIT_TO_NUM(h) ((h) < 'A' ? (h) - '0' : toupper (h) - 'A' + 10)
#define X2DIGITS_TO_NUM(h1, h2) ((XDIGIT_TO_NUM (h1) << 4) + XDIGIT_TO_NUM (h2))

void url_decode( char *s )
{
    char *t = s;                  /* t - tortoise */
    char *h = s;                  /* h - hare     */

    for (; *h; h++, t++)
    {
        if (*h == '+' )
        {
            *t = ' ';
        }
        else if (*h != '%')
        {
copychar:
            *t = *h;
        }
        else
        {
            char c;

            /* Do nothing if '%' is not followed by two hex digits. */
            if (!h[1] || !h[2] || !(isxdigit (h[1]) && isxdigit (h[2])))
                goto copychar;
            c = X2DIGITS_TO_NUM (h[1], h[2]);
            /* Don't unescape %00 because there is no way to insert it
               into a C string without effectively truncating it. */
            if (c == '\0')
                goto copychar;
            *t = c;
            h += 2;
        }
    }
    *t = '\0';
}

void about( int argc, char* argv[] )
{
    printf( "Host This v%s\n", VERSION );
    printf( "Copyright (c) 2016, Joe Marrero.\n");

    printf( "\n" );

    printf( "This is a tool to start a web server to share files on a network.\n" );

    printf( "\n" );

    printf( "Usage:\n" );
    printf( "    %s <args> [<directory>]\n", argv[0] );

    printf( "\n" );

    printf( "Command Line Options:\n" );
    printf( "    %-2s, %-12s  %-50s\n", "-v", "--verbose", "Toggles verbose mode." );
    printf( "    %-2s, %-12s  %-50s\n", "-a", "--address", "Sets the address that the web server listens on (default is ::1)." );
    printf( "    %-2s, %-12s  %-50s\n", "-p", "--port", "Sets the port that the web server listens on (default is 8080)." );
    printf( "    %-2s, %-12s  %-50s\n", "-t", "--title", "Sets the title on the web server." );

    printf( "\n" );
}


