/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * Avahi drop-in compatibility wrapper for avahi-glib/glib-watch.h
 *
 * The GLib integration is handled differently in resolve-avahi-compat;
 * this header is provided for source compatibility.
 */

#ifndef __AVAHI_GLIB_WATCH_H_COMPAT__
#define __AVAHI_GLIB_WATCH_H_COMPAT__

#include <glib.h>

/*
 * In avahi-gobject, the GLib main loop integration is built-in.
 * We don't need AvahiGLibPoll since we use GObject signals.
 *
 * This stub is provided for code that includes this header.
 */

typedef struct AvahiGLibPoll AvahiGLibPoll;

/* Stub functions - not actually needed for resolve-avahi-compat */
static inline AvahiGLibPoll* avahi_glib_poll_new(GMainContext *c, gint priority) {
    (void)c;
    (void)priority;
    return NULL;
}

static inline void avahi_glib_poll_free(AvahiGLibPoll *g) {
    (void)g;
}

#endif /* __AVAHI_GLIB_WATCH_H_COMPAT__ */
