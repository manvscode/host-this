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
#include <stdbool.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#define VECTOR_GROW_AMOUNT(array)      (10)
#include <collections/buffer.h>
#include <collections/vector.h>
#include <xtd/console.h>
#include <xtd/filesystem.h>
#include <xtd/memory.h>
#include <xtd/string.h>
#include "server.h"
#include "textbuffer.h"

#define CONNECTION_QUEUE 10
#ifndef MAX_PATH
#define MAX_PATH   256
#endif

#define VERSION "1.0"

struct {
	server_t* server;
	bool verbose;
	const char* title;
	const char* path;
	bool use_ip4;
	short port;
} app_state = {
	.server  = NULL,
	.verbose = false,
	.title   = "Hosting Files",
	.path    = ".",
	.use_ip4 = false,
	.port    = 8080,
};

typedef struct file_entry {
	const char* path;
	int64_t size;
} file_entry_t;

typedef struct {
	FILE* file;
	int socket;
	int64_t file_size;
	unsigned char* buf;
	size_t buf_size;
	ssize_t bytes_remaining;
} send_file_task_args_t;


static void about( int argc, char* argv[] );
static void on_connection( server_t* server, int peer_socket, struct sockaddr_storage* peer_address, void* user_data );
static void process_directory_content( const char* path, void* args );
static bool send_file_task( int* percent, void* data );
static char* get_requested_file( int peer_socket, char* buffer, size_t buffer_sz );
static void print_verbose_prefix(const char* peer_address_str);
static void print_verbosef(const char* peer_address_str, const char* format, ...);
static void url_decode( char *s );

static void quit(void)
{
	if (server_is_running(app_state.server) )
	{
		console_reset(stdout);
		printf("\n");
		console_text_faderf(stdout, TEXT_FADER_TO_ORANGE, "Stopping server...");
		printf("\n");
		server_stop(app_state.server);
	}
}


static void signal_interrupt_handler( int dummy )
{
	quit();
}

static void signal_quit_handler( int dummy )
{
	quit();
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
			else if( strcmp( "-4", argv[arg] ) == 0 || strcmp( "--ip4", argv[arg] ) == 0 )
			{
				app_state.use_ip4 = true;
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
			else if( strcmp( "-h", argv[arg] ) == 0 || strcmp( "--help", argv[arg] ) == 0 )
			{
				about( argc, argv );
				return 0;
			}
			else
			{
				fprintf( stderr, "ERROR: Unrecognized command line option '%s'\n", argv[arg] );
				about( argc, argv );
				return -2;
			}
		}
	}

	console_hide_cursor(stdout);

	console_fg_color_256(stdout, CONSOLE_COLOR256_BRIGHT_YELLOW);
	printf("  _    _           _      \n");
	printf(" | |  | |         | |     \n");
	printf(" | |__| | ___  ___| |_    \n");
	printf(" |  __  |/ _ \\/ __| __|   \n");
	printf(" | |  | | (_) \\__ \\ |_    \n");
	printf(" |_|  |_|\\___/|___/\\__|   \n");
	printf("     |__   __| |   (_)    \n");
	printf("        | |  | |__  _ ___ \n");
	printf("        | |  | '_ \\| / __|\n");
	printf("        | |  | | | | \\__ \\\n");
	printf("        |_|  |_| |_|_|___/\n");
	printf("\n");
	console_reset(stdout);

	console_fg_color_256(stdout, CONSOLE_COLOR256_GREY_17);
	printf("Serving files from ");
	console_reset(stdout);

	console_fg_color_256(stdout, CONSOLE_COLOR256_BRIGHT_CYAN);
	printf("\"%s\"\n", app_state.path);
	console_reset(stdout);

	/*
	 * We show all of the possible addresses that
	 * can be used to connect.
	 */
	console_fg_color_256(stdout, CONSOLE_COLOR256_GREY_17);
	printf("Connect at:\n");
	console_reset(stdout);

	struct ifaddrs* interfaces = NULL;
	if(!getifaddrs(&interfaces))
	{
		for( struct ifaddrs* interface = interfaces;
			 interface != NULL;
			 interface = interface->ifa_next )
		{
			if( strcmp(interface->ifa_name, "lo") == 0 ) continue; // skip loopback address.
			struct sockaddr* address = interface->ifa_addr;

			int address_family = app_state.use_ip4 ? AF_INET : AF_INET6;

			if( address && address->sa_family == address_family )
			{
				void* local_ip = NULL;
				if( address_family == AF_INET)
				{
					local_ip = &((struct sockaddr_in*) address)->sin_addr;
				}
				else
				{
					local_ip = &((struct sockaddr_in6*) address)->sin6_addr;
				}

				char address_string[128];
				if( !inet_ntop(address_family, local_ip, address_string, sizeof(address_string)))
				{
					console_reset(stdout);
					perror("ERROR");
					return -3;
				}

				console_fg_color_256(stdout, CONSOLE_COLOR256_GREY_17);
				printf(" * ");

				console_fg_color_256(stdout, CONSOLE_COLOR256_BRIGHT_CYAN);
				printf( app_state.use_ip4 ? "http://%s:%d/\n" : "http://[%s]:%d/\n", address_string, app_state.port);
				console_reset(stdout);
			}
		}

		freeifaddrs(interfaces);
	}
	printf("\n");

	app_state.server = server_create( app_state.use_ip4, CONNECTION_QUEUE, NULL );

	/*
	 * Start server and bind to the address passed in
	 * from the command line or bind to all interfaces.
	 */
	if( !server_start( app_state.server, NULL, app_state.port ) )
	{
		return -3;
	}

	server_run( app_state.server, on_connection );
	server_destroy( &app_state.server );

	console_show_cursor(stdout);

	return 0;
}
void print_verbose_prefix(const char* peer_address_str)
{
	const size_t peer_address_hash = string_hash(peer_address_str);
	const int colors[] = {
		CONSOLE_COLOR256_BRIGHT_YELLOW,
		CONSOLE_COLOR256_BRIGHT_RED,
		CONSOLE_COLOR256_BRIGHT_CYAN,
		CONSOLE_COLOR256_BRIGHT_MAGENTA,
		CONSOLE_COLOR256_BRIGHT_GREEN,
		CONSOLE_COLOR256_BRIGHT_BLUE,
	};
	const size_t colors_count = sizeof(colors) / sizeof(colors[0]);
	int len = strlen(peer_address_str);

	console_fg_color_256( stdout, CONSOLE_COLOR256_GREY_11);
	printf("[");

	console_fg_color_256(stdout, colors[peer_address_hash % colors_count]);
	printf("%-.*s", len, peer_address_str);

	console_fg_color_256( stdout, CONSOLE_COLOR256_GREY_11);
	printf("] ");

	console_set_column(stdout, len + 3 + 1);
}

