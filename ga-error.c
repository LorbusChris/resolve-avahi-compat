/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* ga-error.c - Source for error types (systemd-resolved compatibility) */

#include <glib.h>
#include "ga-error.h"

GQuark ga_error_quark(void) {
    static GQuark quark = 0;
    if (!quark)
        quark = g_quark_from_static_string("ga_error");
    return quark;
}
