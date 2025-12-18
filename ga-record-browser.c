/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* ga-record-browser.c - Source for GaRecordBrowser (systemd-resolved compatibility) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-varlink.h>

#include "ga-record-browser.h"
#include "ga-error.h"

#define RESOLVED_VARLINK_ADDRESS "/run/systemd/resolve/io.systemd.Resolve"

/* DNS record classes */
#define DNS_CLASS_IN 1

/* signal enum */
enum {
    NEW_RECORD,
    REMOVED_RECORD,
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
    PROP_NAME,
    PROP_CLASS,
    PROP_TYPE,
    PROP_FLAGS
};

struct _GaRecordBrowserPrivate {
    GaClient *client;
    sd_varlink *link;
    int varlink_fd;
    GSource *varlink_source;
    GaIfIndex interface;
    GaProtocol protocol;
    char *name;
    guint16 clazz;
    guint16 type;
    GaLookupFlags flags;
    gboolean dispose_has_run;
};

#define GA_RECORD_BROWSER_GET_PRIVATE(o) \
    ((GaRecordBrowserPrivate *)ga_record_browser_get_instance_private(o))

G_DEFINE_TYPE_WITH_PRIVATE(GaRecordBrowser, ga_record_browser, G_TYPE_OBJECT)

static void ga_record_browser_init(GaRecordBrowser *obj) {
    GaRecordBrowserPrivate *priv = GA_RECORD_BROWSER_GET_PRIVATE(obj);

    priv->client = NULL;
    priv->link = NULL;
    priv->varlink_fd = -1;
    priv->varlink_source = NULL;
    priv->name = NULL;
    priv->clazz = DNS_CLASS_IN;
    priv->type = 0;
    priv->interface = GA_IF_UNSPEC;
    priv->protocol = GA_PROTOCOL_UNSPEC;
}

static void ga_record_browser_dispose(GObject *object);
static void ga_record_browser_finalize(GObject *object);

