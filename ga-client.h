/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* ga-client.h - Header for GaClient (systemd-resolved compatibility) */

#ifndef __GA_CLIENT_H__
#define __GA_CLIENT_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* Avahi-compatible type aliases for interface index and protocol */
typedef int GaIfIndex;
typedef int GaProtocolType;
typedef int GaProtocol;

/* Re-export as Avahi types for drop-in compatibility */
typedef GaIfIndex AvahiIfIndex;
typedef GaProtocol AvahiProtocol;

/* Match Avahi interface constants */
#define GA_IF_UNSPEC -1
#define AVAHI_IF_UNSPEC GA_IF_UNSPEC

/* Match Avahi protocol constants */
#define GA_PROTO_INET   0
#define GA_PROTO_INET6  1
#define GA_PROTO_UNSPEC -1

#define AVAHI_PROTO_INET   GA_PROTO_INET
#define AVAHI_PROTO_INET6  GA_PROTO_INET6
#define AVAHI_PROTO_UNSPEC GA_PROTO_UNSPEC

/* Alternate naming convention compatibility */
#define GA_PROTOCOL_INET   GA_PROTO_INET
#define GA_PROTOCOL_INET6  GA_PROTO_INET6
#define GA_PROTOCOL_UNSPEC GA_PROTO_UNSPEC

/* Avahi utility macros */
#define AVAHI_ADDRESS_STR_MAX 40

#define AVAHI_IF_VALID(ifindex) (((ifindex) >= 0) || ((ifindex) == AVAHI_IF_UNSPEC))
#define AVAHI_PROTO_VALID(protocol) (((protocol) == AVAHI_PROTO_INET) || ((protocol) == AVAHI_PROTO_INET6) || ((protocol) == AVAHI_PROTO_UNSPEC))

/* Avahi memory allocation compatibility - declared in ga-entry-group.h */

typedef enum {
    GA_CLIENT_STATE_NOT_STARTED = -1,
    GA_CLIENT_STATE_S_REGISTERING = 0,
    GA_CLIENT_STATE_S_RUNNING = 1,
    GA_CLIENT_STATE_S_COLLISION = 2,
    GA_CLIENT_STATE_FAILURE = 100,
    GA_CLIENT_STATE_CONNECTING = 101
} GaClientState;

/* Avahi client state compatibility macros */
typedef GaClientState AvahiClientState;

#define AVAHI_CLIENT_S_REGISTERING GA_CLIENT_STATE_S_REGISTERING
#define AVAHI_CLIENT_S_RUNNING     GA_CLIENT_STATE_S_RUNNING
#define AVAHI_CLIENT_S_COLLISION   GA_CLIENT_STATE_S_COLLISION
#define AVAHI_CLIENT_FAILURE       GA_CLIENT_STATE_FAILURE
#define AVAHI_CLIENT_CONNECTING    GA_CLIENT_STATE_CONNECTING

typedef enum {
    GA_CLIENT_FLAG_NO_FLAGS = 0,
    GA_CLIENT_FLAG_IGNORE_USER_CONFIG = 1,
    GA_CLIENT_FLAG_NO_FAIL = 2
} GaClientFlags;

/* Avahi client flag compatibility macros */
typedef GaClientFlags AvahiClientFlags;

#define AVAHI_CLIENT_NO_FLAGS          GA_CLIENT_FLAG_NO_FLAGS
#define AVAHI_CLIENT_IGNORE_USER_CONFIG GA_CLIENT_FLAG_IGNORE_USER_CONFIG
#define AVAHI_CLIENT_NO_FAIL           GA_CLIENT_FLAG_NO_FAIL

typedef struct _GaClient GaClient;
typedef struct _GaClientClass GaClientClass;
typedef struct _GaClientPrivate GaClientPrivate;

struct _GaClientClass {
    GObjectClass parent_class;
};

struct _GaClient {
    GObject parent;
    GaClientPrivate *priv;
};

GType ga_client_get_type(void);

/* TYPE MACROS */
#define GA_TYPE_CLIENT \
    (ga_client_get_type())
#define GA_CLIENT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GA_TYPE_CLIENT, GaClient))
#define GA_CLIENT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GA_TYPE_CLIENT, GaClientClass))
#define IS_GA_CLIENT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GA_TYPE_CLIENT))
#define IS_GA_CLIENT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GA_TYPE_CLIENT))
#define GA_CLIENT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GA_TYPE_CLIENT, GaClientClass))

GaClient *ga_client_new(GaClientFlags flags);

gboolean ga_client_start(GaClient * client, GError ** error);

gboolean ga_client_start_in_context(GaClient * client, GMainContext * context, GError ** error);

/* Accessor functions for client properties */
GaClientState ga_client_get_state(GaClient *client);

const gchar *ga_client_get_host_name(GaClient *client);

const gchar *ga_client_get_host_name_fqdn(GaClient *client);

const gchar *ga_client_get_domain_name(GaClient *client);

/* Get the last error code from the client */
gint ga_client_get_errno(GaClient *client);

G_END_DECLS

#endif /* #ifndef __GA_CLIENT_H__ */
