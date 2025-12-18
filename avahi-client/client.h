/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * Avahi drop-in compatibility wrapper for avahi-client/client.h
 *
 * This redirects to the GObject-based client interface.
 */

#ifndef __AVAHI_CLIENT_CLIENT_H_COMPAT__
#define __AVAHI_CLIENT_CLIENT_H_COMPAT__

/* Include the GObject-based client */
#include "../ga-client.h"

/* Include common definitions */
#include "../avahi-common/defs.h"
#include "../avahi-common/address.h"
#include "../avahi-common/strlst.h"

/*
 * Compatibility type aliases for code that uses the low-level C API
 *
 * Note: resolve-avahi-compat uses the avahi-gobject API (GaClient, etc.)
 * not the low-level avahi-client API. This header is provided for source
 * compatibility, but applications should use GaClient directly.
 */

/* The GaClient is the equivalent of AvahiClient */
typedef GaClient AvahiClient;

/* Client callback type - not directly used in the GObject API */
typedef void (*AvahiClientCallback)(
    AvahiClient *client,
    AvahiClientState state,
    void *userdata);

/* Version information */
#define AVAHI_CLIENT_VERSION "resolve-avahi-compat"

/* Avahi client API functions - real exported symbols */
const char *avahi_client_get_version_string(AvahiClient *client);
const char *avahi_client_get_host_name(AvahiClient *client);
const char *avahi_client_get_host_name_fqdn(AvahiClient *client);
const char *avahi_client_get_domain_name(AvahiClient *client);
AvahiClientState avahi_client_get_state(AvahiClient *client);
int avahi_client_errno(AvahiClient *client);

#endif /* __AVAHI_CLIENT_CLIENT_H_COMPAT__ */
