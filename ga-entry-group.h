/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* ga-entry-group.h - Header for GaEntryGroup (systemd-resolved compatibility) */

#ifndef __GA_ENTRY_GROUP_H__
#define __GA_ENTRY_GROUP_H__

#include <glib-object.h>
#include "ga-client.h"
#include "ga-enums.h"

G_BEGIN_DECLS

/* Publish flags matching Avahi */
typedef enum {
    GA_PUBLISH_UNIQUE = 1,
    GA_PUBLISH_NO_PROBE = 2,
    GA_PUBLISH_NO_ANNOUNCE = 4,
    GA_PUBLISH_ALLOW_MULTIPLE = 8,
    GA_PUBLISH_NO_REVERSE = 16,
    GA_PUBLISH_NO_COOKIE = 32,
    GA_PUBLISH_UPDATE = 64,
    GA_PUBLISH_USE_WIDE_AREA = 128,
    GA_PUBLISH_USE_MULTICAST = 256
} GaPublishFlags;

/* Re-export as Avahi types for drop-in compatibility */
typedef GaPublishFlags AvahiPublishFlags;

#define AVAHI_PUBLISH_UNIQUE        GA_PUBLISH_UNIQUE
#define AVAHI_PUBLISH_NO_PROBE      GA_PUBLISH_NO_PROBE
#define AVAHI_PUBLISH_NO_ANNOUNCE   GA_PUBLISH_NO_ANNOUNCE
#define AVAHI_PUBLISH_ALLOW_MULTIPLE GA_PUBLISH_ALLOW_MULTIPLE
#define AVAHI_PUBLISH_NO_REVERSE    GA_PUBLISH_NO_REVERSE
#define AVAHI_PUBLISH_NO_COOKIE     GA_PUBLISH_NO_COOKIE
#define AVAHI_PUBLISH_UPDATE        GA_PUBLISH_UPDATE
#define AVAHI_PUBLISH_USE_WIDE_AREA GA_PUBLISH_USE_WIDE_AREA
#define AVAHI_PUBLISH_USE_MULTICAST GA_PUBLISH_USE_MULTICAST

/* backward compatible typo workaround */
#define GA_ENTRY_GROUP_STATE_COLLISTION GA_ENTRY_GROUP_STATE_COLLISION
typedef enum {
    GA_ENTRY_GROUP_STATE_UNCOMMITED = 0,
    GA_ENTRY_GROUP_STATE_REGISTERING = 1,
    GA_ENTRY_GROUP_STATE_ESTABLISHED = 2,
    GA_ENTRY_GROUP_STATE_COLLISION = 3,
    GA_ENTRY_GROUP_STATE_FAILURE = 4
} GaEntryGroupState;

GType ga_entry_group_state_get_type(void) G_GNUC_CONST;
#define GA_TYPE_ENTRY_GROUP_STATE (ga_entry_group_state_get_type())

typedef struct _GaEntryGroupService GaEntryGroupService;
typedef struct _GaEntryGroup GaEntryGroup;
typedef struct _GaEntryGroupClass GaEntryGroupClass;
typedef struct _GaEntryGroupPrivate GaEntryGroupPrivate;

/* String list structure for TXT records (simplified, Avahi-compatible) */
typedef struct GaStringList {
    struct GaStringList *next;
    gsize size;
    guint8 text[1];
} GaStringList;

/* Re-export as Avahi type for drop-in compatibility */
typedef GaStringList AvahiStringList;

struct _GaEntryGroupService {
    GaIfIndex interface;
    GaProtocol protocol;
    GaPublishFlags flags;
    gchar *name;
    gchar *type;
    gchar *domain;
    gchar *host;
    guint16 port;
};

struct _GaEntryGroupClass {
    GObjectClass parent_class;
};

struct _GaEntryGroup {
    GObject parent;
    GaEntryGroupPrivate *priv;
};

GType ga_entry_group_get_type(void);

/* TYPE MACROS */
#define GA_TYPE_ENTRY_GROUP \
  (ga_entry_group_get_type())
#define GA_ENTRY_GROUP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GA_TYPE_ENTRY_GROUP, GaEntryGroup))
#define GA_ENTRY_GROUP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GA_TYPE_ENTRY_GROUP, GaEntryGroupClass))
#define IS_GA_ENTRY_GROUP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GA_TYPE_ENTRY_GROUP))
#define IS_GA_ENTRY_GROUP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GA_TYPE_ENTRY_GROUP))
#define GA_ENTRY_GROUP_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GA_TYPE_ENTRY_GROUP, GaEntryGroupClass))

GaEntryGroup *ga_entry_group_new(void);

gboolean ga_entry_group_attach(GaEntryGroup * group,
                      GaClient * client, GError ** error);

