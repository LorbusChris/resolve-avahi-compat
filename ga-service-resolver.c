/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* ga-service-resolver.c - Source for GaServiceResolver (systemd-resolved compatibility) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <gio/gio.h>
#include <systemd/sd-varlink.h>

#include "ga-service-resolver.h"
#include "ga-entry-group.h"  /* For GaStringList */
#include "ga-error.h"

#define RESOLVED_VARLINK_ADDRESS "/run/systemd/resolve/io.systemd.Resolve"

/* signal enum */
enum {
    FOUND,
    FAILURE,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* properties */
enum {
    PROP_PROTOCOL = 1,
    PROP_IFINDEX,
    PROP_NAME,
    PROP_TYPE,
    PROP_DOMAIN,
    PROP_FLAGS,
    PROP_APROTOCOL
};

struct _GaServiceResolverPrivate {
    GaClient *client;
    GaIfIndex interface;
    GaProtocol protocol;
    GaAddress address;
    uint16_t port;
    char *name;
    char *type;
    char *domain;
    char *host;
    GaProtocol aprotocol;
    GaLookupFlags flags;
    GaStringList *txt;
    gboolean dispose_has_run;
    gboolean resolved;
};

#define GA_SERVICE_RESOLVER_GET_PRIVATE(o) \
    ((GaServiceResolverPrivate *)ga_service_resolver_get_instance_private(o))

G_DEFINE_TYPE_WITH_PRIVATE(GaServiceResolver, ga_service_resolver, G_TYPE_OBJECT)

static void ga_service_resolver_init(GaServiceResolver *obj) {
    GaServiceResolverPrivate *priv = GA_SERVICE_RESOLVER_GET_PRIVATE(obj);

    priv->client = NULL;
    priv->name = NULL;
    priv->type = NULL;
    priv->domain = NULL;
    priv->host = NULL;
    priv->txt = NULL;
    priv->port = 0;
    priv->interface = GA_IF_UNSPEC;
    priv->protocol = GA_PROTOCOL_UNSPEC;
    priv->aprotocol = GA_PROTOCOL_UNSPEC;
    priv->resolved = FALSE;
}

static void ga_service_resolver_dispose(GObject *object);
static void ga_service_resolver_finalize(GObject *object);

static void ga_service_resolver_set_property(GObject *object,
                                             guint property_id,
                                             const GValue *value,
                                             GParamSpec *pspec) {
    GaServiceResolver *resolver = GA_SERVICE_RESOLVER(object);
    GaServiceResolverPrivate *priv = GA_SERVICE_RESOLVER_GET_PRIVATE(resolver);

    switch (property_id) {
        case PROP_PROTOCOL:
            priv->protocol = g_value_get_enum(value);
            break;
        case PROP_APROTOCOL:
            priv->aprotocol = g_value_get_enum(value);
            break;
        case PROP_IFINDEX:
            priv->interface = g_value_get_int(value);
            break;
        case PROP_NAME:
            g_free(priv->name);
            priv->name = g_strdup(g_value_get_string(value));
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

static void ga_service_resolver_get_property(GObject *object,
                                             guint property_id,
                                             GValue *value,
                                             GParamSpec *pspec) {
    GaServiceResolver *resolver = GA_SERVICE_RESOLVER(object);
    GaServiceResolverPrivate *priv = GA_SERVICE_RESOLVER_GET_PRIVATE(resolver);

    switch (property_id) {
        case PROP_APROTOCOL:
            g_value_set_enum(value, priv->aprotocol);
            break;
        case PROP_PROTOCOL:
            g_value_set_enum(value, priv->protocol);
            break;
        case PROP_IFINDEX:
            g_value_set_int(value, priv->interface);
            break;
        case PROP_NAME:
            g_value_set_string(value, priv->name);
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

static void ga_service_resolver_class_init(GaServiceResolverClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GParamSpec *param_spec;

    object_class->dispose = ga_service_resolver_dispose;
    object_class->finalize = ga_service_resolver_finalize;
    object_class->set_property = ga_service_resolver_set_property;
    object_class->get_property = ga_service_resolver_get_property;

    signals[FOUND] =
        g_signal_new("found",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     NULL,  /* Use default marshaller */
                     G_TYPE_NONE, 10,
                     G_TYPE_INT,            /* interface */
                     GA_TYPE_PROTOCOL,      /* protocol */
                     G_TYPE_STRING,         /* name */
                     G_TYPE_STRING,         /* type */
                     G_TYPE_STRING,         /* domain */
                     G_TYPE_STRING,         /* host_name */
                     G_TYPE_POINTER,        /* address (GaAddress*) */
                     G_TYPE_INT,            /* port */
                     G_TYPE_POINTER,        /* txt (GaStringList*) */
                     GA_TYPE_LOOKUP_RESULT_FLAGS);

    signals[FAILURE] =
        g_signal_new("failure",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE, 1, G_TYPE_POINTER);

    param_spec = g_param_spec_enum("protocol", "Protocol",
                                   "Protocol to resolve on",
                                   GA_TYPE_PROTOCOL,
                                   GA_PROTOCOL_UNSPEC,
                                   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_PROTOCOL, param_spec);

    param_spec = g_param_spec_enum("aprotocol", "Address protocol",
                                   "Protocol of the address to be resolved",
                                   GA_TYPE_PROTOCOL,
                                   GA_PROTOCOL_UNSPEC,
                                   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_APROTOCOL, param_spec);

    param_spec = g_param_spec_int("interface", "Interface index",
                                  "Interface to use for resolver",
                                  G_MININT, G_MAXINT,
                                  GA_IF_UNSPEC,
                                  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_IFINDEX, param_spec);

    param_spec = g_param_spec_string("name", "Service name",
                                     "Name to resolve",
                                     NULL,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_NAME, param_spec);

    param_spec = g_param_spec_string("type", "Service type",
                                     "Service type to resolve",
                                     NULL,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_TYPE, param_spec);

    param_spec = g_param_spec_string("domain", "Domain",
                                     "Domain to resolve in",
                                     NULL,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_DOMAIN, param_spec);

    param_spec = g_param_spec_flags("flags", "Lookup flags",
                                    "Resolver lookup flags",
                                    GA_TYPE_LOOKUP_FLAGS,
                                    GA_LOOKUP_NO_FLAGS,
                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_FLAGS, param_spec);
}

static void free_txt_list(GaStringList *list) {
    while (list) {
        GaStringList *next = list->next;
        g_free(list);
        list = next;
    }
}

void ga_service_resolver_dispose(GObject *object) {
    GaServiceResolver *self = GA_SERVICE_RESOLVER(object);
    GaServiceResolverPrivate *priv = GA_SERVICE_RESOLVER_GET_PRIVATE(self);

    if (priv->dispose_has_run)
        return;

    priv->dispose_has_run = TRUE;

    if (priv->client) {
        g_object_unref(priv->client);
        priv->client = NULL;
    }

    if (G_OBJECT_CLASS(ga_service_resolver_parent_class)->dispose)
        G_OBJECT_CLASS(ga_service_resolver_parent_class)->dispose(object);
}

void ga_service_resolver_finalize(GObject *object) {
    GaServiceResolver *self = GA_SERVICE_RESOLVER(object);
    GaServiceResolverPrivate *priv = GA_SERVICE_RESOLVER_GET_PRIVATE(self);

    g_free(priv->name);
    g_free(priv->type);
    g_free(priv->domain);
    g_free(priv->host);
    free_txt_list(priv->txt);

    G_OBJECT_CLASS(ga_service_resolver_parent_class)->finalize(object);
}

/* Parse IP address from services array in ResolveService response */
static gboolean extract_address_from_services(sd_json_variant *services_array,
                                              GaAddress *out_address,
                                              uint16_t *out_port,
                                              GaProtocol preferred_proto) {
    if (!services_array || !sd_json_variant_is_array(services_array))
        return FALSE;

    GaAddress ipv4_addr = { .proto = GA_PROTOCOL_INET };
    GaAddress ipv6_addr = { .proto = GA_PROTOCOL_INET6 };
    gboolean have_ipv4 = FALSE;
    gboolean have_ipv6 = FALSE;
    uint16_t port = 0;

    size_t sn = sd_json_variant_elements(services_array);

    for (size_t si = 0; si < sn; si++) {
        sd_json_variant *srv_entry = sd_json_variant_by_index(services_array, si);
        if (!srv_entry || !sd_json_variant_is_object(srv_entry))
            continue;

        /* Get port from service entry */
        sd_json_variant *port_v = sd_json_variant_by_key(srv_entry, "port");
        if (port_v && sd_json_variant_is_unsigned(port_v)) {
            port = (uint16_t)sd_json_variant_unsigned(port_v);
        }

        sd_json_variant *addr_array = sd_json_variant_by_key(srv_entry, "addresses");
        if (!addr_array || !sd_json_variant_is_array(addr_array))
            continue;

        size_t an = sd_json_variant_elements(addr_array);

        for (size_t ai = 0; ai < an; ai++) {
            sd_json_variant *addr_entry = sd_json_variant_by_index(addr_array, ai);
            if (!addr_entry || !sd_json_variant_is_object(addr_entry))
                continue;

            sd_json_variant *family_v = sd_json_variant_by_key(addr_entry, "family");
            sd_json_variant *address_v = sd_json_variant_by_key(addr_entry, "address");

            if (!family_v || !sd_json_variant_is_integer(family_v))
                continue;
            if (!address_v || !sd_json_variant_is_array(address_v))
                continue;

            int64_t family = sd_json_variant_integer(family_v);
            size_t bn = sd_json_variant_elements(address_v);

            if (family == AF_INET && bn == 4) {
                uint8_t *addr_bytes = (uint8_t *)&ipv4_addr.data.ipv4.address;
                for (size_t bi = 0; bi < 4; bi++) {
                    sd_json_variant *b = sd_json_variant_by_index(address_v, bi);
                    if (b && sd_json_variant_is_unsigned(b)) {
                        addr_bytes[bi] = (uint8_t)sd_json_variant_unsigned(b);
                    }
                }
                have_ipv4 = TRUE;
            } else if (family == AF_INET6 && bn == 16) {
                for (size_t bi = 0; bi < 16; bi++) {
                    sd_json_variant *b = sd_json_variant_by_index(address_v, bi);
                    if (b && sd_json_variant_is_unsigned(b)) {
                        ipv6_addr.data.ipv6.address[bi] = (uint8_t)sd_json_variant_unsigned(b);
                    }
                }
                have_ipv6 = TRUE;
            }
        }
    }

    /* Select address based on preference */
    if (preferred_proto == GA_PROTOCOL_INET && have_ipv4) {
        *out_address = ipv4_addr;
        *out_port = port;
        return TRUE;
    } else if (preferred_proto == GA_PROTOCOL_INET6 && have_ipv6) {
        *out_address = ipv6_addr;
        *out_port = port;
        return TRUE;
    } else if (have_ipv4) {
        *out_address = ipv4_addr;
        *out_port = port;
        return TRUE;
    } else if (have_ipv6) {
        *out_address = ipv6_addr;
        *out_port = port;
        return TRUE;
    }

    return FALSE;
}

/* Resolve task data */
typedef struct {
    GaServiceResolver *resolver;
    char *name;
    char *type;
    char *domain;
    int ifindex;
    GaProtocol aprotocol;
} ResolveTaskData;

static void resolve_task_data_free(ResolveTaskData *data) {
    g_free(data->name);
    g_free(data->type);
    g_free(data->domain);
    g_free(data);
}

static void resolve_task_thread(GTask *task,
                                G_GNUC_UNUSED gpointer source_object,
                                gpointer task_data,
                                G_GNUC_UNUSED GCancellable *cancellable) {
    ResolveTaskData *data = task_data;
    sd_varlink *vl = NULL;
    sd_json_variant *params = NULL;
    sd_json_variant *reply = NULL;
    const char *error_id = NULL;
    int r;

    r = sd_varlink_connect_address(&vl, RESOLVED_VARLINK_ADDRESS);
    if (r < 0) {
        g_task_return_new_error(task, GA_ERROR, GA_ERROR_NO_DAEMON,
                                "Failed to connect to systemd-resolved: %s",
                                g_strerror(-r));
        goto out;
    }

    int family = AF_UNSPEC;
    if (data->aprotocol == GA_PROTOCOL_INET)
        family = AF_INET;
    else if (data->aprotocol == GA_PROTOCOL_INET6)
        family = AF_INET6;

    r = sd_json_buildo(&params,
                       SD_JSON_BUILD_PAIR_STRING("name", data->name),
                       SD_JSON_BUILD_PAIR_STRING("type", data->type),
                       SD_JSON_BUILD_PAIR_STRING("domain", data->domain),
                       SD_JSON_BUILD_PAIR_INTEGER("ifindex", data->ifindex),
                       SD_JSON_BUILD_PAIR_INTEGER("family", family),
                       SD_JSON_BUILD_PAIR_UNSIGNED("flags", 0));
    if (r < 0) {
        g_task_return_new_error(task, GA_ERROR, GA_ERROR_FAILURE,
                                "Failed to build params: %s",
                                g_strerror(-r));
        goto out;
    }

    r = sd_varlink_call(vl,
                        "io.systemd.Resolve.ResolveService",
                        params,
                        &reply,
                        &error_id);
    if (r < 0) {
        g_task_return_new_error(task, GA_ERROR, GA_ERROR_FAILURE,
                                "ResolveService call failed: %s",
                                g_strerror(-r));
        goto out;
    }

    if (error_id) {
        g_task_return_new_error(task, GA_ERROR, GA_ERROR_NOT_FOUND,
                                "ResolveService error: %s", error_id);
        goto out;
    }

    /* Parse the response */
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));

    sd_json_variant *services = sd_json_variant_by_key(reply, "services");
    sd_json_variant *txt = sd_json_variant_by_key(reply, "txt");

    GaAddress address = {0};
    uint16_t port = 0;
    if (extract_address_from_services(services, &address, &port, data->aprotocol)) {
        g_variant_builder_add(&builder, "{sv}", "port",
                              g_variant_new_uint16(port));
        g_variant_builder_add(&builder, "{sv}", "proto",
                              g_variant_new_int32(address.proto));
        if (address.proto == GA_PROTOCOL_INET) {
            g_variant_builder_add(&builder, "{sv}", "address",
                                  g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                                            &address.data.ipv4.address, 4, 1));
        } else {
            g_variant_builder_add(&builder, "{sv}", "address",
                                  g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                                            address.data.ipv6.address, 16, 1));
        }
    }

    /* TXT records as string array */
    if (txt && sd_json_variant_is_array(txt)) {
        GVariantBuilder txt_builder;
        g_variant_builder_init(&txt_builder, G_VARIANT_TYPE("as"));
        size_t n = sd_json_variant_elements(txt);
        for (size_t i = 0; i < n; i++) {
            sd_json_variant *entry = sd_json_variant_by_index(txt, i);
            if (entry && sd_json_variant_is_string(entry)) {
                g_variant_builder_add(&txt_builder, "s",
                                      sd_json_variant_string(entry));
            }
        }
        g_variant_builder_add(&builder, "{sv}", "txt",
                              g_variant_builder_end(&txt_builder));
    }

    g_task_return_pointer(task, g_variant_ref_sink(g_variant_builder_end(&builder)),
                          (GDestroyNotify)g_variant_unref);

out:
    if (params)
        sd_json_variant_unref(params);
    if (vl)
        sd_varlink_unref(vl);
}

static void resolve_complete_cb(GObject *source,
                                GAsyncResult *result,
                                G_GNUC_UNUSED gpointer user_data) {
    GaServiceResolver *resolver = GA_SERVICE_RESOLVER(source);
    GaServiceResolverPrivate *priv = GA_SERVICE_RESOLVER_GET_PRIVATE(resolver);
    GError *error = NULL;

    GVariant *response = g_task_propagate_pointer(G_TASK(result), &error);
    if (!response) {
        g_signal_emit(resolver, signals[FAILURE], 0, error);
        g_error_free(error);
        return;
    }

    /* Extract data from response */
    GVariant *port_v = g_variant_lookup_value(response, "port", G_VARIANT_TYPE_UINT16);
    GVariant *proto_v = g_variant_lookup_value(response, "proto", G_VARIANT_TYPE_INT32);
    GVariant *addr_v = g_variant_lookup_value(response, "address", G_VARIANT_TYPE_BYTESTRING);
    GVariant *txt_v = g_variant_lookup_value(response, "txt", G_VARIANT_TYPE_STRING_ARRAY);

    if (port_v) {
        priv->port = g_variant_get_uint16(port_v);
        g_variant_unref(port_v);
    }

    if (proto_v) {
        priv->address.proto = g_variant_get_int32(proto_v);
        g_variant_unref(proto_v);
    }

    if (addr_v) {
        gsize n_elements;
        const guint8 *data = g_variant_get_fixed_array(addr_v, &n_elements, 1);
        if (priv->address.proto == GA_PROTOCOL_INET && n_elements >= 4) {
            memcpy(&priv->address.data.ipv4.address, data, 4);
        } else if (priv->address.proto == GA_PROTOCOL_INET6 && n_elements >= 16) {
            memcpy(priv->address.data.ipv6.address, data, 16);
        }
        g_variant_unref(addr_v);
    }

    /* Build TXT list */
    if (txt_v) {
        gsize n;
        const gchar **txt_array = g_variant_get_strv(txt_v, &n);
        GaStringList *head = NULL;
        GaStringList *tail = NULL;

        for (gsize i = 0; i < n; i++) {
            size_t len = strlen(txt_array[i]);
            GaStringList *node = g_malloc(sizeof(GaStringList) + len);
            node->next = NULL;
            node->size = len;
            memcpy(node->text, txt_array[i], len + 1);

            if (tail) {
                tail->next = node;
                tail = node;
            } else {
                head = tail = node;
            }
        }
        priv->txt = head;
        g_free(txt_array);
        g_variant_unref(txt_v);
    }

    priv->resolved = TRUE;

    /* Emit found signal */
    GaLookupResultFlags result_flags = GA_LOOKUP_RESULT_MULTICAST;
    g_signal_emit(resolver, signals[FOUND], 0,
                  priv->interface,
                  priv->protocol,
                  priv->name,
                  priv->type,
                  priv->domain,
                  priv->host ? priv->host : "",
                  &priv->address,
                  (gint)priv->port,
                  priv->txt,
                  result_flags);

    g_variant_unref(response);
}

GaServiceResolver *ga_service_resolver_new(GaIfIndex interface,
                                           GaProtocol protocol,
                                           const gchar *name,
                                           const gchar *type,
                                           const gchar *domain,
                                           GaProtocol address_protocol,
                                           GaLookupFlags flags) {
    return g_object_new(GA_TYPE_SERVICE_RESOLVER,
                        "interface", interface,
                        "protocol", protocol,
                        "name", name,
                        "type", type,
                        "domain", domain,
                        "aprotocol", address_protocol,
                        "flags", flags,
                        NULL);
}

gboolean ga_service_resolver_attach(GaServiceResolver *resolver,
                                    GaClient *client,
                                    G_GNUC_UNUSED GError **error) {
    GaServiceResolverPrivate *priv = GA_SERVICE_RESOLVER_GET_PRIVATE(resolver);

    g_return_val_if_fail(IS_GA_SERVICE_RESOLVER(resolver), FALSE);
    g_return_val_if_fail(IS_GA_CLIENT(client), FALSE);

    g_object_ref(client);
    priv->client = client;

    /* Start async resolution.
     * GA_IF_UNSPEC (-1) is passed directly; systemd-resolved normalizes it to 0
     * which means "all mDNS interfaces" (Avahi AVAHI_IF_UNSPEC semantics). */
    ResolveTaskData *data = g_new0(ResolveTaskData, 1);
    data->resolver = resolver;
    data->name = g_strdup(priv->name);
    data->type = g_strdup(priv->type);
    data->domain = g_strdup(priv->domain ? priv->domain : "local");
    data->ifindex = priv->interface;
    data->aprotocol = priv->aprotocol;

    GTask *task = g_task_new(resolver, NULL, resolve_complete_cb, NULL);
    g_task_set_task_data(task, data, (GDestroyNotify)resolve_task_data_free);
    g_task_run_in_thread(task, resolve_task_thread);
    g_object_unref(task);

    return TRUE;
}

gboolean ga_service_resolver_get_address(GaServiceResolver *resolver,
                                         GaAddress *address,
                                         uint16_t *port) {
    GaServiceResolverPrivate *priv = GA_SERVICE_RESOLVER_GET_PRIVATE(resolver);

    if (!priv->resolved || priv->port == 0)
        return FALSE;

    *address = priv->address;
    *port = priv->port;
    return TRUE;
}

gchar *ga_address_snprint(gchar *ret, gsize length, const GaAddress *a) {
    const char *result = NULL;

    if (!ret || length == 0 || !a)
        return NULL;

    if (a->proto == GA_PROTO_INET) {
        result = inet_ntop(AF_INET, &a->data.ipv4.address, ret, length);
    } else if (a->proto == GA_PROTO_INET6) {
        result = inet_ntop(AF_INET6, a->data.ipv6.address, ret, length);
    } else {
        return NULL;
    }

    return result ? ret : NULL;
}

gchar *avahi_address_snprint(gchar *ret, gsize length, const GaAddress *a) {
    return ga_address_snprint(ret, length, a);
}

