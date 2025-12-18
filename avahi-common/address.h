/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * Avahi drop-in compatibility wrapper for avahi-common/address.h
 *
 * This provides the AvahiAddress, AvahiIfIndex, AvahiProtocol types
 * and related constants that applications using Avahi expect.
 */

#ifndef __AVAHI_COMMON_ADDRESS_H_COMPAT__
#define __AVAHI_COMMON_ADDRESS_H_COMPAT__

#include "../ga-client.h"
#include "../ga-service-resolver.h"

/*
 * All the key types are already defined in ga-client.h and ga-service-resolver.h:
 * - AvahiIfIndex (typedef of GaIfIndex)
 * - AvahiProtocol (typedef of GaProtocol)
 * - AVAHI_IF_UNSPEC, AVAHI_PROTO_*, etc.
 * - AvahiAddress, AvahiIPv4Address, AvahiIPv6Address
 * - avahi_address_snprint
 */

#endif /* __AVAHI_COMMON_ADDRESS_H_COMPAT__ */
