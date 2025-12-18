/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * Avahi drop-in compatibility wrapper for avahi-client/publish.h
 *
 * This redirects to the GObject-based entry group interface.
 */

#ifndef __AVAHI_CLIENT_PUBLISH_H_COMPAT__
#define __AVAHI_CLIENT_PUBLISH_H_COMPAT__

/* Include the GObject-based entry group interface */
#include "../ga-entry-group.h"

/* Include common definitions */
#include "../avahi-common/defs.h"
#include "../avahi-common/strlst.h"

/*
 * Note: resolve-avahi-compat uses the avahi-gobject API (GaEntryGroup, etc.)
 * not the low-level avahi-client API. This header is provided for source
 * compatibility.
 */

/* Compatibility type alias */
typedef GaEntryGroup AvahiEntryGroup;

/* Publish flags - for source compatibility */
typedef enum {
    AVAHI_PUBLISH_UNIQUE = 1 << 0,
    AVAHI_PUBLISH_NO_PROBE = 1 << 1,
    AVAHI_PUBLISH_NO_ANNOUNCE = 1 << 2,
    AVAHI_PUBLISH_ALLOW_MULTIPLE = 1 << 3,
    AVAHI_PUBLISH_NO_REVERSE = 1 << 4,
    AVAHI_PUBLISH_NO_COOKIE = 1 << 5,
    AVAHI_PUBLISH_UPDATE = 1 << 6,
    AVAHI_PUBLISH_USE_WIDE_AREA = 1 << 7,
    AVAHI_PUBLISH_USE_MULTICAST = 1 << 8,
} AvahiPublishFlags;

#endif /* __AVAHI_CLIENT_PUBLISH_H_COMPAT__ */
