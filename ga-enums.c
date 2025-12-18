/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* ga-enums.c - Enum type implementations (systemd-resolved compatibility) */

#include "ga-client.h"
#include "ga-enums.h"

GType ga_protocol_get_type(void) {
    static GType type = 0;
    if (G_UNLIKELY(type == 0)) {
        static const GEnumValue values[] = {
            { GA_PROTOCOL_INET, "GA_PROTOCOL_INET", "inet" },
            { GA_PROTOCOL_INET6, "GA_PROTOCOL_INET6", "inet6" },
            { GA_PROTOCOL_UNSPEC, "GA_PROTOCOL_UNSPEC", "unspec" },
            { 0, NULL, NULL }
        };
        type = g_enum_register_static("GaProtocol", values);
    }
    return type;
}

GType ga_lookup_result_flags_get_type(void) {
    static GType type = 0;
    if (G_UNLIKELY(type == 0)) {
        static const GFlagsValue values[] = {
            { GA_LOOKUP_RESULT_CACHED, "GA_LOOKUP_RESULT_CACHED", "cached" },
            { GA_LOOKUP_RESULT_WIDE_AREA, "GA_LOOKUP_RESULT_WIDE_AREA", "wide-area" },
            { GA_LOOKUP_RESULT_MULTICAST, "GA_LOOKUP_RESULT_MULTICAST", "multicast" },
            { GA_LOOKUP_RESULT_LOCAL, "GA_LOOKUP_RESULT_LOCAL", "local" },
            { GA_LOOKUP_RESULT_OUR_OWN, "GA_LOOKUP_RESULT_OUR_OWN", "our-own" },
            { GA_LOOKUP_RESULT_STATIC, "GA_LOOKUP_RESULT_STATIC", "static" },
            { 0, NULL, NULL }
        };
        type = g_flags_register_static("GaLookupResultFlags", values);
    }
    return type;
}

GType ga_lookup_flags_get_type(void) {
    static GType type = 0;
    if (G_UNLIKELY(type == 0)) {
        static const GFlagsValue values[] = {
            { GA_LOOKUP_NO_FLAGS, "GA_LOOKUP_NO_FLAGS", "no-flags" },
            { GA_LOOKUP_USE_WIDE_AREA, "GA_LOOKUP_USE_WIDE_AREA", "use-wide-area" },
            { GA_LOOKUP_USE_MULTICAST, "GA_LOOKUP_USE_MULTICAST", "use-multicast" },
            { GA_LOOKUP_NO_TXT, "GA_LOOKUP_NO_TXT", "no-txt" },
            { GA_LOOKUP_NO_ADDRESS, "GA_LOOKUP_NO_ADDRESS", "no-address" },
            { 0, NULL, NULL }
        };
        type = g_flags_register_static("GaLookupFlags", values);
    }
    return type;
}

GType ga_resolver_event_get_type(void) {
    static GType type = 0;
    if (G_UNLIKELY(type == 0)) {
        static const GEnumValue values[] = {
            { GA_RESOLVER_FOUND, "GA_RESOLVER_FOUND", "found" },
            { GA_RESOLVER_FAILURE, "GA_RESOLVER_FAILURE", "failure" },
            { 0, NULL, NULL }
        };
        type = g_enum_register_static("GaResolverEvent", values);
    }
    return type;
}

GType ga_browser_event_get_type(void) {
    static GType type = 0;
    if (G_UNLIKELY(type == 0)) {
        static const GEnumValue values[] = {
            { GA_BROWSER_NEW, "GA_BROWSER_NEW", "new" },
            { GA_BROWSER_REMOVE, "GA_BROWSER_REMOVE", "remove" },
            { GA_BROWSER_CACHE_EXHAUSTED, "GA_BROWSER_CACHE_EXHAUSTED", "cache-exhausted" },
            { GA_BROWSER_ALL_FOR_NOW, "GA_BROWSER_ALL_FOR_NOW", "all-for-now" },
            { GA_BROWSER_FAILURE, "GA_BROWSER_FAILURE", "failure" },
            { 0, NULL, NULL }
        };
        type = g_enum_register_static("GaBrowserEvent", values);
    }
    return type;
}