void print_verbosef(const char* peer_address_str, const char* format, ...)
{
	print_verbose_prefix(peer_address_str);

	char fmtbuf[ 256 ];
	va_list args;
	va_start( args, format );
	vsnprintf( fmtbuf, sizeof(fmtbuf), format, args );
	fmtbuf[ sizeof(fmtbuf) - 1 ] = '\0';
	va_end( args );

	console_text_fader( stdout, TEXT_FADER_TO_WHITE, fmtbuf );
	console_reset(stdout);
}

char* get_requested_file( int peer_socket, char* buffer, size_t buffer_sz )
{
	char* requested_file = NULL;
	// Stream is closed by the server so we don't need to close this.
	FILE* request_stream = fdopen( peer_socket, "rb" );

	if( request_stream )
	{
		setvbuf( request_stream, NULL, _IONBF, 0 );
#if 1
		file_readline( request_stream, buffer, buffer_sz );

		char* position_of_space = strchr( buffer, ' ' );

		if (!position_of_space)
		{
			// peer closed the connection.
			return NULL;
		}

		requested_file = position_of_space + 1;
		char* requested_file_end = strrchr( buffer, ' ' );

		if( requested_file && requested_file_end )
		{
			*requested_file_end = '\0';
			url_decode( requested_file );
			//printf( "File: %s\n", requested_file );
		}
#else
		char* request_line = NULL;
		size_t request_line_size = 0;
		getline( &request_line, &request_line_size, request_stream );
		printf( "Request: %s\n", request_line );
#endif
	}

	return requested_file;
}

static char* get_peer_address(char* buffer, size_t sz, struct sockaddr_storage* peer_address)
{
	inet_ntop(peer_address->ss_family, peer_address, buffer, sz);
	buffer[ sz - 1 ] = '\0';
	return buffer;
}

