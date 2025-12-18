/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * ga-entry-group.c - Source for GaEntryGroup (systemd-resolved compatibility)
 *
 * Service publishing is implemented via .dnssd files in /run/systemd/dnssd/
 * as documented in systemd.dnssd(5). After creating/modifying files,
 * systemd-resolved is signaled to reload its configuration.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gio/gio.h>

#include "ga-entry-group.h"
#include "ga-error.h"

#define DNSSD_RUNTIME_DIR "/run/systemd/dnssd"

/* signal enum */
enum {
    STATE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* properties */
enum {
    PROP_STATE = 1
};

struct _GaEntryGroupPrivate {
    GaEntryGroupState state;
    GaClient *client;
    GHashTable *services;
    GPtrArray *created_files;  /* Track .dnssd files we created */
    gboolean dispose_has_run;
};

typedef struct {
    GaEntryGroupService public;
    GaEntryGroup *group;
    gboolean frozen;
    GHashTable *txt_entries;
    gchar *dnssd_filename;  /* Filename in DNSSD_RUNTIME_DIR */
} GaEntryGroupServicePrivate;

#define GA_ENTRY_GROUP_GET_PRIVATE(o) \
    ((GaEntryGroupPrivate *)ga_entry_group_get_instance_private(o))

G_DEFINE_TYPE_WITH_PRIVATE(GaEntryGroup, ga_entry_group, G_TYPE_OBJECT)

/* Forward declarations for helper functions */
static gchar *generate_dnssd_content(GaEntryGroupServicePrivate *service);
static void signal_resolved_reload(void);
static void cleanup_dnssd_files(GaEntryGroupPrivate *priv);

GType ga_entry_group_state_get_type(void) {
    static GType type = 0;
    if (G_UNLIKELY(type == 0)) {
        static const GEnumValue values[] = {
            { GA_ENTRY_GROUP_STATE_UNCOMMITED, "GA_ENTRY_GROUP_STATE_UNCOMMITED", "uncommitted" },
            { GA_ENTRY_GROUP_STATE_REGISTERING, "GA_ENTRY_GROUP_STATE_REGISTERING", "registering" },
            { GA_ENTRY_GROUP_STATE_ESTABLISHED, "GA_ENTRY_GROUP_STATE_ESTABLISHED", "established" },
            { GA_ENTRY_GROUP_STATE_COLLISION, "GA_ENTRY_GROUP_STATE_COLLISION", "collision" },
            { GA_ENTRY_GROUP_STATE_FAILURE, "GA_ENTRY_GROUP_STATE_FAILURE", "failure" },
            { 0, NULL, NULL }
        };
        type = g_enum_register_static("GaEntryGroupState", values);
    }
    return type;
}

static void free_service(gpointer data) {
    GaEntryGroupService *s = (GaEntryGroupService *)data;
    GaEntryGroupServicePrivate *p = (GaEntryGroupServicePrivate *)s;

    g_free(s->name);
    g_free(s->type);
    g_free(s->domain);
    g_free(s->host);
    if (p->txt_entries)
        g_hash_table_destroy(p->txt_entries);
    g_free(p->dnssd_filename);
    g_free(s);
}

static void ga_entry_group_init(GaEntryGroup *obj) {
    GaEntryGroupPrivate *priv = GA_ENTRY_GROUP_GET_PRIVATE(obj);

    priv->state = GA_ENTRY_GROUP_STATE_UNCOMMITED;
    priv->client = NULL;
    priv->services = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                           NULL, free_service);
    priv->created_files = g_ptr_array_new_with_free_func(g_free);
}

static void ga_entry_group_dispose(GObject *object);
static void ga_entry_group_finalize(GObject *object);

