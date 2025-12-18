/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* ga-enums.h - Enumeration types (systemd-resolved compatibility) */

#ifndef __GA_ENUMS_H__
#define __GA_ENUMS_H__

#include <glib-object.h>

G_BEGIN_DECLS

/*
 * Note: GaProtocol and GA_PROTOCOL_* constants are defined in ga-client.h
 * to avoid circular dependencies. Include ga-client.h if you need them.
 */

/** Some flags for lookup callback functions */
typedef enum {
    GA_LOOKUP_RESULT_CACHED = 1,      /**< This response originates from the cache */
    GA_LOOKUP_RESULT_WIDE_AREA = 2,   /**< This response originates from wide area DNS */
    GA_LOOKUP_RESULT_MULTICAST = 4,   /**< This response originates from multicast DNS */
    GA_LOOKUP_RESULT_LOCAL = 8,       /**< This record/service resides on and was announced by the local host */
    GA_LOOKUP_RESULT_OUR_OWN = 16,    /**< This service belongs to the same local client as the browser object */
    GA_LOOKUP_RESULT_STATIC = 32      /**< The returned data has been defined statically */
} GaLookupResultFlags;

/* Re-export as Avahi types for drop-in compatibility */
typedef GaLookupResultFlags AvahiLookupResultFlags;

#define AVAHI_LOOKUP_RESULT_CACHED     GA_LOOKUP_RESULT_CACHED
#define AVAHI_LOOKUP_RESULT_WIDE_AREA  GA_LOOKUP_RESULT_WIDE_AREA
#define AVAHI_LOOKUP_RESULT_MULTICAST  GA_LOOKUP_RESULT_MULTICAST
#define AVAHI_LOOKUP_RESULT_LOCAL      GA_LOOKUP_RESULT_LOCAL
#define AVAHI_LOOKUP_RESULT_OUR_OWN    GA_LOOKUP_RESULT_OUR_OWN
#define AVAHI_LOOKUP_RESULT_STATIC     GA_LOOKUP_RESULT_STATIC

typedef enum {
    GA_LOOKUP_NO_FLAGS = 0,
    GA_LOOKUP_USE_WIDE_AREA = 1,    /**< Force lookup via wide area DNS */
    GA_LOOKUP_USE_MULTICAST = 2,    /**< Force lookup via multicast DNS */
    GA_LOOKUP_NO_TXT = 4,           /**< When doing service resolving, don't lookup TXT record */
    GA_LOOKUP_NO_ADDRESS = 8        /**< When doing service resolving, don't lookup A/AAAA record */
} GaLookupFlags;

typedef GaLookupFlags AvahiLookupFlags;

#define AVAHI_LOOKUP_NO_FLAGS       GA_LOOKUP_NO_FLAGS
#define AVAHI_LOOKUP_USE_WIDE_AREA  GA_LOOKUP_USE_WIDE_AREA
#define AVAHI_LOOKUP_USE_MULTICAST  GA_LOOKUP_USE_MULTICAST
#define AVAHI_LOOKUP_NO_TXT         GA_LOOKUP_NO_TXT
#define AVAHI_LOOKUP_NO_ADDRESS     GA_LOOKUP_NO_ADDRESS

typedef enum {
    GA_RESOLVER_FOUND = 0,           /**< RR found, resolving successful */
    GA_RESOLVER_FAILURE = 1          /**< Resolving failed */
} GaResolverEvent;

typedef GaResolverEvent AvahiResolverEvent;

#define AVAHI_RESOLVER_FOUND   GA_RESOLVER_FOUND
#define AVAHI_RESOLVER_FAILURE GA_RESOLVER_FAILURE

typedef enum {
    GA_BROWSER_NEW = 0,               /**< The object is new on the network */
    GA_BROWSER_REMOVE = 1,            /**< The object has been removed from the network */
    GA_BROWSER_CACHE_EXHAUSTED = 2,   /**< One-time event, all cache entries have been sent */
    GA_BROWSER_ALL_FOR_NOW = 3,       /**< One-time event, no more records will show up soon */
    GA_BROWSER_FAILURE = 4            /**< Browsing failed */
} GaBrowserEvent;

