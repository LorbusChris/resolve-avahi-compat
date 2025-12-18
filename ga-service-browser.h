/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* ga-service-browser.h - Header for GaServiceBrowser (systemd-resolved compatibility) */

#ifndef __GA_SERVICE_BROWSER_H__
#define __GA_SERVICE_BROWSER_H__

#include <glib-object.h>
#include "ga-client.h"
#include "ga-enums.h"

G_BEGIN_DECLS

typedef struct _GaServiceBrowser GaServiceBrowser;
typedef struct _GaServiceBrowserClass GaServiceBrowserClass;
typedef struct _GaServiceBrowserPrivate GaServiceBrowserPrivate;

struct _GaServiceBrowserClass {
    GObjectClass parent_class;
};

struct _GaServiceBrowser {
    GObject parent;
    GaServiceBrowserPrivate *priv;
};

GType ga_service_browser_get_type(void);

/* TYPE MACROS */
#define GA_TYPE_SERVICE_BROWSER \
  (ga_service_browser_get_type())
#define GA_SERVICE_BROWSER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GA_TYPE_SERVICE_BROWSER, GaServiceBrowser))
#define GA_SERVICE_BROWSER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GA_TYPE_SERVICE_BROWSER, GaServiceBrowserClass))
#define IS_GA_SERVICE_BROWSER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GA_TYPE_SERVICE_BROWSER))
#define IS_GA_SERVICE_BROWSER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GA_TYPE_SERVICE_BROWSER))
#define GA_SERVICE_BROWSER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GA_TYPE_SERVICE_BROWSER, GaServiceBrowserClass))

GaServiceBrowser *ga_service_browser_new(const gchar * type);

GaServiceBrowser *ga_service_browser_new_full(GaIfIndex interface,
                                              GaProtocol protocol,
                                              const gchar * type,
                                              gchar * domain,
                                              GaLookupFlags flags);

gboolean
ga_service_browser_attach(GaServiceBrowser * browser,
                          GaClient * client, GError ** error);

G_END_DECLS

#endif /* #ifndef __GA_SERVICE_BROWSER_H__ */
