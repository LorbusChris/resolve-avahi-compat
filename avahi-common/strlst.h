/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * Avahi drop-in compatibility wrapper for avahi-common/strlst.h
 *
 * This provides the AvahiStringList type and functions.
 */

#ifndef __AVAHI_COMMON_STRLST_H_COMPAT__
#define __AVAHI_COMMON_STRLST_H_COMPAT__

#include "../ga-entry-group.h"

/*
 * AvahiStringList is defined in ga-entry-group.h as a typedef of GaStringList.
 * The following functions are available:
 * - avahi_string_list_new
 * - avahi_string_list_new_from_array
 * - avahi_string_list_free
 * - avahi_string_list_find
 * - avahi_string_list_get_pair
 * - avahi_string_list_get_next
 * - avahi_string_list_get_text
 * - avahi_string_list_get_size
 */

#endif /* __AVAHI_COMMON_STRLST_H_COMPAT__ */
