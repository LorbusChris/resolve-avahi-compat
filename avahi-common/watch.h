/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * Avahi drop-in compatibility wrapper for avahi-common/watch.h
 *
 * The watch/poll abstraction is not needed when using GObject;
 * GLib's main loop is used instead.
 */

#ifndef __AVAHI_COMMON_WATCH_H_COMPAT__
#define __AVAHI_COMMON_WATCH_H_COMPAT__

#include <sys/poll.h>
#include <sys/time.h>

/* Event mask - for compatibility only */
typedef enum {
    AVAHI_WATCH_IN = POLLIN,
    AVAHI_WATCH_OUT = POLLOUT,
    AVAHI_WATCH_ERR = POLLERR,
    AVAHI_WATCH_HUP = POLLHUP,
} AvahiWatchEvent;

/* Opaque types - not actually used in resolve-avahi-compat */
typedef struct AvahiWatch AvahiWatch;
typedef struct AvahiTimeout AvahiTimeout;
typedef struct AvahiPoll AvahiPoll;

/*
 * In resolve-avahi-compat (avahi-gobject), the poll abstraction
 * is entirely internal and uses GLib's event loop. This header
 * is provided only for source compatibility.
 */

#endif /* __AVAHI_COMMON_WATCH_H_COMPAT__ */
