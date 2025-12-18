/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * Avahi drop-in compatibility wrapper for avahi-glib/glib-malloc.h
 *
 * Memory allocation via GLib.
 */

#ifndef __AVAHI_GLIB_MALLOC_H_COMPAT__
#define __AVAHI_GLIB_MALLOC_H_COMPAT__

#include <glib.h>
#include "../avahi-common/malloc.h"

/* Return a pointer to a memory allocator that uses GLib's g_malloc().
 * For resolve-avahi-compat, this returns NULL as GLib is always used. */
static inline const AvahiAllocator* avahi_glib_allocator(void) {
    return NULL;
}

#endif /* __AVAHI_GLIB_MALLOC_H_COMPAT__ */