typedef GaBrowserEvent AvahiBrowserEvent;

#define AVAHI_BROWSER_NEW             GA_BROWSER_NEW
#define AVAHI_BROWSER_REMOVE          GA_BROWSER_REMOVE
#define AVAHI_BROWSER_CACHE_EXHAUSTED GA_BROWSER_CACHE_EXHAUSTED
#define AVAHI_BROWSER_ALL_FOR_NOW     GA_BROWSER_ALL_FOR_NOW
#define AVAHI_BROWSER_FAILURE         GA_BROWSER_FAILURE

/* Avahi server state compatibility (maps to client states for simplicity) */
typedef enum {
    GA_SERVER_INVALID = 0,         /**< Invalid state (initial) */
    GA_SERVER_REGISTERING = 1,     /**< Host RRs are being registered */
    GA_SERVER_RUNNING = 2,         /**< All host RRs have been established */
    GA_SERVER_COLLISION = 3,       /**< Host RR collision */
    GA_SERVER_FAILURE = 4          /**< Fatal failure */
} GaServerState;

typedef GaServerState AvahiServerState;

#define AVAHI_SERVER_INVALID     GA_SERVER_INVALID
#define AVAHI_SERVER_REGISTERING GA_SERVER_REGISTERING
#define AVAHI_SERVER_RUNNING     GA_SERVER_RUNNING
#define AVAHI_SERVER_COLLISION   GA_SERVER_COLLISION
#define AVAHI_SERVER_FAILURE     GA_SERVER_FAILURE

/* Avahi entry group state compatibility */
typedef enum {
    GA_ENTRY_GROUP_UNCOMMITED = 0,    /**< Group not yet committed */
    GA_ENTRY_GROUP_REGISTERING = 1,   /**< Entries being registered */
    GA_ENTRY_GROUP_ESTABLISHED = 2,   /**< Entries established */
    GA_ENTRY_GROUP_COLLISION = 3,     /**< Name collision */
    GA_ENTRY_GROUP_FAILURE = 4        /**< Failure */
} GaEntryGroupStateCompat;

typedef GaEntryGroupStateCompat AvahiEntryGroupState;

#define AVAHI_ENTRY_GROUP_UNCOMMITED   GA_ENTRY_GROUP_UNCOMMITED
#define AVAHI_ENTRY_GROUP_REGISTERING  GA_ENTRY_GROUP_REGISTERING
#define AVAHI_ENTRY_GROUP_ESTABLISHED  GA_ENTRY_GROUP_ESTABLISHED
#define AVAHI_ENTRY_GROUP_COLLISION    GA_ENTRY_GROUP_COLLISION
#define AVAHI_ENTRY_GROUP_FAILURE      GA_ENTRY_GROUP_FAILURE

/* Domain browser type */
typedef enum {
    GA_DOMAIN_BROWSER_BROWSE = 0,          /**< Browse for available domains */
    GA_DOMAIN_BROWSER_BROWSE_DEFAULT = 1,  /**< Browse for default domain */
    GA_DOMAIN_BROWSER_REGISTER = 2,        /**< Browse for registering domains */
    GA_DOMAIN_BROWSER_REGISTER_DEFAULT = 3,/**< Browse for default registering domain */
    GA_DOMAIN_BROWSER_BROWSE_LEGACY = 4,   /**< Legacy browse domain */
    GA_DOMAIN_BROWSER_MAX = 5
} GaDomainBrowserType;

typedef GaDomainBrowserType AvahiDomainBrowserType;

