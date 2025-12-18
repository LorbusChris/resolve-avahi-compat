/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * Avahi drop-in compatibility wrapper for avahi-common/gccmacro.h
 *
 * GCC attribute macros - most not needed when using GLib.
 */

#ifndef __AVAHI_COMMON_GCCMACRO_H_COMPAT__
#define __AVAHI_COMMON_GCCMACRO_H_COMPAT__

#include <glib.h>

/* Map to GLib/standard compiler attributes where possible */
#define AVAHI_GCC_UNUSED       G_GNUC_UNUSED
#define AVAHI_GCC_PRINTF_ATTR12 G_GNUC_PRINTF(1, 2)
#define AVAHI_GCC_PRINTF_ATTR23 G_GNUC_PRINTF(2, 3)
#define AVAHI_GCC_SENTINEL     G_GNUC_NULL_TERMINATED

#ifdef __GNUC__
#define AVAHI_GCC_NORETURN     __attribute__((noreturn))
#define AVAHI_GCC_ALLOC_SIZE(x) __attribute__((alloc_size(x)))
#define AVAHI_GCC_ALLOC_SIZE2(x,y) __attribute__((alloc_size(x,y)))
#define AVAHI_GCC_DEPRECATED   __attribute__((deprecated))
#else
#define AVAHI_GCC_NORETURN
#define AVAHI_GCC_ALLOC_SIZE(x)
#define AVAHI_GCC_ALLOC_SIZE2(x,y)
#define AVAHI_GCC_DEPRECATED
#endif

#endif /* __AVAHI_COMMON_GCCMACRO_H_COMPAT__ */
