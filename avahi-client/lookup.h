/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * Avahi drop-in compatibility wrapper for avahi-client/lookup.h
 *
 * This redirects to the GObject-based service browser/resolver.
 */

#ifndef __AVAHI_CLIENT_LOOKUP_H_COMPAT__
#define __AVAHI_CLIENT_LOOKUP_H_COMPAT__

/* Include the GObject-based browser/resolver interfaces */
#include "../ga-service-browser.h"
#include "../ga-service-resolver.h"
#include "../ga-record-browser.h"

/* Include common definitions */
#include "../avahi-common/defs.h"

/*
 * Note: resolve-avahi-compat uses the avahi-gobject API (GaServiceBrowser, etc.)
 * not the low-level avahi-client API. This header is provided for source
 * compatibility.
 */

/* Compatibility type aliases */
typedef GaServiceBrowser AvahiServiceBrowser;
typedef GaServiceResolver AvahiServiceResolver;
typedef GaRecordBrowser AvahiRecordBrowser;

#endif /* __AVAHI_CLIENT_LOOKUP_H_COMPAT__ */
