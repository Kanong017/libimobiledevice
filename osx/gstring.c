//	GString.c
//
//	G-lib replacement for OS X
//
//	Created by Aaron Burghardt 7/25/2013.
//	Copyright 2013 Aaron Burghardt. All rights reserved.

//	g_string_append_c(GString *string, char c)
// g_string_new(app_path);
// g_string_free(payload, TRUE);
// g_string_prepend(packet, ",");
// g_string_append_len(in_queue, buf, received);
// g_string_erase(in_queue, 0, in_queue->str - pkg_end);

//#include <CoreFoundation/CoreFoundation.h>
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "glib.h"

// typedef struct __CFString GString;

GString * g_string_new(const gchar *init)
{
	assert(init != NULL);

	GString *s = malloc(sizeof(GString));

	s->len = strlen(init);
	s->str = malloc(s->len + 1);
	s->allocated_len = s->len + 1;

	memcpy(s->str, init, s->len);
	s->str[s ->len] = 0;

	// we will manage the str buffer
//	CFStringRef cf_string = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, (UInt8 *) init, strlen(init), kCFStringEncodingUTF8, true, kCFAllocatorNull);
//	s->cf_string = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, cf_string);
//	CFRelease(cf_string);

	return s;
}

GString * g_string_new_len(const gchar *init, gssize len)
{
	GString *s = malloc(sizeof(GString));
	s->len = len;
	s->str = malloc(len + 1);
	s->allocated_len = len + 1;

	memcpy(s->str, init, len);
	s->str[s ->len] = 0;

//	CFStringRef string = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, (UInt8 *) init, len, kCFStringEncodingUTF8, true, kCFAllocatorNull);
//	s->cf_string = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, string);
//	CFRelease(string);

	return s;
}

GString * g_string_sized_new(gsize dfl_size)
{
	assert(FALSE);
}

GString * g_string_assign(GString *string, const gchar *rval)
{
	assert(FALSE);
}

#define	g_string_sprintf
#define	g_string_sprintfa

void g_string_vprintf(GString *string, const gchar *format, va_list args)
{
	assert(FALSE);
}

void g_string_append_vprintf(GString *string, const gchar *format, va_list args)
{
	assert(FALSE);
}

void g_string_printf(GString *string, const gchar *format, ...)
{
	assert(FALSE);
}

void g_string_append_printf(GString *string, const gchar *format, ...)
{
	assert(FALSE);
}

GString * g_string_append(GString *string, const gchar *val)
{
	assert(FALSE);
}

GString * g_string_append_c(GString *string, gchar c)
{
	if ((string->len + 2) > string->allocated_len) {
		string->str = realloc(string->str, string->allocated_len + 2);
		string->allocated_len += 2;
	}
	string->str[string->len] = c;
	string->str[string->len + 1] = 0;

//	CFStringAppendCString(string->cf_string, string->str + string->len, kCFStringEncodingUTF8);
	string->len += 1;

	return string;
}

GString * g_string_append_unichar(GString *string, gunichar wc)
{
	assert(FALSE);
}

GString * g_string_append_len(GString *string, const gchar *val, gssize len)
{
	if ((string->len + len) > string->allocated_len) {
		string->str = realloc(string->str, string->allocated_len + len);
		string->allocated_len += len;
	}
	memcpy(string->str + string->len, val, len);
	string->len += len;
	string->str[string->len] = 0;

//	CFStringAppendCString(string->cf_string, val, kCFStringEncodingUTF8);

	return string;
}

GString * g_string_append_uri_escaped(GString *string, const char *unescaped, const char *reserved_chars_allowed, gboolean allow_utf8)
{
	assert(FALSE);
}

GString * g_string_prepend(GString *string, const gchar *val)
{
	size_t len = strlen(val);

	if (string->len + len > string->allocated_len) {
		string->str = realloc(string->str, string->len + len);
		string->allocated_len += len;
	}
	memmove(string->str + len, string->str, string->len);
	memcpy(string->str, val, len);
	string->len += len;

	return string;
}

GString * g_string_prepend_c(GString *string, gchar c)
{
	assert(FALSE);
}

GString * g_string_prepend_unichar(GString *string, gunichar wc)
{
	assert(FALSE);
}

GString * g_string_prepend_len(GString *string, const gchar *val, gssize len)
{
	assert(FALSE);
}

GString * g_string_insert(GString *string, gssize pos, const gchar *val)
{
	assert(FALSE);
}

GString * g_string_insert_c(GString *string, gssize pos, gchar c)
{
	assert(FALSE);
}

GString * g_string_insert_unichar(GString *string, gssize pos, gunichar wc)
{
	assert(FALSE);
}

GString * g_string_insert_len(GString *string, gssize pos, const gchar *val, gssize len)
{
	assert(FALSE);
}

GString * g_string_overwrite(GString *string, gsize pos, const gchar *val)
{
	assert(FALSE);
}

GString * g_string_overwrite_len(GString *string, gsize pos, const gchar *val, gssize len)
{
	assert(FALSE);
}

GString * g_string_erase(GString *string, gssize pos, gssize len)
{
	if (len < -1)
		return string;
	
	if (len == -1)
		len = string->len - pos;

	assert(string->len > pos);

	memmove(string->str + pos, string->str + pos + len, len);
	string->len -= len;

	return string;
}

GString * g_string_truncate(GString *string, gsize len)
{
	assert(FALSE);
}

GString * g_string_set_size(GString *string, gsize len)
{
	assert(FALSE);
}

gchar * g_string_free(GString *string, gboolean free_segment)
{
	char *retval = string->str;

//	CFRelease(string->cf_string);
	if (free_segment) {
		free(string->str);
		retval = NULL;
	}
	free(string);

	return retval;
}

GString * g_string_up(GString *string)
{
	assert(FALSE);
}

GString * g_string_down(GString *string)
{
	assert(FALSE);
}

guint g_string_hash(const GString *str)
{
	assert(FALSE);
}

gboolean g_string_equal(const GString *v, const GString *v2)
{
	assert(FALSE);
}
