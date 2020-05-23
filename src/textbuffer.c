#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <collections/buffer.h>
#ifdef __linux__
# include <sys/types.h>
#endif
#include "textbuffer.h"


void textbuffer_create( textbuffer_t* textbuffer )
{
	textbuffer->buffer = lc_buffer_create( 512, false );
	textbuffer->count  = 0;
}

void textbuffer_destroy( textbuffer_t* textbuffer )
{
	if( textbuffer )
	{
		lc_buffer_destroy( &textbuffer->buffer );
	}
}

bool textbuffer_printf( textbuffer_t *p_buffer, const char *format, ... )
{
	bool result = false;
	va_list args;

	va_start( args, format );
	result = textbuffer_vprintf( p_buffer, format, args );
	va_end( args );

	return result;
}

bool textbuffer_vprintf( textbuffer_t* p_buffer, const char *format, va_list args )
{
	bool result = false;
	va_list args_copy;
	va_copy(args_copy, args);

	const int GROWTH = 64;

retry:
	{
		ssize_t size_remaining = lc_buffer_size(p_buffer->buffer) - p_buffer->count;
		char* text = (char*) lc_buffer_data(p_buffer->buffer) + p_buffer->count;
		ssize_t ret = vsnprintf( text, size_remaining, format, args );
		text[ size_remaining - 1 ] = '\0';

		if( ret >= size_remaining )
		{
			// buffer is too small
			if( lc_buffer_resize( &p_buffer->buffer, lc_buffer_size(p_buffer->buffer) + ret + GROWTH + 1 ) )
			{
				va_end( args );
				va_copy( args, args_copy );
				goto retry;
			}
			else
			{
				fprintf( stderr, "ERROR: Unable to resize buffer.\n" );
				result = false;
			}
		}
		else if( ret > 0 )
		{
			p_buffer->count += ret;
		}
	}
	va_end( args_copy );

	return result;
}
