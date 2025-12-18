/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* ga-service-browser.c - Source for GaServiceBrowser (systemd-resolved compatibility) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-varlink.h>

#include "ga-service-browser.h"
#include "ga-error.h"

#define RESOLVED_VARLINK_ADDRESS "/run/systemd/resolve/io.systemd.Resolve"

/* signal enum */
enum {
    NEW_SERVICE,
    REMOVED_SERVICE,
    CACHE_EXHAUSTED,
    ALL_FOR_NOW,
    FAILURE,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* properties */
enum {
    PROP_PROTOCOL = 1,
    PROP_IFINDEX,
    PROP_TYPE,
    PROP_DOMAIN,
    PROP_FLAGS
};

struct _GaServiceBrowserPrivate {
    GaClient *client;
    sd_varlink *link;
    int varlink_fd;
    GSource *varlink_source;
    GaIfIndex interface;
    GaProtocol protocol;
    char *type;
    char *domain;
    GaLookupFlags flags;
    gboolean dispose_has_run;
    gboolean initial_snapshot_done;
};

#define GA_SERVICE_BROWSER_GET_PRIVATE(o) \
    ((GaServiceBrowserPrivate *)ga_service_browser_get_instance_private(o))

G_DEFINE_TYPE_WITH_PRIVATE(GaServiceBrowser, ga_service_browser, G_TYPE_OBJECT)

static void ga_service_browser_init(GaServiceBrowser *obj) {
    GaServiceBrowserPrivate *priv = GA_SERVICE_BROWSER_GET_PRIVATE(obj);

    priv->client = NULL;
    priv->link = NULL;
    priv->varlink_fd = -1;
    priv->varlink_source = NULL;
    priv->type = NULL;
    priv->domain = NULL;
    priv->interface = GA_IF_UNSPEC;
    priv->protocol = GA_PROTOCOL_UNSPEC;
    priv->initial_snapshot_done = FALSE;
}

static void ga_service_browser_dispose(GObject *object);
static void ga_service_browser_finalize(GObject *object);
static void disconnect_from_resolved(GaServiceBrowser *browser);
gboolean ga_service_browser_attach(GaServiceBrowser *browser,
                                   GaClient *client,
                                   GError **error);

static void ga_service_browser_set_property(GObject *object,
                                            guint property_id,
                                            const GValue *value,
                                            GParamSpec *pspec) {
    GaServiceBrowser *browser = GA_SERVICE_BROWSER(object);
    GaServiceBrowserPrivate *priv = GA_SERVICE_BROWSER_GET_PRIVATE(browser);

    switch (property_id) {
        case PROP_PROTOCOL:
            priv->protocol = g_value_get_enum(value);
            break;
        case PROP_IFINDEX:
            priv->interface = g_value_get_int(value);
            break;
        case PROP_TYPE:
            g_free(priv->type);
            priv->type = g_strdup(g_value_get_string(value));
            break;
        case PROP_DOMAIN:
            g_free(priv->domain);
            priv->domain = g_strdup(g_value_get_string(value));
            break;
        case PROP_FLAGS:
            priv->flags = g_value_get_flags(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ga_service_browser_get_property(GObject *object,
                                            guint property_id,
                                            GValue *value,
                                            GParamSpec *pspec) {
    GaServiceBrowser *browser = GA_SERVICE_BROWSER(object);
    GaServiceBrowserPrivate *priv = GA_SERVICE_BROWSER_GET_PRIVATE(browser);

    switch (property_id) {
        case PROP_PROTOCOL:
            g_value_set_enum(value, priv->protocol);
            break;
        case PROP_IFINDEX:
            g_value_set_int(value, priv->interface);
            break;
        case PROP_TYPE:
            g_value_set_string(value, priv->type);
            break;
        case PROP_DOMAIN:
            g_value_set_string(value, priv->domain);
            break;
        case PROP_FLAGS:
            g_value_set_flags(value, priv->flags);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ga_service_browser_class_init(GaServiceBrowserClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GParamSpec *param_spec;

    object_class->dispose = ga_service_browser_dispose;
    object_class->finalize = ga_service_browser_finalize;
    object_class->set_property = ga_service_browser_set_property;
    object_class->get_property = ga_service_browser_get_property;

    signals[NEW_SERVICE] =
        g_signal_new("new-service",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     NULL,  /* Use default marshaller */
                     G_TYPE_NONE, 6,
                     G_TYPE_INT,           /* interface */
                     GA_TYPE_PROTOCOL,     /* protocol */
                     G_TYPE_STRING,        /* name */
                     G_TYPE_STRING,        /* type */
                     G_TYPE_STRING,        /* domain */
                     GA_TYPE_LOOKUP_RESULT_FLAGS);  /* flags */

    signals[REMOVED_SERVICE] =
        g_signal_new("removed-service",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     NULL,
                     G_TYPE_NONE, 6,
                     G_TYPE_INT,
                     GA_TYPE_PROTOCOL,
                     G_TYPE_STRING,
                     G_TYPE_STRING,
                     G_TYPE_STRING,
                     GA_TYPE_LOOKUP_RESULT_FLAGS);

    signals[ALL_FOR_NOW] =
        g_signal_new("all-for-now",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    signals[CACHE_EXHAUSTED] =
        g_signal_new("cache-exhausted",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    signals[FAILURE] =
        g_signal_new("failure",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE, 1, G_TYPE_POINTER);

    param_spec = g_param_spec_enum("protocol", "Protocol",
                                   "Protocol to browse",
                                   GA_TYPE_PROTOCOL,
                                   GA_PROTOCOL_UNSPEC,
                                   G_PARAM_READWRITE |
                                   G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_PROTOCOL, param_spec);

    param_spec = g_param_spec_int("interface", "Interface index",
                                  "Interface to use for browsing",
                                  G_MININT, G_MAXINT,
                                  GA_IF_UNSPEC,
                                  G_PARAM_READWRITE |
                                  G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_IFINDEX, param_spec);

    param_spec = g_param_spec_string("type", "Service type",
                                     "Service type to browse for",
                                     NULL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_TYPE, param_spec);

    param_spec = g_param_spec_string("domain", "Domain",
                                     "Domain to browse in",
                                     NULL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_DOMAIN, param_spec);

    param_spec = g_param_spec_flags("flags", "Lookup flags",
                                    "Browser lookup flags",
                                    GA_TYPE_LOOKUP_FLAGS,
                                    GA_LOOKUP_NO_FLAGS,
                                    G_PARAM_READWRITE |
                                    G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_FLAGS, param_spec);
}

static void disconnect_from_resolved(GaServiceBrowser *browser) {
    GaServiceBrowserPrivate *priv = GA_SERVICE_BROWSER_GET_PRIVATE(browser);

    if (priv->varlink_source) {
        g_source_destroy(priv->varlink_source);
        priv->varlink_source = NULL;
    }

    if (priv->link) {
        sd_varlink_unref(priv->link);
        priv->link = NULL;
    }

    priv->varlink_fd = -1;
}

void ga_service_browser_dispose(GObject *object) {
    GaServiceBrowser *self = GA_SERVICE_BROWSER(object);
    GaServiceBrowserPrivate *priv = GA_SERVICE_BROWSER_GET_PRIVATE(self);

    if (priv->dispose_has_run)
        return;

    priv->dispose_has_run = TRUE;

    disconnect_from_resolved(self);

    if (priv->client) {
        g_object_unref(priv->client);
        priv->client = NULL;
    }

    if (G_OBJECT_CLASS(ga_service_browser_parent_class)->dispose)
        G_OBJECT_CLASS(ga_service_browser_parent_class)->dispose(object);
}

void ga_service_browser_finalize(GObject *object) {
    GaServiceBrowser *self = GA_SERVICE_BROWSER(object);
    GaServiceBrowserPrivate *priv = GA_SERVICE_BROWSER_GET_PRIVATE(self);

    g_free(priv->type);
    g_free(priv->domain);

    G_OBJECT_CLASS(ga_service_browser_parent_class)->finalize(object);
}

/* Varlink notification callback */
static int browse_notify_cb(G_GNUC_UNUSED sd_varlink *link,
                            sd_json_variant *parameters,
                            const char *error_id,
                            G_GNUC_UNUSED sd_varlink_reply_flags_t flags,
                            void *userdata) {
    GaServiceBrowser *browser = GA_SERVICE_BROWSER(userdata);
    GaServiceBrowserPrivate *priv = GA_SERVICE_BROWSER_GET_PRIVATE(browser);

    g_debug("GaServiceBrowser: browse_notify_cb called, error_id=%s",
            error_id ? error_id : "(none)");

    if (error_id) {
        /* If the subscription timed out or disconnected, try to reconnect.
         * systemd-resolved BrowseServices has a default timeout. */
        if (g_strcmp0(error_id, "io.systemd.TimedOut") == 0 ||
            g_strcmp0(error_id, "io.systemd.Disconnected") == 0) {
            g_debug("GaServiceBrowser: Subscription ended (%s), attempting reconnect", error_id);
            /* Disconnect and reconnect */
            disconnect_from_resolved(browser);
            GError *reconnect_error = NULL;
            if (!ga_service_browser_attach(browser, priv->client, &reconnect_error)) {
                g_warning("GaServiceBrowser: Failed to reconnect: %s",
                          reconnect_error ? reconnect_error->message : "unknown");
                GError *error = g_error_new(GA_ERROR, GA_ERROR_FAILURE,
                                            "Reconnection failed: %s", error_id);
                g_signal_emit(browser, signals[FAILURE], 0, error);
                g_error_free(error);
                g_clear_error(&reconnect_error);
            } else {
                g_debug("GaServiceBrowser: Successfully reconnected after %s", error_id);
            }
            return 0;
        }

        GError *error = g_error_new(GA_ERROR, GA_ERROR_FAILURE,
                                    "Browse error: %s", error_id);
        g_signal_emit(browser, signals[FAILURE], 0, error);
        g_error_free(error);
        return 0;
    }

    sd_json_variant *array = sd_json_variant_by_key(parameters, "browserServiceData");
    if (!array || !sd_json_variant_is_array(array)) {
        g_debug("GaServiceBrowser: No browserServiceData array in notification");
        return 0;
    }

    size_t n = sd_json_variant_elements(array);
    g_debug("GaServiceBrowser: Processing %zu service entries", n);

    for (size_t i = 0; i < n; i++) {
        sd_json_variant *entry = sd_json_variant_by_index(array, i);
        if (!entry || !sd_json_variant_is_object(entry))
            continue;

        sd_json_variant *update_flag_v = sd_json_variant_by_key(entry, "updateFlag");
        sd_json_variant *name_v = sd_json_variant_by_key(entry, "name");
        sd_json_variant *type_v = sd_json_variant_by_key(entry, "type");
        sd_json_variant *domain_v = sd_json_variant_by_key(entry, "domain");
        sd_json_variant *ifindex_v = sd_json_variant_by_key(entry, "ifindex");

        const char *update_flag = (update_flag_v && sd_json_variant_is_string(update_flag_v))
                                  ? sd_json_variant_string(update_flag_v) : NULL;
        const char *name = (name_v && sd_json_variant_is_string(name_v))
                           ? sd_json_variant_string(name_v) : NULL;
        const char *type = (type_v && sd_json_variant_is_string(type_v))
                           ? sd_json_variant_string(type_v) : NULL;
        const char *domain = (domain_v && sd_json_variant_is_string(domain_v))
                             ? sd_json_variant_string(domain_v) : NULL;
        int64_t ifindex = (ifindex_v && sd_json_variant_is_integer(ifindex_v))
                          ? sd_json_variant_integer(ifindex_v) : 0;

        g_debug("GaServiceBrowser: Entry[%zu]: flag=%s name=%s type=%s domain=%s ifindex=%"G_GINT64_FORMAT,
                i, update_flag ? update_flag : "(null)",
                name ? name : "(null)", type ? type : "(null)",
                domain ? domain : "(null)", (gint64)ifindex);

        /* Filter by type if specified */
        if (priv->type && type && g_strcmp0(priv->type, type) != 0) {
            g_debug("GaServiceBrowser: Skipping, type mismatch (want=%s)", priv->type);
            continue;
        }

        GaLookupResultFlags result_flags = GA_LOOKUP_RESULT_MULTICAST;

        if (g_strcmp0(update_flag, "added") == 0) {
            g_debug("GaServiceBrowser: Emitting new-service for '%s'", name ? name : "(null)");
            priv->initial_snapshot_done = TRUE;
            g_signal_emit(browser, signals[NEW_SERVICE], 0,
                          (gint)ifindex,
                          priv->protocol,
                          name,
                          type,
                          domain,
                          result_flags);
        } else if (g_strcmp0(update_flag, "removed") == 0) {
            g_debug("GaServiceBrowser: Emitting removed-service for '%s'", name ? name : "(null)");
            g_signal_emit(browser, signals[REMOVED_SERVICE], 0,
                          (gint)ifindex,
                          priv->protocol,
                          name,
                          type,
                          domain,
                          result_flags);
        } else {
            g_debug("GaServiceBrowser: Unknown update_flag '%s'", update_flag ? update_flag : "(null)");
        }
    }

    return 0;
}

/* GLib IO callback for varlink */
static gboolean varlink_io_cb(G_GNUC_UNUSED GIOChannel *source,
                              GIOCondition condition,
                              gpointer user_data) {
    GaServiceBrowser *browser = GA_SERVICE_BROWSER(user_data);
    GaServiceBrowserPrivate *priv = GA_SERVICE_BROWSER_GET_PRIVATE(browser);

    g_debug("GaServiceBrowser: varlink_io_cb triggered, condition=0x%x", condition);

    if (condition & (G_IO_HUP | G_IO_ERR)) {
        g_debug("GaServiceBrowser: Connection lost (HUP or ERR)");
        GError *error = g_error_new(GA_ERROR, GA_ERROR_DISCONNECTED,
                                    "Connection to systemd-resolved lost");
        g_signal_emit(browser, signals[FAILURE], 0, error);
        g_error_free(error);
        return G_SOURCE_REMOVE;
    }

    int r;
    while ((r = sd_varlink_process(priv->link)) > 0)
        ;

    if (r < 0) {
        g_debug("GaServiceBrowser: varlink processing error: %s", g_strerror(-r));
        GError *error = g_error_new(GA_ERROR, GA_ERROR_FAILURE,
                                    "Varlink processing error: %s",
                                    g_strerror(-r));
        g_signal_emit(browser, signals[FAILURE], 0, error);
        g_error_free(error);
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

GaServiceBrowser *ga_service_browser_new(const gchar *type) {
    return ga_service_browser_new_full(GA_IF_UNSPEC, GA_PROTOCOL_UNSPEC,
                                       type, NULL, GA_LOOKUP_NO_FLAGS);
}

GaServiceBrowser *ga_service_browser_new_full(GaIfIndex interface,
                                              GaProtocol protocol,
                                              const gchar *type,
                                              gchar *domain,
                                              GaLookupFlags flags) {
    return g_object_new(GA_TYPE_SERVICE_BROWSER,
                        "interface", interface,
                        "protocol", protocol,
                        "type", type,
                        "domain", domain,
                        "flags", flags,
                        NULL);
}

gboolean ga_service_browser_attach(GaServiceBrowser *browser,
                                   GaClient *client,
                                   GError **error) {
    GaServiceBrowserPrivate *priv = GA_SERVICE_BROWSER_GET_PRIVATE(browser);
    int r;

    g_return_val_if_fail(IS_GA_SERVICE_BROWSER(browser), FALSE);
    g_return_val_if_fail(IS_GA_CLIENT(client), FALSE);

    g_object_ref(client);
    priv->client = client;

    /* Connect to systemd-resolved */
    r = sd_varlink_connect_address(&priv->link, RESOLVED_VARLINK_ADDRESS);
    if (r < 0) {
        if (error) {
            *error = g_error_new(GA_ERROR, GA_ERROR_NO_DAEMON,
                                 "Failed to connect to systemd-resolved: %s",
                                 g_strerror(-r));
        }
        return FALSE;
    }

    priv->varlink_fd = sd_varlink_get_fd(priv->link);
    if (priv->varlink_fd < 0) {
        if (error) {
            *error = g_error_new(GA_ERROR, GA_ERROR_FAILURE,
                                 "Failed to get varlink fd");
        }
        sd_varlink_unref(priv->link);
        priv->link = NULL;
        return FALSE;
    }

    /* Set up GLib main loop integration */
    GIOChannel *channel = g_io_channel_unix_new(priv->varlink_fd);
    priv->varlink_source = g_io_create_watch(channel, G_IO_IN | G_IO_HUP | G_IO_ERR);
    g_io_channel_unref(channel);

    g_source_set_callback(priv->varlink_source,
                          G_SOURCE_FUNC(varlink_io_cb),
                          browser,
                          NULL);
    g_source_attach(priv->varlink_source, NULL);

    sd_varlink_set_userdata(priv->link, browser);
    sd_varlink_bind_reply(priv->link, browse_notify_cb);

    /* Start browsing.
     * GA_IF_UNSPEC (-1) means "all interfaces" - we pass it directly to systemd-resolved
     * which (with the ifindex<=0 patch) normalizes -1 to 0 and browses all mDNS interfaces.
     * This provides full Avahi AVAHI_IF_UNSPEC semantics. */
    const char *domain = priv->domain ? priv->domain : "local";
    int ifindex = priv->interface;

    r = sd_varlink_observebo(priv->link,
                             "io.systemd.Resolve.BrowseServices",
                             SD_JSON_BUILD_PAIR_STRING("domain", domain),
                             SD_JSON_BUILD_PAIR_STRING("type", priv->type),
                             SD_JSON_BUILD_PAIR_INTEGER("ifindex", ifindex),
                             SD_JSON_BUILD_PAIR_UNSIGNED("flags", 0));
    if (r < 0) {
        if (error) {
            *error = g_error_new(GA_ERROR, GA_ERROR_FAILURE,
                                 "Failed to start browsing: %s",
                                 g_strerror(-r));
        }
        disconnect_from_resolved(browser);
        return FALSE;
    }

    sd_varlink_flush(priv->link);

    /* Wait for initial snapshot (bounded, up to 1s) */
    gint64 deadline = g_get_monotonic_time() + G_TIME_SPAN_SECOND;
    while (!priv->initial_snapshot_done && g_get_monotonic_time() < deadline) {
        sd_varlink_wait(priv->link, 100 * 1000);  /* 100ms */
        int pr = sd_varlink_process(priv->link);
        if (pr < 0)
            break;
    }

    /* Emit all-for-now to indicate initial results are ready */
    g_signal_emit(browser, signals[ALL_FOR_NOW], 0);

    return TRUE;
}
