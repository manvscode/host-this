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
