/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* ga-record-browser.h - Header for GaRecordBrowser (systemd-resolved compatibility) */

#ifndef __GA_RECORD_BROWSER_H__
#define __GA_RECORD_BROWSER_H__

#include <glib-object.h>
#include "ga-client.h"
#include "ga-enums.h"

G_BEGIN_DECLS

typedef struct _GaRecordBrowser GaRecordBrowser;
typedef struct _GaRecordBrowserClass GaRecordBrowserClass;
typedef struct _GaRecordBrowserPrivate GaRecordBrowserPrivate;

struct _GaRecordBrowserClass {
    GObjectClass parent_class;
};

struct _GaRecordBrowser {
    GObject parent;
    GaRecordBrowserPrivate *priv;
};

GType ga_record_browser_get_type(void);

/* TYPE MACROS */
#define GA_TYPE_RECORD_BROWSER \
  (ga_record_browser_get_type())
#define GA_RECORD_BROWSER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GA_TYPE_RECORD_BROWSER, GaRecordBrowser))
#define GA_RECORD_BROWSER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GA_TYPE_RECORD_BROWSER, GaRecordBrowserClass))
#define IS_GA_RECORD_BROWSER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GA_TYPE_RECORD_BROWSER))
#define IS_GA_RECORD_BROWSER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GA_TYPE_RECORD_BROWSER))
#define GA_RECORD_BROWSER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GA_TYPE_RECORD_BROWSER, GaRecordBrowserClass))

GaRecordBrowser *ga_record_browser_new(const gchar * name, guint16 type);

GaRecordBrowser *ga_record_browser_new_full(GaIfIndex interface,
                                            GaProtocol protocol,
                                            const gchar * name,
                                            guint16 clazz,
                                            guint16 type,
                                            GaLookupFlags flags);

gboolean
ga_record_browser_attach(GaRecordBrowser * browser,
                         GaClient * client, GError ** error);

G_END_DECLS

#endif /* #ifndef __GA_RECORD_BROWSER_H__ */
