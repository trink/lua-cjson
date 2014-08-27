/* strbuf - String buffer routines
 *
 * Copyright (c) 2010-2012  Mark Pulford <mark@kyne.com.au>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <lauxlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "strbuf.h"

#ifdef _WIN32
#define snprintf _snprintf
#endif

void strbuf_init(strbuf_t *s, int len, lua_State *l, int max_size)
{
    int size;

    if (len <= 0)
        if (max_size > 0 && max_size < STRBUF_DEFAULT_SIZE) {
            size = max_size;
        } else {
            size = STRBUF_DEFAULT_SIZE;
        }
    else
        size = len + 1;         /* \0 terminator */

    s->buf = NULL;
    s->size = size;
    s->length = 0;
    s->increment = STRBUF_DEFAULT_INCREMENT;
    s->dynamic = 0;
    s->reallocs = 0;
    s->debug = 0;
    s->max_size = max_size;
    s->lua = l;

    s->buf = malloc(size);
    if (!s->buf)
        luaL_error(l, "strbuf_init out of memory");

    strbuf_ensure_null(s);
}

strbuf_t *strbuf_new(int len, lua_State *l)
{
    strbuf_t *s;

    s = malloc(sizeof(strbuf_t));
    if (!s)
        luaL_error(l, "strbuf_new out of memory");

    strbuf_init(s, len, l, 0);

    /* Dynamic strbuf allocation / deallocation */
    s->dynamic = 1;

    return s;
}

void strbuf_set_increment(strbuf_t *s, int increment)
{
    /* Increment > 0:  Linear buffer growth rate
     * Increment < -1: Exponential buffer growth rate */
    if (increment == 0 || increment == -1)
        luaL_error(s->lua, "BUG: Invalid string increment");

    s->increment = increment;
}

static inline void debug_stats(strbuf_t *s)
{
    if (s->debug) {
        fprintf(stderr, "strbuf(%lx) reallocs: %d, length: %d, size: %d\n",
                (long)s, s->reallocs, s->length, s->size);
    }
}

/* If strbuf_t has not been dynamically allocated, strbuf_free() can
 * be called any number of times strbuf_init() */
void strbuf_free(strbuf_t *s)
{
    debug_stats(s);

    if (s->buf) {
        free(s->buf);
        s->buf = NULL;
    }
    if (s->dynamic)
        free(s);
}

char *strbuf_free_to_string(strbuf_t *s, int *len)
{
    char *buf;

    debug_stats(s);

    strbuf_ensure_null(s);

    buf = s->buf;
    if (len)
        *len = s->length;

    if (s->dynamic)
        free(s);

    return buf;
}

static int calculate_new_size(strbuf_t *s, int len)
{
    int reqsize, newsize;

    if (len <= 0)
        luaL_error(s->lua, "BUG: Invalid strbuf length requested");

    /* Ensure there is room for optional NULL termination */
    reqsize = len + 1;

    /* If the user has requested to shrink the buffer, do it exactly */
    if (s->size > reqsize)
        return reqsize;

    if (s->max_size > 0 && reqsize > s->max_size) {
        luaL_error(s->lua, "strbuf max_size exceeded");
    }
    newsize = s->size;
    if (s->increment < 0) {
        /* Exponential sizing */
        while (newsize < reqsize)
            newsize *= -s->increment;
    } else {
        /* Linear sizing */
        newsize = ((newsize + s->increment - 1) / s->increment) * s->increment;
    }
    if (s->max_size > 0 && newsize > s->max_size) {
        newsize = s->max_size;
    }
    return newsize;
}


/* Ensure strbuf can handle a string length bytes long (ignoring NULL
 * optional termination). */
void strbuf_resize(strbuf_t *s, int len)
{
    int newsize;

    newsize = calculate_new_size(s, len);

    if (s->debug > 1) {
        fprintf(stderr, "strbuf(%lx) resize: %d => %d\n",
                (long)s, s->size, newsize);
    }

    s->size = newsize;
    char *b = realloc(s->buf, s->size);
    if (!b) {
        // the original memory is still intact leave it untouched
        // incase the caller recovers and trys to reuse it
        luaL_error(s->lua, "strbuf_resize out of memory");
    } else {
        s->buf = b;
    }
    s->reallocs++;
}

void strbuf_append_string(strbuf_t *s, const char *str)
{
    int space, i;

    space = strbuf_empty_length(s);

    for (i = 0; str[i]; i++) {
        if (space < 1) {
            strbuf_resize(s, s->length + 1);
            space = strbuf_empty_length(s);
        }

        s->buf[s->length] = str[i];
        s->length++;
        space--;
    }
}

/* strbuf_append_fmt() should only be used when an upper bound
 * is known for the output string. */
void strbuf_append_fmt(strbuf_t *s, int len, const char *fmt, ...)
{
    va_list arg;
    int fmt_len;

    strbuf_ensure_empty_length(s, len);

    va_start(arg, fmt);
    fmt_len = vsnprintf(s->buf + s->length, len, fmt, arg);
    va_end(arg);

    if (fmt_len < 0)
        luaL_error(s->lua, "BUG: Unable to convert number");  /* This should never happen.. */

    s->length += fmt_len;
}

/* strbuf_append_fmt_retry() can be used when the there is no known
 * upper bound for the output string. */
void strbuf_append_fmt_retry(strbuf_t *s, const char *fmt, ...)
{
    va_list arg;
    int fmt_len, try;
    int empty_len;

    /* If the first attempt to append fails, resize the buffer appropriately
     * and try again */
    for (try = 0; ; try++) {
        va_start(arg, fmt);
        /* Append the new formatted string */
        /* fmt_len is the length of the string required, excluding the
         * trailing NULL */
        empty_len = strbuf_empty_length(s);
        /* Add 1 since there is also space to store the terminating NULL. */
        fmt_len = vsnprintf(s->buf + s->length, empty_len + 1, fmt, arg);
        va_end(arg);

        if (fmt_len <= empty_len)
            break;  /* SUCCESS */
        if (try > 0)
            luaL_error(s->lua, "BUG: length of formatted string changed");

        strbuf_resize(s, s->length + fmt_len);
    }

    s->length += fmt_len;
}

/* vi:ai et sw=4 ts=4:
 */