static void ga_entry_group_get_property(GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec) {
    GaEntryGroup *group = GA_ENTRY_GROUP(object);
    GaEntryGroupPrivate *priv = GA_ENTRY_GROUP_GET_PRIVATE(group);

    switch (property_id) {
        case PROP_STATE:
            g_value_set_enum(value, priv->state);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static GQuark detail_for_state(GaEntryGroupState state) {
    static struct {
        GaEntryGroupState state;
        const gchar *name;
        GQuark quark;
    } states[] = {
        { GA_ENTRY_GROUP_STATE_UNCOMMITED, "uncommitted", 0 },
        { GA_ENTRY_GROUP_STATE_REGISTERING, "registering", 0 },
        { GA_ENTRY_GROUP_STATE_ESTABLISHED, "established", 0 },
        { GA_ENTRY_GROUP_STATE_COLLISION, "collision", 0 },
        { GA_ENTRY_GROUP_STATE_FAILURE, "failure", 0 },
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

static void ga_entry_group_class_init(GaEntryGroupClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GParamSpec *param_spec;

    object_class->dispose = ga_entry_group_dispose;
    object_class->finalize = ga_entry_group_finalize;
    object_class->get_property = ga_entry_group_get_property;

    param_spec = g_param_spec_enum("state", "Entry Group state",
                                   "The state of the entry group",
                                   GA_TYPE_ENTRY_GROUP_STATE,
                                   GA_ENTRY_GROUP_STATE_UNCOMMITED,
                                   G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_STATE, param_spec);

    signals[STATE_CHANGED] =
        g_signal_new("state-changed",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__ENUM,
                     G_TYPE_NONE, 1, GA_TYPE_ENTRY_GROUP_STATE);
}

/* Delete all .dnssd files we created and signal reload */
static void cleanup_dnssd_files(GaEntryGroupPrivate *priv) {
    if (!priv->created_files || priv->created_files->len == 0)
        return;

    for (guint i = 0; i < priv->created_files->len; i++) {
        const gchar *filepath = g_ptr_array_index(priv->created_files, i);
        if (unlink(filepath) != 0 && errno != ENOENT) {
            g_warning("Failed to remove .dnssd file %s: %s", filepath, g_strerror(errno));
        }
    }
    g_ptr_array_set_size(priv->created_files, 0);

    /* Signal systemd-resolved to reload via D-Bus */
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if (bus) {
        g_dbus_connection_call_sync(bus,
                                     "org.freedesktop.resolve1",
                                     "/org/freedesktop/resolve1",
                                     "org.freedesktop.resolve1.Manager",
                                     "ReloadDNSSD",
                                     NULL,
                                     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     NULL);
        g_object_unref(bus);
    }
}

void ga_entry_group_dispose(GObject *object) {
    GaEntryGroup *self = GA_ENTRY_GROUP(object);
    GaEntryGroupPrivate *priv = GA_ENTRY_GROUP_GET_PRIVATE(self);

    if (priv->dispose_has_run)
        return;

    priv->dispose_has_run = TRUE;

    /* Clean up any published services */
    cleanup_dnssd_files(priv);

    if (priv->client) {
        g_object_unref(priv->client);
        priv->client = NULL;
    }

    if (G_OBJECT_CLASS(ga_entry_group_parent_class)->dispose)
        G_OBJECT_CLASS(ga_entry_group_parent_class)->dispose(object);
}

void ga_entry_group_finalize(GObject *object) {
    GaEntryGroup *self = GA_ENTRY_GROUP(object);
    GaEntryGroupPrivate *priv = GA_ENTRY_GROUP_GET_PRIVATE(self);

    if (priv->services) {
        g_hash_table_destroy(priv->services);
        priv->services = NULL;
    }

    if (priv->created_files) {
        g_ptr_array_free(priv->created_files, TRUE);
        priv->created_files = NULL;
    }

    G_OBJECT_CLASS(ga_entry_group_parent_class)->finalize(object);
}

GaEntryGroup *ga_entry_group_new(void) {
    return g_object_new(GA_TYPE_ENTRY_GROUP, NULL);
}

gboolean ga_entry_group_attach(GaEntryGroup *group,
                               GaClient *client,
                               G_GNUC_UNUSED GError **error) {
    GaEntryGroupPrivate *priv = GA_ENTRY_GROUP_GET_PRIVATE(group);

    g_return_val_if_fail(IS_GA_ENTRY_GROUP(group), FALSE);
    g_return_val_if_fail(IS_GA_CLIENT(client), FALSE);

    g_object_ref(client);
    priv->client = client;

    /* Note: systemd-resolved doesn't support publishing via varlink yet.
     * This would require D-Bus integration or an alternative approach. */
    g_warning("ga_entry_group_attach: Service publishing is not supported "
              "by systemd-resolved's varlink API. Consider using D-Bus or "
              "an alternative mDNS responder for publishing.");

    return TRUE;
}

GaEntryGroupService *ga_entry_group_add_service_strlist(GaEntryGroup *group,
                                                        const gchar *name,
                                                        const gchar *type,
                                                        guint16 port,
                                                        GError **error,
                                                        GaStringList *txt) {
    return ga_entry_group_add_service_full_strlist(group, GA_IF_UNSPEC,
                                                   GA_PROTOCOL_UNSPEC, 0,
                                                   name, type, NULL, NULL,
                                                   port, error, txt);
}

GaEntryGroupService *ga_entry_group_add_service_full_strlist(GaEntryGroup *group,
                                                             GaIfIndex interface,
                                                             GaProtocol protocol,
                                                             GaPublishFlags flags,
                                                             const gchar *name,
                                                             const gchar *type,
                                                             const gchar *domain,
                                                             const gchar *host,
                                                             guint16 port,
                                                             G_GNUC_UNUSED GError **error,
                                                             GaStringList *txt) {
    GaEntryGroupPrivate *priv = GA_ENTRY_GROUP_GET_PRIVATE(group);
    GaEntryGroupServicePrivate *service;

    /* Create the service structure (stored locally, not actually published) */
    service = g_new0(GaEntryGroupServicePrivate, 1);
    service->public.interface = interface;
    service->public.protocol = protocol;
    service->public.flags = flags;
    service->public.name = g_strdup(name);
    service->public.type = g_strdup(type);
    service->public.domain = g_strdup(domain);
    service->public.host = g_strdup(host);
    service->public.port = port;
    service->group = group;
    service->frozen = FALSE;
    service->txt_entries = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                 g_free, g_free);

    /* Copy TXT records */
    for (GaStringList *t = txt; t != NULL; t = t->next) {
        /* Parse "key=value" format */
        const gchar *s = (const gchar *)t->text;
        const gchar *eq = strchr(s, '=');
        if (eq) {
            gchar *key = g_strndup(s, eq - s);
            gchar *value = g_strdup(eq + 1);
            g_hash_table_insert(service->txt_entries, key, value);
        } else {
            g_hash_table_insert(service->txt_entries, g_strdup(s), NULL);
        }
    }

    g_hash_table_insert(priv->services, service, service);

    /* Service will be published when ga_entry_group_commit() is called */
    return (GaEntryGroupService *)service;
}

GaEntryGroupService *ga_entry_group_add_service(GaEntryGroup *group,
                                                const gchar *name,
                                                const gchar *type,
                                                guint16 port,
                                                GError **error, ...) {
    /* For simplicity, not parsing varargs here */
    return ga_entry_group_add_service_full_strlist(group,
                                                   GA_IF_UNSPEC,
                                                   GA_PROTOCOL_UNSPEC,
                                                   0,
                                                   name, type,
                                                   NULL, NULL,
                                                   port, error, NULL);
}

GaEntryGroupService *ga_entry_group_add_service_full(GaEntryGroup *group,
                                                     GaIfIndex interface,
                                                     GaProtocol protocol,
                                                     GaPublishFlags flags,
                                                     const gchar *name,
                                                     const gchar *type,
                                                     const gchar *domain,
                                                     const gchar *host,
                                                     guint16 port,
                                                     GError **error, ...) {
    return ga_entry_group_add_service_full_strlist(group,
                                                   interface, protocol,
                                                   flags,
                                                   name, type,
                                                   domain, host,
                                                   port, error, NULL);
}

gboolean ga_entry_group_add_record(GaEntryGroup *group,
                                   GaPublishFlags flags,
                                   const gchar *name,
                                   guint16 type,
                                   guint32 ttl,
                                   const void *rdata,
                                   gsize size,
                                   GError **error) {
    return ga_entry_group_add_record_full(group,
                                          GA_IF_UNSPEC, GA_PROTOCOL_UNSPEC,
                                          flags, name, 1 /* DNS_CLASS_IN */,
                                          type, ttl, rdata, size, error);
}

gboolean ga_entry_group_add_record_full(G_GNUC_UNUSED GaEntryGroup *group,
                                        G_GNUC_UNUSED GaIfIndex interface,
                                        G_GNUC_UNUSED GaProtocol protocol,
                                        G_GNUC_UNUSED GaPublishFlags flags,
                                        G_GNUC_UNUSED const gchar *name,
                                        G_GNUC_UNUSED guint16 clazz,
                                        G_GNUC_UNUSED guint16 type,
                                        G_GNUC_UNUSED guint32 ttl,
                                        G_GNUC_UNUSED const void *rdata,
                                        G_GNUC_UNUSED gsize size,
                                        GError **error) {
    if (error) {
        *error = g_error_new(GA_ERROR, GA_ERROR_NOT_SUPPORTED,
                             "Record publishing not supported by systemd-resolved varlink API");
    }
    return FALSE;
}

void ga_entry_group_service_freeze(GaEntryGroupService *service) {
    GaEntryGroupServicePrivate *priv = (GaEntryGroupServicePrivate *)service;
    priv->frozen = TRUE;
}

gboolean ga_entry_group_service_set(GaEntryGroupService *service,
                                    const gchar *key,
                                    const gchar *value,
                                    G_GNUC_UNUSED GError **error) {
    GaEntryGroupServicePrivate *priv = (GaEntryGroupServicePrivate *)service;

    g_hash_table_insert(priv->txt_entries, g_strdup(key),
                        value ? g_strdup(value) : NULL);

    return TRUE;
}

gboolean ga_entry_group_service_set_arbitrary(GaEntryGroupService *service,
                                              const gchar *key,
                                              const guint8 *value,
                                              gsize size,
                                              G_GNUC_UNUSED GError **error) {
    GaEntryGroupServicePrivate *priv = (GaEntryGroupServicePrivate *)service;

    gchar *value_copy = g_strndup((const gchar *)value, size);
    g_hash_table_insert(priv->txt_entries, g_strdup(key), value_copy);

    return TRUE;
}

gboolean ga_entry_group_service_remove_key(GaEntryGroupService *service,
                                           const gchar *key,
                                           G_GNUC_UNUSED GError **error) {
    GaEntryGroupServicePrivate *priv = (GaEntryGroupServicePrivate *)service;

    g_hash_table_remove(priv->txt_entries, key);

    return TRUE;
}

gboolean ga_entry_group_service_thaw(GaEntryGroupService *service,
                                     GError **error) {
    GaEntryGroupServicePrivate *priv = (GaEntryGroupServicePrivate *)service;
    GaEntryGroupPrivate *group_priv = GA_ENTRY_GROUP_GET_PRIVATE(priv->group);

    priv->frozen = FALSE;

    /* If the group is already established, update the .dnssd file */
    if (group_priv->state == GA_ENTRY_GROUP_STATE_ESTABLISHED && priv->dnssd_filename) {
        gchar *filepath = g_build_filename(DNSSD_RUNTIME_DIR, priv->dnssd_filename, NULL);
        gchar *content = generate_dnssd_content(priv);

        GError *write_error = NULL;
        if (!g_file_set_contents(filepath, content, -1, &write_error)) {
            g_warning("Failed to update %s: %s", filepath, write_error->message);
            if (error) {
                *error = g_error_new(GA_ERROR, GA_ERROR_FAILURE,
                                     "Failed to update .dnssd file: %s",
                                     write_error->message);
            }
            g_error_free(write_error);
            g_free(filepath);
            g_free(content);
            return FALSE;
        }

        g_free(filepath);
        g_free(content);

        /* Signal reload */
        signal_resolved_reload();
    }

    return TRUE;
}

/* Generate a sanitized filename for a .dnssd file */
static gchar *generate_dnssd_filename(const gchar *name, const gchar *type) {
    GString *filename = g_string_new(NULL);

    /* Use name and type to create a unique filename */
    for (const gchar *p = name; *p; p++) {
        if (g_ascii_isalnum(*p) || *p == '-' || *p == '_') {
            g_string_append_c(filename, *p);
        } else if (*p == ' ') {
            g_string_append_c(filename, '_');
        }
    }
    g_string_append_c(filename, '-');

    /* Process type (e.g., _http._tcp) */
    for (const gchar *p = type; *p; p++) {
        if (g_ascii_isalnum(*p) || *p == '-') {
            g_string_append_c(filename, *p);
        } else if (*p == '_' || *p == '.') {
            g_string_append_c(filename, '-');
        }
    }

    g_string_append(filename, ".dnssd");

    return g_string_free(filename, FALSE);
}

/* Generate .dnssd file content for a service */
static gchar *generate_dnssd_content(GaEntryGroupServicePrivate *service) {
    GString *content = g_string_new("[Service]\n");

    /* Name= */
    g_string_append_printf(content, "Name=%s\n", service->public.name);

    /* Type= (systemd expects just the type like "_http._tcp") */
    g_string_append_printf(content, "Type=%s\n", service->public.type);

    /* Port= */
    g_string_append_printf(content, "Port=%u\n", service->public.port);

    /* TxtText= for each TXT record */
    if (service->txt_entries && g_hash_table_size(service->txt_entries) > 0) {
        GHashTableIter iter;
        gpointer key, value;

        g_hash_table_iter_init(&iter, service->txt_entries);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            if (value) {
                g_string_append_printf(content, "TxtText=%s=%s\n",
                                       (const gchar *)key,
                                       (const gchar *)value);
            } else {
                g_string_append_printf(content, "TxtText=%s\n",
                                       (const gchar *)key);
            }
        }
    }

    return g_string_free(content, FALSE);
}

/* Ensure the runtime directory exists */
static gboolean ensure_dnssd_dir(GError **error) {
    if (g_mkdir_with_parents(DNSSD_RUNTIME_DIR, 0755) != 0) {
        if (error) {
            *error = g_error_new(GA_ERROR, GA_ERROR_FAILURE,
                                 "Failed to create %s: %s",
                                 DNSSD_RUNTIME_DIR, g_strerror(errno));
        }
        return FALSE;
    }
    return TRUE;
}

/* Signal systemd-resolved to reload DNS-SD configuration */
static void signal_resolved_reload(void) {
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if (bus) {
        GError *err = NULL;
        g_dbus_connection_call_sync(bus,
                                     "org.freedesktop.resolve1",
                                     "/org/freedesktop/resolve1",
                                     "org.freedesktop.resolve1.Manager",
                                     "ReloadDNSSD",
                                     NULL,
                                     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     &err);
        if (err) {
            g_debug("ReloadDNSSD call failed (may not be supported): %s", err->message);
            g_error_free(err);
        }
        g_object_unref(bus);
    }
}

gboolean ga_entry_group_commit(GaEntryGroup *group, GError **error) {
    GaEntryGroupPrivate *priv = GA_ENTRY_GROUP_GET_PRIVATE(group);
    GHashTableIter iter;
    gpointer key, value;
    gboolean success = TRUE;

    /* Transition to registering state */
    priv->state = GA_ENTRY_GROUP_STATE_REGISTERING;
    g_signal_emit(group, signals[STATE_CHANGED],
                  detail_for_state(priv->state), priv->state);

    /* Ensure the directory exists */
    if (!ensure_dnssd_dir(error)) {
        priv->state = GA_ENTRY_GROUP_STATE_FAILURE;
        g_signal_emit(group, signals[STATE_CHANGED],
                      detail_for_state(priv->state), priv->state);
        return FALSE;
    }

    /* Write .dnssd files for each service */
    g_hash_table_iter_init(&iter, priv->services);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        GaEntryGroupServicePrivate *service = (GaEntryGroupServicePrivate *)value;

        /* Generate filename and content */
        gchar *filename = generate_dnssd_filename(service->public.name, service->public.type);
        gchar *filepath = g_build_filename(DNSSD_RUNTIME_DIR, filename, NULL);
        gchar *content = generate_dnssd_content(service);

        /* Write the file */
        GError *write_error = NULL;
        if (!g_file_set_contents(filepath, content, -1, &write_error)) {
            g_warning("Failed to write %s: %s", filepath, write_error->message);
            if (error && !*error) {
                *error = g_error_new(GA_ERROR, GA_ERROR_FAILURE,
                                     "Failed to write .dnssd file: %s",
                                     write_error->message);
            }
            g_error_free(write_error);
            success = FALSE;
        } else {
            /* Track the file for cleanup */
            g_ptr_array_add(priv->created_files, g_strdup(filepath));
            service->dnssd_filename = g_strdup(filename);
            g_debug("Created DNS-SD service file: %s", filepath);
        }

        g_free(filename);
        g_free(filepath);
        g_free(content);

        if (!success)
            break;
    }

    if (success) {
        /* Signal systemd-resolved to reload */
        signal_resolved_reload();

        priv->state = GA_ENTRY_GROUP_STATE_ESTABLISHED;
        g_signal_emit(group, signals[STATE_CHANGED],
                      detail_for_state(priv->state), priv->state);
    } else {
        /* Clean up any files we created on failure */
        cleanup_dnssd_files(priv);

        priv->state = GA_ENTRY_GROUP_STATE_FAILURE;
        g_signal_emit(group, signals[STATE_CHANGED],
                      detail_for_state(priv->state), priv->state);
    }

    return success;
}

gboolean ga_entry_group_reset(GaEntryGroup *group, G_GNUC_UNUSED GError **error) {
    GaEntryGroupPrivate *priv = GA_ENTRY_GROUP_GET_PRIVATE(group);

    /* Clean up .dnssd files */
    cleanup_dnssd_files(priv);

    g_hash_table_remove_all(priv->services);

    priv->state = GA_ENTRY_GROUP_STATE_UNCOMMITED;
    g_signal_emit(group, signals[STATE_CHANGED],
                  detail_for_state(priv->state), priv->state);

    return TRUE;
}

/* String list helpers */
GaStringList *ga_string_list_new(const gchar *txt, ...) {
    GaStringList *head = NULL;
    GaStringList *tail = NULL;
    va_list ap;
    const gchar *s;

    if (!txt)
        return NULL;

    s = txt;
    va_start(ap, txt);

    do {
        size_t len = strlen(s);
        GaStringList *node = g_malloc(sizeof(GaStringList) + len);
        node->next = NULL;
        node->size = len;
        memcpy(node->text, s, len + 1);

        if (tail) {
            tail->next = node;
            tail = node;
        } else {
            head = tail = node;
        }

        s = va_arg(ap, const gchar *);
    } while (s != NULL);

    va_end(ap);
    return head;
}

GaStringList *ga_string_list_new_from_array(const gchar **array, gint length) {
    GaStringList *head = NULL;
    GaStringList *tail = NULL;

    if (!array)
        return NULL;

    for (gint i = 0; (length < 0 && array[i]) || i < length; i++) {
        const gchar *s = array[i];
        if (!s)
            break;

        size_t len = strlen(s);
        GaStringList *node = g_malloc(sizeof(GaStringList) + len);
        node->next = NULL;
        node->size = len;
        memcpy(node->text, s, len + 1);

        if (tail) {
            tail->next = node;
            tail = node;
        } else {
            head = tail = node;
        }
    }

    return head;
}

void ga_string_list_free(GaStringList *list) {
    while (list) {
        GaStringList *next = list->next;
        g_free(list);
        list = next;
    }
}

/**
 * ga_string_list_find:
 * @list: The string list to search
 * @key: The key prefix to find (without '=')
 *
 * Find the first entry in the string list that starts with the given key
 * followed by '='. For example, if key is "fn", it will match "fn=SomeName".
 *
 * Returns: (nullable): The matching string list node, or NULL if not found
 */
GaStringList *ga_string_list_find(GaStringList *list, const gchar *key) {
    gsize key_len;

    if (!list || !key)
        return NULL;

    key_len = strlen(key);

    while (list) {
        /* Check if this entry starts with "key=" */
        if (list->size > key_len &&
            list->text[key_len] == '=' &&
            strncmp((const char *)list->text, key, key_len) == 0) {
            return list;
        }
        list = list->next;
    }

    return NULL;
}

/**
 * ga_string_list_get_pair:
 * @list: A string list node
 * @key: (out) (optional) (transfer full): Return location for the key
 * @value: (out) (optional) (transfer full): Return location for the value
 * @size: (out) (optional): Return location for the value size
 *
 * Parse a "key=value" string list entry into its components.
 *
 * Returns: 0 on success, -1 on failure
 */
gint ga_string_list_get_pair(GaStringList *list, gchar **key, gchar **value, gsize *size) {
    gchar *eq;
    gsize key_len;

    if (!list)
        return -1;

    /* Find the '=' separator */
    eq = memchr(list->text, '=', list->size);

    if (eq) {
        key_len = eq - (gchar *)list->text;

        if (key)
            *key = g_strndup((const gchar *)list->text, key_len);

        if (value)
            *value = g_strdup(eq + 1);

        if (size)
            *size = list->size - key_len - 1;
    } else {
        /* No '=' found, entire entry is the key with empty value */
        if (key)
            *key = g_strndup((const gchar *)list->text, list->size);

        if (value)
            *value = g_strdup("");

        if (size)
            *size = 0;
    }

    return 0;
}

/*
 * Avahi-compatible function exports.
 * These are real function symbols (not just macros) for binary compatibility
 * with applications compiled against Avahi headers.
 */

GaStringList *avahi_string_list_new(const gchar *txt, ...) {
    GaStringList *head = NULL;
    GaStringList *tail = NULL;
    va_list ap;
    const gchar *s;

    if (!txt)
        return NULL;

    s = txt;
    va_start(ap, txt);

    do {
        size_t len = strlen(s);
        GaStringList *node = g_malloc(sizeof(GaStringList) + len);
        node->next = NULL;
        node->size = len;
        memcpy(node->text, s, len + 1);

        if (tail) {
            tail->next = node;
            tail = node;
        } else {
            head = tail = node;
        }

        s = va_arg(ap, const gchar *);
    } while (s != NULL);

    va_end(ap);
    return head;
}

void avahi_string_list_free(GaStringList *list) {
    ga_string_list_free(list);
}

GaStringList *avahi_string_list_find(GaStringList *list, const gchar *key) {
    return ga_string_list_find(list, key);
}

gint avahi_string_list_get_pair(GaStringList *list, gchar **key, gchar **value, gsize *size) {
    return ga_string_list_get_pair(list, key, value, size);
}

GaStringList *avahi_string_list_get_next(GaStringList *l) {
    return ga_string_list_get_next(l);
}

guint8 *avahi_string_list_get_text(GaStringList *l) {
    return ga_string_list_get_text(l);
}

gsize avahi_string_list_get_size(GaStringList *l) {
    return ga_string_list_get_size(l);
}

/*
 * Avahi memory allocation function exports.
 * Real function symbols for binary compatibility with applications
 * compiled against Avahi headers.
 */

void *avahi_malloc(size_t size) {
    return g_malloc(size);
}

void *avahi_malloc0(size_t size) {
    return g_malloc0(size);
}

void *avahi_realloc(void *p, size_t size) {
    return g_realloc(p, size);
}

void avahi_free(void *p) {
    g_free(p);
}

char *avahi_strdup(const char *s) {
    return g_strdup(s);
}

char *avahi_strndup(const char *s, size_t n) {
    return g_strndup(s, n);
}

void *avahi_memdup(const void *p, size_t size) {
    return g_memdup2(p, size);
}

GaStringList *avahi_string_list_new_from_array(const gchar **array, gint length) {
    return ga_string_list_new_from_array(array, length);
}


