/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* ga-client.c - Source for GaClient (systemd-resolved compatibility) */

#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-varlink.h>

#include "ga-client.h"
#include "ga-error.h"
#include "ga-enums.h"

#define RESOLVED_VARLINK_ADDRESS "/run/systemd/resolve/io.systemd.Resolve"

/* signal enum */
enum {
    STATE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* properties */
enum {
    PROP_STATE = 1,
    PROP_FLAGS
};

struct _GaClientPrivate {
    GaClientFlags flags;
    GaClientState state;
    GMainContext *context;
    gboolean dispose_has_run;
};

#define GA_CLIENT_GET_PRIVATE(o) \
    ((GaClientPrivate *)ga_client_get_instance_private(o))

G_DEFINE_TYPE_WITH_PRIVATE(GaClient, ga_client, G_TYPE_OBJECT)

/* GType for enums */
GType ga_client_state_get_type(void) {
    static GType type = 0;
    if (G_UNLIKELY(type == 0)) {
        static const GEnumValue values[] = {
            { GA_CLIENT_STATE_NOT_STARTED, "GA_CLIENT_STATE_NOT_STARTED", "not-started" },
            { GA_CLIENT_STATE_S_REGISTERING, "GA_CLIENT_STATE_S_REGISTERING", "registering" },
            { GA_CLIENT_STATE_S_RUNNING, "GA_CLIENT_STATE_S_RUNNING", "running" },
            { GA_CLIENT_STATE_S_COLLISION, "GA_CLIENT_STATE_S_COLLISION", "collision" },
            { GA_CLIENT_STATE_FAILURE, "GA_CLIENT_STATE_FAILURE", "failure" },
            { GA_CLIENT_STATE_CONNECTING, "GA_CLIENT_STATE_CONNECTING", "connecting" },
            { 0, NULL, NULL }
        };
        type = g_enum_register_static("GaClientState", values);
    }
    return type;
}

#define GA_TYPE_CLIENT_STATE (ga_client_state_get_type())

GType ga_client_flags_get_type(void) {
    static GType type = 0;
    if (G_UNLIKELY(type == 0)) {
        static const GFlagsValue values[] = {
            { GA_CLIENT_FLAG_NO_FLAGS, "GA_CLIENT_FLAG_NO_FLAGS", "no-flags" },
            { GA_CLIENT_FLAG_IGNORE_USER_CONFIG, "GA_CLIENT_FLAG_IGNORE_USER_CONFIG", "ignore-user-config" },
            { GA_CLIENT_FLAG_NO_FAIL, "GA_CLIENT_FLAG_NO_FAIL", "no-fail" },
            { 0, NULL, NULL }
        };
        type = g_flags_register_static("GaClientFlags", values);
    }
    return type;
}

#define GA_TYPE_CLIENT_FLAGS (ga_client_flags_get_type())

static void ga_client_init(GaClient *self) {
    GaClientPrivate *priv = GA_CLIENT_GET_PRIVATE(self);
    priv->state = GA_CLIENT_STATE_NOT_STARTED;
    priv->flags = GA_CLIENT_FLAG_NO_FLAGS;
    priv->context = NULL;
    priv->dispose_has_run = FALSE;
}

static void ga_client_dispose(GObject *object);
static void ga_client_finalize(GObject *object);

static void ga_client_set_property(GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec) {
    GaClient *client = GA_CLIENT(object);
    GaClientPrivate *priv = GA_CLIENT_GET_PRIVATE(client);

    switch (property_id) {
        case PROP_FLAGS:
            priv->flags = g_value_get_flags(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ga_client_get_property(GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec) {
    GaClient *client = GA_CLIENT(object);
    GaClientPrivate *priv = GA_CLIENT_GET_PRIVATE(client);

    switch (property_id) {
        case PROP_STATE:
            g_value_set_enum(value, priv->state);
            break;
        case PROP_FLAGS:
            g_value_set_flags(value, priv->flags);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static GQuark detail_for_state(GaClientState state) {
    static struct {
        GaClientState state;
        const gchar *name;
        GQuark quark;
    } states[] = {
        { GA_CLIENT_STATE_S_REGISTERING, "registering", 0 },
        { GA_CLIENT_STATE_S_RUNNING, "running", 0 },
        { GA_CLIENT_STATE_S_COLLISION, "collision", 0 },
        { GA_CLIENT_STATE_FAILURE, "failure", 0 },
        { GA_CLIENT_STATE_CONNECTING, "connecting", 0 },
        { 0, NULL, 0 }
    };
    
    for (int i = 0; states[i].name != NULL; i++) {
        if (state != states[i].state)
            continue;
        if (!states[i].quark)
            states[i].quark = g_quark_from_static_string(states[i].name);
        return states[i].quark;
    }
    return 0;
}

static void ga_client_class_init(GaClientClass *ga_client_class) {
    GObjectClass *object_class = G_OBJECT_CLASS(ga_client_class);
    GParamSpec *param_spec;

    object_class->dispose = ga_client_dispose;
    object_class->finalize = ga_client_finalize;
    object_class->set_property = ga_client_set_property;
    object_class->get_property = ga_client_get_property;

    param_spec = g_param_spec_enum("state", "Client state",
                                   "The state of the client",
                                   GA_TYPE_CLIENT_STATE,
                                   GA_CLIENT_STATE_NOT_STARTED,
                                   G_PARAM_READABLE |
                                   G_PARAM_STATIC_NAME |
                                   G_PARAM_STATIC_BLURB);
    g_object_class_install_property(object_class, PROP_STATE, param_spec);

    param_spec = g_param_spec_flags("flags", "Client flags",
                                    "The flags the client is started with",
                                    GA_TYPE_CLIENT_FLAGS,
                                    GA_CLIENT_FLAG_NO_FLAGS,
                                    G_PARAM_READWRITE |
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_STATIC_NAME |
                                    G_PARAM_STATIC_BLURB);
    g_object_class_install_property(object_class, PROP_FLAGS, param_spec);

    signals[STATE_CHANGED] =
        g_signal_new("state-changed",
                     G_OBJECT_CLASS_TYPE(ga_client_class),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__ENUM,
                     G_TYPE_NONE, 1, GA_TYPE_CLIENT_STATE);
}

void ga_client_dispose(GObject *object) {
    GaClient *self = GA_CLIENT(object);
    GaClientPrivate *priv = GA_CLIENT_GET_PRIVATE(self);

    if (priv->dispose_has_run)
        return;

    priv->dispose_has_run = TRUE;

    if (priv->context) {
        g_main_context_unref(priv->context);
        priv->context = NULL;
    }

    if (G_OBJECT_CLASS(ga_client_parent_class)->dispose)
        G_OBJECT_CLASS(ga_client_parent_class)->dispose(object);
}

void ga_client_finalize(GObject *object) {
    G_OBJECT_CLASS(ga_client_parent_class)->finalize(object);
}

GaClient *ga_client_new(GaClientFlags flags) {
    return g_object_new(GA_TYPE_CLIENT, "flags", flags, NULL);
}

gboolean ga_client_start(GaClient *client, GError **error) {
    return ga_client_start_in_context(client, NULL, error);
}

gboolean ga_client_start_in_context(GaClient *client, GMainContext *context, GError **error) {
    GaClientPrivate *priv = GA_CLIENT_GET_PRIVATE(client);
    sd_varlink *vl = NULL;
    int r;

    g_return_val_if_fail(IS_GA_CLIENT(client), FALSE);

    /* Test connection to systemd-resolved */
    priv->state = GA_CLIENT_STATE_CONNECTING;
    g_signal_emit(client, signals[STATE_CHANGED],
                  detail_for_state(priv->state), priv->state);

    r = sd_varlink_connect_address(&vl, RESOLVED_VARLINK_ADDRESS);
    if (r < 0) {
        priv->state = GA_CLIENT_STATE_FAILURE;
        g_signal_emit(client, signals[STATE_CHANGED],
                      detail_for_state(priv->state), priv->state);
        
        if (error != NULL) {
            *error = g_error_new(GA_ERROR, GA_ERROR_NO_DAEMON,
                                 "Failed to connect to systemd-resolved: %s",
                                 g_strerror(-r));
        }
        return FALSE;
    }

    sd_varlink_unref(vl);

    if (context) {
        priv->context = g_main_context_ref(context);
    }

    priv->state = GA_CLIENT_STATE_S_RUNNING;
    g_signal_emit(client, signals[STATE_CHANGED],
                  detail_for_state(priv->state), priv->state);

    return TRUE;
}

GaClientState ga_client_get_state(GaClient *client) {
    g_return_val_if_fail(IS_GA_CLIENT(client), GA_CLIENT_STATE_FAILURE);
    GaClientPrivate *priv = GA_CLIENT_GET_PRIVATE(client);
    return priv->state;
}

const gchar *ga_client_get_host_name(GaClient *client) {
    g_return_val_if_fail(IS_GA_CLIENT(client), NULL);

    /*
     * Return the local hostname.
     * In Avahi, this comes from the daemon; for systemd-resolved,
     * we return the system hostname.
     */
    return g_get_host_name();
}

const gchar *ga_client_get_host_name_fqdn(GaClient *client) {
    g_return_val_if_fail(IS_GA_CLIENT(client), NULL);

    /*
     * Return the fully-qualified domain name.
     * For systemd-resolved, we append .local to the hostname.
     */
    static gchar fqdn[256];
    g_snprintf(fqdn, sizeof(fqdn), "%s.local", g_get_host_name());
    return fqdn;
}

const gchar *ga_client_get_domain_name(GaClient *client) {
    g_return_val_if_fail(IS_GA_CLIENT(client), NULL);

    /* In mDNS/DNS-SD, the default domain is "local" */
    return "local";
}

gint ga_client_get_errno(GaClient *client) {
    g_return_val_if_fail(IS_GA_CLIENT(client), GA_ERROR_FAILURE);
    GaClientPrivate *priv = GA_CLIENT_GET_PRIVATE(client);

    /* Map state to error code */
    switch (priv->state) {
        case GA_CLIENT_STATE_S_RUNNING:
        case GA_CLIENT_STATE_S_REGISTERING:
            return GA_ERROR_OK;
        case GA_CLIENT_STATE_FAILURE:
            return GA_ERROR_FAILURE;
        case GA_CLIENT_STATE_S_COLLISION:
            return GA_ERROR_COLLISION;
        case GA_CLIENT_STATE_CONNECTING:
        case GA_CLIENT_STATE_NOT_STARTED:
        default:
            return GA_ERROR_NOT_PERMITTED;
    }
}

/*
 * Avahi client API function exports.
 * Real function symbols for binary compatibility with applications
 * compiled against Avahi headers.
 */

const char *avahi_client_get_version_string(G_GNUC_UNUSED GaClient *client) {
    return "resolve-avahi-compat";
}

const char *avahi_client_get_host_name(GaClient *client) {
    return ga_client_get_host_name(client);
}

const char *avahi_client_get_host_name_fqdn(GaClient *client) {
    return ga_client_get_host_name_fqdn(client);
}

const char *avahi_client_get_domain_name(GaClient *client) {
    return ga_client_get_domain_name(client);
}

GaClientState avahi_client_get_state(GaClient *client) {
    return ga_client_get_state(client);
}

int avahi_client_errno(GaClient *client) {
    return ga_client_get_errno(client);
}