#define AVAHI_DOMAIN_BROWSER_BROWSE          GA_DOMAIN_BROWSER_BROWSE
#define AVAHI_DOMAIN_BROWSER_BROWSE_DEFAULT  GA_DOMAIN_BROWSER_BROWSE_DEFAULT
#define AVAHI_DOMAIN_BROWSER_REGISTER        GA_DOMAIN_BROWSER_REGISTER
#define AVAHI_DOMAIN_BROWSER_REGISTER_DEFAULT GA_DOMAIN_BROWSER_REGISTER_DEFAULT
#define AVAHI_DOMAIN_BROWSER_BROWSE_LEGACY   GA_DOMAIN_BROWSER_BROWSE_LEGACY
#define AVAHI_DOMAIN_BROWSER_MAX             GA_DOMAIN_BROWSER_MAX

/* DNS record types (RFC 1035 and extensions) */
#define GA_DNS_TYPE_A     0x01
#define GA_DNS_TYPE_NS    0x02
#define GA_DNS_TYPE_CNAME 0x05
#define GA_DNS_TYPE_SOA   0x06
#define GA_DNS_TYPE_PTR   0x0C
#define GA_DNS_TYPE_HINFO 0x0D
#define GA_DNS_TYPE_MX    0x0F
#define GA_DNS_TYPE_TXT   0x10
#define GA_DNS_TYPE_AAAA  0x1C
#define GA_DNS_TYPE_SRV   0x21

#define AVAHI_DNS_TYPE_A     GA_DNS_TYPE_A
#define AVAHI_DNS_TYPE_NS    GA_DNS_TYPE_NS
#define AVAHI_DNS_TYPE_CNAME GA_DNS_TYPE_CNAME
#define AVAHI_DNS_TYPE_SOA   GA_DNS_TYPE_SOA
#define AVAHI_DNS_TYPE_PTR   GA_DNS_TYPE_PTR
#define AVAHI_DNS_TYPE_HINFO GA_DNS_TYPE_HINFO
#define AVAHI_DNS_TYPE_MX    GA_DNS_TYPE_MX
#define AVAHI_DNS_TYPE_TXT   GA_DNS_TYPE_TXT
#define AVAHI_DNS_TYPE_AAAA  GA_DNS_TYPE_AAAA
#define AVAHI_DNS_TYPE_SRV   GA_DNS_TYPE_SRV

/* DNS record classes (RFC 1035) */
#define GA_DNS_CLASS_IN 0x01

#define AVAHI_DNS_CLASS_IN GA_DNS_CLASS_IN

/* Default TTL values */
#define GA_DEFAULT_TTL_HOST_NAME 120
#define GA_DEFAULT_TTL           (75*60)

#define AVAHI_DEFAULT_TTL_HOST_NAME GA_DEFAULT_TTL_HOST_NAME
#define AVAHI_DEFAULT_TTL           GA_DEFAULT_TTL

/* Service cookie constants */
#define GA_SERVICE_COOKIE "org.freedesktop.Avahi.cookie"
#define GA_SERVICE_COOKIE_INVALID 0

#define AVAHI_SERVICE_COOKIE         GA_SERVICE_COOKIE
#define AVAHI_SERVICE_COOKIE_INVALID GA_SERVICE_COOKIE_INVALID

/* GType functions for enums */
GType ga_protocol_get_type(void) G_GNUC_CONST;
GType ga_lookup_result_flags_get_type(void) G_GNUC_CONST;
GType ga_lookup_flags_get_type(void) G_GNUC_CONST;
GType ga_resolver_event_get_type(void) G_GNUC_CONST;
GType ga_browser_event_get_type(void) G_GNUC_CONST;

#define GA_TYPE_PROTOCOL (ga_protocol_get_type())
#define GA_TYPE_LOOKUP_RESULT_FLAGS (ga_lookup_result_flags_get_type())
#define GA_TYPE_LOOKUP_FLAGS (ga_lookup_flags_get_type())
#define GA_TYPE_RESOLVER_EVENT (ga_resolver_event_get_type())
#define GA_TYPE_BROWSER_EVENT (ga_browser_event_get_type())

G_END_DECLS

#endif /* #ifndef __GA_ENUMS_H__ */
