/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* ga-service-resolver.h - Header for GaServiceResolver (systemd-resolved compatibility) */

#ifndef __GA_SERVICE_RESOLVER_H__
#define __GA_SERVICE_RESOLVER_H__

#include <glib-object.h>
#include <stdint.h>
#include "ga-client.h"
#include "ga-enums.h"

G_BEGIN_DECLS

/* IPv4 address structure (Avahi compatible) */
typedef struct {
    uint32_t address; /**< Address data in network byte order. */
} GaIPv4Address;

/* IPv6 address structure (Avahi compatible) */
typedef struct {
    uint8_t address[16]; /**< Address data */
} GaIPv6Address;

/* Address structure compatible with Avahi */
typedef struct {
    GaProtocol proto;  /**< Address family */
    union {
        GaIPv6Address ipv6;                  /**< Address when IPv6 */
        GaIPv4Address ipv4;                  /**< Address when IPv4 */
        uint8_t data[sizeof(GaIPv6Address)]; /**< Type-independent data field */
    } data;
} GaAddress;

/* Re-export as Avahi types for drop-in compatibility */
typedef GaIPv4Address AvahiIPv4Address;
typedef GaIPv6Address AvahiIPv6Address;
typedef GaAddress AvahiAddress;

typedef struct _GaServiceResolver GaServiceResolver;
typedef struct _GaServiceResolverClass GaServiceResolverClass;
typedef struct _GaServiceResolverPrivate GaServiceResolverPrivate;

struct _GaServiceResolverClass {
    GObjectClass parent_class;
};

struct _GaServiceResolver {
    GObject parent;
    GaServiceResolverPrivate *priv;
};

GType ga_service_resolver_get_type(void);

/* TYPE MACROS */
#define GA_TYPE_SERVICE_RESOLVER \
  (ga_service_resolver_get_type())
#define GA_SERVICE_RESOLVER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GA_TYPE_SERVICE_RESOLVER, GaServiceResolver))
#define GA_SERVICE_RESOLVER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GA_TYPE_SERVICE_RESOLVER, GaServiceResolverClass))
#define IS_GA_SERVICE_RESOLVER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GA_TYPE_SERVICE_RESOLVER))
#define IS_GA_SERVICE_RESOLVER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GA_TYPE_SERVICE_RESOLVER))
#define GA_SERVICE_RESOLVER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GA_TYPE_SERVICE_RESOLVER, GaServiceResolverClass))

GaServiceResolver *ga_service_resolver_new(GaIfIndex interface,
                                           GaProtocol protocol,
                                           const gchar * name,
                                           const gchar * type,
                                           const gchar * domain,
                                           GaProtocol address_protocol,
                                           GaLookupFlags flags);

gboolean
ga_service_resolver_attach(GaServiceResolver * resolver,
                           GaClient * client, GError ** error);

gboolean
ga_service_resolver_get_address(GaServiceResolver * resolver,
                                GaAddress * address, uint16_t * port);

/**
 * ga_address_snprint:
 * @ret: (out): Buffer to write the address string to
 * @length: Size of the buffer
 * @a: The address to convert
 *
 * Convert a GaAddress to a string representation.
 *
 * Returns: @ret on success, NULL on error
 */
gchar *ga_address_snprint(gchar *ret, gsize length, const GaAddress *a);

/* Avahi-compatible function - real exported symbol */
gchar *avahi_address_snprint(gchar *ret, gsize length, const GaAddress *a);

G_END_DECLS

#endif /* #ifndef __GA_SERVICE_RESOLVER_H__ */