void on_connection( server_t* server, int peer_socket, struct sockaddr_storage* peer_address, void* user_data )
{
	if( server_socket(server) <= 0 )
	{
		return;
	}

	char peer_address_str[ 46 ];
	get_peer_address(peer_address_str, sizeof(peer_address_str), peer_address);

	if( app_state.verbose )
	{
		print_verbosef(peer_address_str, "Accepted connection.");
		printf("\n");
	}

	char request_buffer[ 512 ] = { '\0' };
	char* requested_file = get_requested_file( peer_socket, request_buffer, sizeof(request_buffer) );

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

	if( is_directory( absolute_path ) )
	{
		if( app_state.verbose )
		{
			print_verbosef(peer_address_str, "Sending directory contents for \"%s\"", absolute_path );
			printf("\n");
		}

		textbuffer_t body_buffer;

		textbuffer_create( &body_buffer );
		textbuffer_printf( &body_buffer, "<!DOCTYPE html>\n" );
		textbuffer_printf( &body_buffer, "<html>\n" );
		textbuffer_printf( &body_buffer, "<header>\n" );
		textbuffer_printf( &body_buffer, "    <title> %s </title>\n", app_state.title );
		textbuffer_printf( &body_buffer, "    <link rel='stylesheet' href='http://yui.yahooapis.com/pure/0.6.0/pure-min.css'>\n" );
		textbuffer_printf( &body_buffer, "    <style>\n" );
		textbuffer_printf( &body_buffer, "    body {\n" );
		textbuffer_printf( &body_buffer, "        background: #ffffff;\n" );
		textbuffer_printf( &body_buffer, "        color: #333;\n" );
		textbuffer_printf( &body_buffer, "        font-family: Arial, Helvetica, sans-serif;\n" );
		textbuffer_printf( &body_buffer, "    }\n" );
		textbuffer_printf( &body_buffer, "     {\n" );
		textbuffer_printf( &body_buffer, "        width: 800px;\n" );
		textbuffer_printf( &body_buffer, "    }\n" );
		textbuffer_printf( &body_buffer, "    .content {\n" );
		textbuffer_printf( &body_buffer, "        background: #ffffff;\n" );
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
		lc_vector_create( files, 1 );

		directory_enumerate( absolute_path, false, ENUMERATE_ALL, process_directory_content, &files );

		if( lc_vector_size(files) > 0 )
		{
			textbuffer_printf( &body_buffer, "    <table class='pure-table pure-table-horizontal'>\n" );
			textbuffer_printf( &body_buffer, "         <tr><thead><th>Filename</th><th>Size</th></tr></thead><tbody>\n" );
			char download_path[ MAX_PATH ];

			for( int i = 0; i < lc_vector_size(files); i++ )
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

		lc_vector_destroy( files );

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

		send( peer_socket, lc_buffer_data(headers_buffer.buffer), headers_buffer.count, 0 );
		send( peer_socket, lc_buffer_data(body_buffer.buffer), body_buffer.count, 0 );

		textbuffer_destroy( &body_buffer );
		textbuffer_destroy( &headers_buffer );
	}
	else if( file_exists(absolute_path) )
	{
		if( app_state.verbose )
		{
			print_verbosef(peer_address_str, "Requested file \"%s\" ", absolute_path );
			printf("\n");
		}

		int64_t content_len = file_size( absolute_path );
		const char* filename = file_basename( absolute_path );

		textbuffer_t headers_buffer;
		textbuffer_create( &headers_buffer );

		textbuffer_printf( &headers_buffer, "HTTP/1.1 200 OK\r\n" );
		textbuffer_printf( &headers_buffer, "Content-Type: application/octet-stream\r\n" );
		textbuffer_printf( &headers_buffer, "Content-Disposition: attachment; filename=\"%s\"\r\n", filename );
		textbuffer_printf( &headers_buffer, "Content-Length: %ld\r\n", content_len );
		textbuffer_printf( &headers_buffer, "Cache-Control: no-cache, no-store, must-revalidate\r\n" );
		textbuffer_printf( &headers_buffer, "Pragma: no-cache\r\n" );
		textbuffer_printf( &headers_buffer, "Expires: 0\r\n" );
		textbuffer_printf( &headers_buffer, "\r\n" );

		send( peer_socket, lc_buffer_data(headers_buffer.buffer), headers_buffer.count, 0 );

		textbuffer_destroy( &headers_buffer );

		FILE* file = fopen( absolute_path, "rb" );
		if( file )
		{
			unsigned char buffer[ 4096 ];
			send_file_task_args_t args = {
				.file = file,
				.file_size = content_len,
				.socket = peer_socket,
				.buf = buffer,
				.buf_size = sizeof(buffer),
				.bytes_remaining = content_len,
			};

			print_verbose_prefix(peer_address_str);

			char description[512];
			snprintf(description, sizeof(description), "Sending \"%s\"", absolute_path);
			description[ sizeof(description) - 1 ] = '\0';
			console_progress_indicator( stdout, description, PROGRESS_INDICATOR_STYLE_BLUE, send_file_task, &args );

			fclose( file );
		}

	}

	if( app_state.verbose )
	{
		print_verbosef(peer_address_str, "Closing connection." );
		printf("\n");
	}
}

void process_directory_content( const char* path, void* args )
{
	file_entry_t** entries = (file_entry_t**) args;
	char* path_copy = string_dup( path );

	file_entry_t entry = {
		.path = path_copy,
		.size = file_size(path_copy)
	};
	lc_vector_push( *entries, entry );
}

bool send_file_task( int* percent, void* data )
{
	send_file_task_args_t* args = (send_file_task_args_t*) data;
	bool isSending = !feof(args->file) && args->bytes_remaining > 0;

	if( isSending )
	{
		size_t bytes_read = fread( args->buf, sizeof(char), args->buf_size, args->file );
		ssize_t bytes_sent = 0;

		while( bytes_read > 0 )
		{
			bytes_sent = send( args->socket, args->buf + bytes_sent, bytes_read, 0 );
			bytes_read -= bytes_sent;
			args->bytes_remaining -= bytes_sent;
		}
	}

	*percent = 100 * (args->file_size - args->bytes_remaining) / args->file_size;

	fflush(stdout);

	return isSending;
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
	printf( "    %-2s, %-12s  %-50s\n", "-4", "--ip4", "Toggles IPv4 mode." );
	printf( "    %-2s, %-12s  %-50s\n", "-p", "--port", "Sets the port that the web server listens on (default is 8080)." );
	printf( "    %-2s, %-12s  %-50s\n", "-t", "--title", "Sets the title on the web server." );

	printf( "\n" );
}