static void ga_record_browser_set_property(GObject *object,
                                           guint property_id,
                                           const GValue *value,
                                           GParamSpec *pspec) {
    GaRecordBrowser *browser = GA_RECORD_BROWSER(object);
    GaRecordBrowserPrivate *priv = GA_RECORD_BROWSER_GET_PRIVATE(browser);

    switch (property_id) {
        case PROP_PROTOCOL:
            priv->protocol = g_value_get_enum(value);
            break;
        case PROP_IFINDEX:
            priv->interface = g_value_get_int(value);
            break;
        case PROP_NAME:
            g_free(priv->name);
            priv->name = g_strdup(g_value_get_string(value));
            break;
        case PROP_CLASS:
            priv->clazz = (guint16)g_value_get_uint(value);
            break;
        case PROP_TYPE:
            priv->type = (guint16)g_value_get_uint(value);
            break;
        case PROP_FLAGS:
            priv->flags = g_value_get_flags(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ga_record_browser_get_property(GObject *object,
                                           guint property_id,
                                           GValue *value,
                                           GParamSpec *pspec) {
    GaRecordBrowser *browser = GA_RECORD_BROWSER(object);
    GaRecordBrowserPrivate *priv = GA_RECORD_BROWSER_GET_PRIVATE(browser);

    switch (property_id) {
        case PROP_PROTOCOL:
            g_value_set_enum(value, priv->protocol);
            break;
        case PROP_IFINDEX:
            g_value_set_int(value, priv->interface);
            break;
        case PROP_NAME:
            g_value_set_string(value, priv->name);
            break;
        case PROP_CLASS:
            g_value_set_uint(value, priv->clazz);
            break;
        case PROP_TYPE:
            g_value_set_uint(value, priv->type);
            break;
        case PROP_FLAGS:
            g_value_set_flags(value, priv->flags);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ga_record_browser_class_init(GaRecordBrowserClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GParamSpec *param_spec;

    object_class->dispose = ga_record_browser_dispose;
    object_class->finalize = ga_record_browser_finalize;
    object_class->set_property = ga_record_browser_set_property;
    object_class->get_property = ga_record_browser_get_property;

    signals[NEW_RECORD] =
        g_signal_new("new-record",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     NULL,
                     G_TYPE_NONE, 7,
                     G_TYPE_INT,           /* interface */
                     GA_TYPE_PROTOCOL,     /* protocol */
                     G_TYPE_STRING,        /* name */
                     G_TYPE_UINT,          /* class */
                     G_TYPE_UINT,          /* type */
                     G_TYPE_POINTER,       /* rdata */
                     G_TYPE_UINT);         /* size */

    signals[REMOVED_RECORD] =
        g_signal_new("removed-record",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     NULL,
                     G_TYPE_NONE, 7,
                     G_TYPE_INT,
                     GA_TYPE_PROTOCOL,
                     G_TYPE_STRING,
                     G_TYPE_UINT,
                     G_TYPE_UINT,
                     G_TYPE_POINTER,
                     G_TYPE_UINT);

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
                                   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_PROTOCOL, param_spec);

    param_spec = g_param_spec_int("interface", "Interface index",
                                  "Interface to use for browsing",
                                  G_MININT, G_MAXINT,
                                  GA_IF_UNSPEC,
                                  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_IFINDEX, param_spec);

    param_spec = g_param_spec_string("name", "Record name",
                                     "DNS name to browse for",
                                     NULL,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_NAME, param_spec);

    param_spec = g_param_spec_uint("class", "DNS class",
                                   "DNS record class",
                                   0, G_MAXUINT16,
                                   DNS_CLASS_IN,
                                   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_CLASS, param_spec);

    param_spec = g_param_spec_uint("type", "DNS type",
                                   "DNS record type",
                                   0, G_MAXUINT16,
                                   0,
                                   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_TYPE, param_spec);

    param_spec = g_param_spec_flags("flags", "Lookup flags",
                                    "Browser lookup flags",
                                    GA_TYPE_LOOKUP_FLAGS,
                                    GA_LOOKUP_NO_FLAGS,
                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_FLAGS, param_spec);
}

static void disconnect_from_resolved(GaRecordBrowser *browser) {
    GaRecordBrowserPrivate *priv = GA_RECORD_BROWSER_GET_PRIVATE(browser);

    if (priv->varlink_source) {
        g_source_destroy(priv->varlink_source);
        g_source_unref(priv->varlink_source);
        priv->varlink_source = NULL;
    }

    if (priv->link) {
        sd_varlink_unref(priv->link);
        priv->link = NULL;
    }

    priv->varlink_fd = -1;
}

void ga_record_browser_dispose(GObject *object) {
    GaRecordBrowser *self = GA_RECORD_BROWSER(object);
    GaRecordBrowserPrivate *priv = GA_RECORD_BROWSER_GET_PRIVATE(self);

    if (priv->dispose_has_run)
        return;

    priv->dispose_has_run = TRUE;

    disconnect_from_resolved(self);

    if (priv->client) {
        g_object_unref(priv->client);
        priv->client = NULL;
    }

    if (G_OBJECT_CLASS(ga_record_browser_parent_class)->dispose)
        G_OBJECT_CLASS(ga_record_browser_parent_class)->dispose(object);
}

void ga_record_browser_finalize(GObject *object) {
    GaRecordBrowser *self = GA_RECORD_BROWSER(object);
    GaRecordBrowserPrivate *priv = GA_RECORD_BROWSER_GET_PRIVATE(self);

    g_free(priv->name);

    G_OBJECT_CLASS(ga_record_browser_parent_class)->finalize(object);
}

GaRecordBrowser *ga_record_browser_new(const gchar *name, guint16 type) {
    return ga_record_browser_new_full(GA_IF_UNSPEC, GA_PROTOCOL_UNSPEC,
                                      name, DNS_CLASS_IN, type, GA_LOOKUP_NO_FLAGS);
}

GaRecordBrowser *ga_record_browser_new_full(GaIfIndex interface,
                                            GaProtocol protocol,
                                            const gchar *name,
                                            guint16 clazz,
                                            guint16 type,
                                            GaLookupFlags flags) {
    return g_object_new(GA_TYPE_RECORD_BROWSER,
                        "interface", interface,
                        "protocol", protocol,
                        "name", name,
                        "class", (guint)clazz,
                        "type", (guint)type,
                        "flags", flags,
                        NULL);
}

gboolean ga_record_browser_attach(GaRecordBrowser *browser,
                                  GaClient *client,
                                  GError **error) {
    GaRecordBrowserPrivate *priv = GA_RECORD_BROWSER_GET_PRIVATE(browser);
    int r;

    g_return_val_if_fail(IS_GA_RECORD_BROWSER(browser), FALSE);
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

    /* Use ResolveRecord for DNS record browsing.
     * Note: systemd-resolved doesn't have a streaming record browser like Avahi,
     * so we do a one-shot query and emit results.
     * GA_IF_UNSPEC (-1) is passed directly; systemd-resolved normalizes it to 0
     * which means "all mDNS interfaces" (Avahi AVAHI_IF_UNSPEC semantics). */
    sd_json_variant *params = NULL;
    sd_json_variant *reply = NULL;
    const char *error_id = NULL;

    int ifindex = priv->interface;

    r = sd_json_buildo(&params,
                       SD_JSON_BUILD_PAIR_INTEGER("ifindex", ifindex),
                       SD_JSON_BUILD_PAIR_STRING("name", priv->name),
                       SD_JSON_BUILD_PAIR_INTEGER("class", priv->clazz),
                       SD_JSON_BUILD_PAIR_INTEGER("type", priv->type),
                       SD_JSON_BUILD_PAIR_UNSIGNED("flags", 0));
    if (r < 0) {
        if (error) {
            *error = g_error_new(GA_ERROR, GA_ERROR_FAILURE,
                                 "Failed to build params: %s",
                                 g_strerror(-r));
        }
        disconnect_from_resolved(browser);
        return FALSE;
    }

    r = sd_varlink_call(priv->link,
                        "io.systemd.Resolve.ResolveRecord",
                        params,
                        &reply,
                        &error_id);
    sd_json_variant_unref(params);

    if (r < 0 || error_id) {
        if (error) {
            *error = g_error_new(GA_ERROR, GA_ERROR_NOT_FOUND,
                                 "ResolveRecord failed: %s",
                                 error_id ? error_id : g_strerror(-r));
        }
        disconnect_from_resolved(browser);
        return FALSE;
    }

    /* Parse and emit record results */
    sd_json_variant *rrs = sd_json_variant_by_key(reply, "rrs");
    if (rrs && sd_json_variant_is_array(rrs)) {
        size_t n = sd_json_variant_elements(rrs);
        for (size_t i = 0; i < n; i++) {
            sd_json_variant *rr = sd_json_variant_by_index(rrs, i);
            if (!rr || !sd_json_variant_is_object(rr))
                continue;

            sd_json_variant *rdata_v = sd_json_variant_by_key(rr, "rdata");
            if (!rdata_v || !sd_json_variant_is_array(rdata_v))
                continue;

            /* Convert rdata array to bytes */
            size_t rdata_len = sd_json_variant_elements(rdata_v);
            guint8 *rdata = g_malloc(rdata_len);
            for (size_t j = 0; j < rdata_len; j++) {
                sd_json_variant *b = sd_json_variant_by_index(rdata_v, j);
                if (b && sd_json_variant_is_unsigned(b)) {
                    rdata[j] = (guint8)sd_json_variant_unsigned(b);
                }
            }

            g_signal_emit(browser, signals[NEW_RECORD], 0,
                          ifindex,
                          priv->protocol,
                          priv->name,
                          (guint)priv->clazz,
                          (guint)priv->type,
                          rdata,
                          (guint)rdata_len);

            g_free(rdata);
        }
    }

    g_signal_emit(browser, signals[ALL_FOR_NOW], 0);

    disconnect_from_resolved(browser);
    return TRUE;
}
