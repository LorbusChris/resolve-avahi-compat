/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * example-browse.c - Example service browser using resolve-avahi-compat
 *
 * Build with:
 *   cc -o example-browse example-browse.c \
 *      $(pkg-config --cflags --libs resolve-avahi-compat)
 *
 * Or manually:
 *   cc -o example-browse example-browse.c \
 *      -I.. -L../builddir -lresolve-avahi-compat \
 *      $(pkg-config --cflags --libs glib-2.0 gobject-2.0 libsystemd)
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "ga-client.h"
#include "ga-service-browser.h"
#include "ga-service-resolver.h"
#include "ga-error.h"

static GMainLoop *main_loop = NULL;
static GaClient *client = NULL;

static void on_resolver_found(GaServiceResolver *resolver,
                              G_GNUC_UNUSED gint interface,
                              G_GNUC_UNUSED GaProtocol protocol,
                              const gchar *name,
                              G_GNUC_UNUSED const gchar *type,
                              G_GNUC_UNUSED const gchar *domain,
                              const gchar *host_name,
                              const GaAddress *address,
                              gint port,
                              G_GNUC_UNUSED gpointer txt,
                              G_GNUC_UNUSED GaLookupResultFlags flags,
                              G_GNUC_UNUSED gpointer user_data) {
    char addr_str[64] = {0};
    
    if (address) {
        if (address->proto == GA_PROTOCOL_INET) {
            uint8_t *bytes = (uint8_t *)&address->data.ipv4.address;
            snprintf(addr_str, sizeof(addr_str), "%d.%d.%d.%d",
                     bytes[0], bytes[1], bytes[2], bytes[3]);
        } else if (address->proto == GA_PROTOCOL_INET6) {
            snprintf(addr_str, sizeof(addr_str), "IPv6 address");
        }
    }
    
    g_print("  RESOLVED: %s at %s:%d (host: %s)\n",
            name, addr_str, port, host_name ? host_name : "?");
    
    g_object_unref(resolver);
}

static void on_resolver_failure(GaServiceResolver *resolver,
                                GError *error,
                                G_GNUC_UNUSED gpointer user_data) {
    g_printerr("  RESOLVE FAILED: %s\n", error->message);
    g_object_unref(resolver);
}

static void on_new_service(G_GNUC_UNUSED GaServiceBrowser *browser,
                           gint interface,
                           GaProtocol protocol,
                           const gchar *name,
                           const gchar *type,
                           const gchar *domain,
                           G_GNUC_UNUSED GaLookupResultFlags flags,
                           G_GNUC_UNUSED gpointer user_data) {
    g_print("+ NEW: '%s' type=%s domain=%s interface=%d\n",
            name, type, domain, interface);
    
    /* Create resolver to get the IP address */
    GaServiceResolver *resolver = ga_service_resolver_new(
        interface,
        protocol,
        name,
        type,
        domain,
        GA_PROTOCOL_UNSPEC,
        GA_LOOKUP_NO_FLAGS
    );
    
    g_signal_connect(resolver, "found",
                     G_CALLBACK(on_resolver_found), NULL);
    g_signal_connect(resolver, "failure",
                     G_CALLBACK(on_resolver_failure), NULL);
    
    GError *error = NULL;
    if (!ga_service_resolver_attach(resolver, client, &error)) {
        g_printerr("  Failed to start resolver: %s\n", error->message);
        g_error_free(error);
        g_object_unref(resolver);
    }
}

static void on_removed_service(G_GNUC_UNUSED GaServiceBrowser *browser,
                               G_GNUC_UNUSED gint interface,
                               G_GNUC_UNUSED GaProtocol protocol,
                               const gchar *name,
                               const gchar *type,
                               const gchar *domain,
                               G_GNUC_UNUSED GaLookupResultFlags flags,
                               G_GNUC_UNUSED gpointer user_data) {
    g_print("- REMOVED: '%s' type=%s domain=%s\n", name, type, domain);
}

static void on_all_for_now(G_GNUC_UNUSED GaServiceBrowser *browser,
                           G_GNUC_UNUSED gpointer user_data) {
    g_print("-- Initial snapshot complete --\n");
}

static void on_failure(G_GNUC_UNUSED GaServiceBrowser *browser,
                       GError *error,
                       G_GNUC_UNUSED gpointer user_data) {
    g_printerr("Browser failure: %s\n", error->message);
    g_main_loop_quit(main_loop);
}

static void signal_handler(G_GNUC_UNUSED int sig) {
    g_print("\nExiting...\n");
    if (main_loop)
        g_main_loop_quit(main_loop);
}

int main(int argc, char *argv[]) {
    GError *error = NULL;
    const char *service_type = "_http._tcp";
    
    if (argc > 1) {
        service_type = argv[1];
    }
    
    g_print("Browsing for services of type: %s\n", service_type);
    g_print("Press Ctrl+C to exit\n\n");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create client */
    client = ga_client_new(GA_CLIENT_FLAG_NO_FLAGS);
    if (!ga_client_start(client, &error)) {
        g_printerr("Failed to start client: %s\n", error->message);
        g_printerr("Make sure systemd-resolved is running and mDNS is enabled.\n");
        g_error_free(error);
        return 1;
    }
    
    g_print("Connected to systemd-resolved\n");
    
    /* Create browser */
    GaServiceBrowser *browser = ga_service_browser_new(service_type);
    
    g_signal_connect(browser, "new-service",
                     G_CALLBACK(on_new_service), NULL);
    g_signal_connect(browser, "removed-service",
                     G_CALLBACK(on_removed_service), NULL);
    g_signal_connect(browser, "all-for-now",
                     G_CALLBACK(on_all_for_now), NULL);
    g_signal_connect(browser, "failure",
                     G_CALLBACK(on_failure), NULL);
    
    if (!ga_service_browser_attach(browser, client, &error)) {
        g_printerr("Failed to start browsing: %s\n", error->message);
        g_error_free(error);
        g_object_unref(browser);
        g_object_unref(client);
        return 1;
    }
    
    /* Run main loop */
    main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop);
    
    /* Cleanup */
    g_main_loop_unref(main_loop);
    g_object_unref(browser);
    g_object_unref(client);
    
    return 0;
}
