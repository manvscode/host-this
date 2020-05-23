#ifndef __TEXTBUFFER_H__
#define __TEXTBUFFER_H__

#include <collections/buffer.h>

typedef struct textbuffer {
	lc_buffer_t* buffer;
	size_t count; /* characters written */
} textbuffer_t;

void textbuffer_create( textbuffer_t* textbuffer );
void textbuffer_destroy( textbuffer_t* textbuffer );
bool textbuffer_printf( textbuffer_t *p_buffer, const char *format, ... );
bool textbuffer_vprintf( textbuffer_t* p_buffer, const char *format, va_list ap );
#endif /* __TEXTBUFFER_H__ */
