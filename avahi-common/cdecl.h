/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * Avahi drop-in compatibility wrapper for avahi-common/cdecl.h
 *
 * This provides the AVAHI_C_DECL_BEGIN/END macros.
 */

#ifndef __AVAHI_COMMON_CDECL_H_COMPAT__
#define __AVAHI_COMMON_CDECL_H_COMPAT__

/* C++ compatibility macros - GLib provides these via G_BEGIN_DECLS/G_END_DECLS */
#ifdef __cplusplus
#define AVAHI_C_DECL_BEGIN extern "C" {
#define AVAHI_C_DECL_END }
#else
#define AVAHI_C_DECL_BEGIN
#define AVAHI_C_DECL_END
#endif

#endif /* __AVAHI_COMMON_CDECL_H_COMPAT__ */