GaEntryGroupService *ga_entry_group_add_service_strlist(GaEntryGroup * group,
                                                        const gchar * name,
                                                        const gchar * type,
                                                        guint16 port,
                                                        GError ** error,
                                                        GaStringList * txt);

GaEntryGroupService *ga_entry_group_add_service_full_strlist(GaEntryGroup * group,
                                                             GaIfIndex interface,
                                                             GaProtocol protocol,
                                                             GaPublishFlags flags,
                                                             const gchar * name,
                                                             const gchar * type,
                                                             const gchar * domain,
                                                             const gchar * host,
                                                             guint16 port,
                                                             GError ** error,
                                                             GaStringList * txt);

GaEntryGroupService *ga_entry_group_add_service(GaEntryGroup * group,
                                                const gchar * name,
                                                const gchar * type,
                                                guint16 port, GError ** error,
                                                ...);

GaEntryGroupService *ga_entry_group_add_service_full(GaEntryGroup * group,
                                                     GaIfIndex interface,
                                                     GaProtocol protocol,
                                                     GaPublishFlags flags,
                                                     const gchar * name,
                                                     const gchar * type,
                                                     const gchar * domain,
                                                     const gchar * host,
                                                     guint16 port,
                                                     GError ** error, ...);

/* Add raw record */
gboolean ga_entry_group_add_record(GaEntryGroup * group,
                          GaPublishFlags flags,
                          const gchar * name,
                          guint16 type,
                          guint32 ttl,
                          const void *rdata, gsize size, GError ** error);

gboolean ga_entry_group_add_record_full(GaEntryGroup * group,
                               GaIfIndex interface,
                               GaProtocol protocol,
                               GaPublishFlags flags,
                               const gchar * name,
                               guint16 clazz,
                               guint16 type,
                               guint32 ttl,
                               const void *rdata,
                               gsize size, GError ** error);

void ga_entry_group_service_freeze(GaEntryGroupService * service);

/* Set a key in the service record */
gboolean ga_entry_group_service_set(GaEntryGroupService * service,
                           const gchar * key, const gchar * value,
                           GError ** error);

gboolean ga_entry_group_service_set_arbitrary(GaEntryGroupService * service,
                                     const gchar * key, const guint8 * value,
                                     gsize size, GError ** error);

/* Remove one key from the service record */
gboolean ga_entry_group_service_remove_key(GaEntryGroupService * service,
                                  const gchar * key, GError ** error);

/* Update the txt record of the frozen service */
gboolean ga_entry_group_service_thaw(GaEntryGroupService * service, GError ** error);

/* Commit all newly added services */
gboolean ga_entry_group_commit(GaEntryGroup * group, GError ** error);

/* Invalidate all GaEntryGroupServices */
gboolean ga_entry_group_reset(GaEntryGroup * group, GError ** error);

/* String list helpers */
GaStringList *ga_string_list_new(const gchar *txt, ...);
GaStringList *ga_string_list_new_from_array(const gchar **array, gint length);
void ga_string_list_free(GaStringList *list);

/* Avahi compatibility string list functions */
GaStringList *ga_string_list_find(GaStringList *list, const gchar *key);
gint ga_string_list_get_pair(GaStringList *list, gchar **key, gchar **value, gsize *size);

/* Additional string list accessors for Avahi compatibility */
static inline GaStringList *ga_string_list_get_next(GaStringList *l) { return l ? l->next : NULL; }
static inline guint8 *ga_string_list_get_text(GaStringList *l) { return l ? l->text : NULL; }
static inline gsize ga_string_list_get_size(GaStringList *l) { return l ? l->size : 0; }

/*
 * Avahi-compatible function declarations.
 * These are real exported symbols for binary compatibility with applications
 * compiled against Avahi headers. For source compatibility (recompilation),
 * the macro aliases below redirect to the ga_* implementations.
 */
GaStringList *avahi_string_list_new(const gchar *txt, ...);
void avahi_string_list_free(GaStringList *list);
GaStringList *avahi_string_list_find(GaStringList *list, const gchar *key);
gint avahi_string_list_get_pair(GaStringList *list, gchar **key, gchar **value, gsize *size);
GaStringList *avahi_string_list_get_next(GaStringList *l);
guint8 *avahi_string_list_get_text(GaStringList *l);
gsize avahi_string_list_get_size(GaStringList *l);

/* Avahi memory allocation functions */
void *avahi_malloc(gsize size);
void *avahi_malloc0(gsize size);
void *avahi_realloc(void *p, gsize size);
void avahi_free(void *p);
gchar *avahi_strdup(const gchar *s);
gchar *avahi_strndup(const gchar *s, gsize n);
void *avahi_memdup(const void *p, gsize size);
GaStringList *avahi_string_list_new_from_array(const gchar **array, gint length);

G_END_DECLS

#endif /* #ifndef __GA_ENTRY_GROUP_H__ */
