/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * Avahi drop-in compatibility wrapper for avahi-common/malloc.h
 *
 * Maps Avahi memory allocation functions to GLib.
 */

#ifndef __AVAHI_COMMON_MALLOC_H_COMPAT__
#define __AVAHI_COMMON_MALLOC_H_COMPAT__

#include <glib.h>

/*
 * Avahi memory allocation function declarations.
 * These are real exported symbols for binary compatibility.
 */
void *avahi_malloc(size_t size);
void *avahi_malloc0(size_t size);
void *avahi_realloc(void *p, size_t size);
void avahi_free(void *p);
char *avahi_strdup(const char *s);
char *avahi_strndup(const char *s, size_t n);
void *avahi_memdup(const void *p, size_t size);

/* Avahi allocator structure - not needed for GLib-based code */
typedef struct AvahiAllocator {
    void* (*malloc)(size_t size);
    void (*free)(void *p);
    void* (*realloc)(void *p, size_t size);
    void* (*calloc)(size_t nmemb, size_t size);
} AvahiAllocator;

/* No-op for compatibility */
static inline void avahi_set_allocator(const AvahiAllocator *a) { (void)a; }

#endif /* __AVAHI_COMMON_MALLOC_H_COMPAT__ */
