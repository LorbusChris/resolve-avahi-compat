/* SPDX-License-Identifier: LGPL-2.1-or-later */

/*
 * example-publish.c - Example of service publishing
 *
 * This example publishes an HTTP service using the resolve-avahi-compat
 * library, which writes .dnssd files to /run/systemd/dnssd/.
 *
 * Requirements:
 * - Write access to /run/systemd/dnssd/ (may need root or appropriate permissions)
 * - systemd-resolved configured with MulticastDNS=yes
 *
 * Build:
 *   gcc -o example-publish example-publish.c ga-*.c \
 *       $(pkg-config --cflags --libs glib-2.0 gobject-2.0 gio-2.0 libsystemd)
 *
 * Usage:
 *   ./example-publish [service-name] [port]
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <glib.h>

#include "ga-client.h"
#include "ga-entry-group.h"

static GMainLoop *loop = NULL;
static GaEntryGroup *group = NULL;

static void on_state_changed(G_GNUC_UNUSED GaEntryGroup *grp,
                              GaEntryGroupState state,
                              G_GNUC_UNUSED gpointer user_data) {
    switch (state) {
        case GA_ENTRY_GROUP_STATE_UNCOMMITED:
            g_print("State: UNCOMMITED\n");
            break;
        case GA_ENTRY_GROUP_STATE_REGISTERING:
            g_print("State: REGISTERING...\n");
            break;
        case GA_ENTRY_GROUP_STATE_ESTABLISHED:
            g_print("State: ESTABLISHED - Service is now published!\n");
            break;
        case GA_ENTRY_GROUP_STATE_COLLISION:
            g_print("State: COLLISION - Name conflict detected\n");
            break;
        case GA_ENTRY_GROUP_STATE_FAILURE:
            g_print("State: FAILURE - Failed to publish service\n");
            g_main_loop_quit(loop);
            break;
    }
}

static void on_signal(int signum) {
    g_print("\nReceived signal %d, cleaning up...\n", signum);
    if (group) {
        ga_entry_group_reset(group, NULL);
    }
    if (loop) {
        g_main_loop_quit(loop);
    }
}

int main(int argc, char *argv[]) {
    const gchar *service_name = argc > 1 ? argv[1] : "My Test Service";
    guint16 port = argc > 2 ? (guint16)atoi(argv[2]) : 8080;
    GError *error = NULL;

    g_print("Service Publishing Example\n");
    g_print("==========================\n");
    g_print("Publishing: %s on port %u\n\n", service_name, port);

    /* Handle signals for cleanup */
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    /* Create client */
    GaClient *client = ga_client_new(GA_CLIENT_FLAG_NO_FLAGS);
    if (!ga_client_start(client, &error)) {
        g_printerr("Failed to start client: %s\n", error->message);
        g_error_free(error);
        return 1;
    }
    g_print("Client started\n");

    /* Create entry group */
    group = ga_entry_group_new();
    g_signal_connect(group, "state-changed", G_CALLBACK(on_state_changed), NULL);

    if (!ga_entry_group_attach(group, client, &error)) {
        g_printerr("Failed to attach entry group: %s\n", error->message);
        g_error_free(error);
        g_object_unref(client);
        return 1;
    }
    g_print("Entry group attached\n");

    /* Add a service */
    GaEntryGroupService *service = ga_entry_group_add_service(
        group,
        service_name,
        "_http._tcp",
        port,
        &error,
        NULL);

    if (!service) {
        g_printerr("Failed to add service: %s\n", error->message);
        g_error_free(error);
        g_object_unref(group);
        g_object_unref(client);
        return 1;
    }
    g_print("Service added\n");

    /* Add some TXT records */
    ga_entry_group_service_set(service, "path", "/", NULL);
    ga_entry_group_service_set(service, "version", "1.0", NULL);
    ga_entry_group_service_set(service, "info", "Example service", NULL);
    g_print("TXT records added\n");

    /* Commit (writes .dnssd file) */
    g_print("\nCommitting (writing .dnssd file)...\n");
    if (!ga_entry_group_commit(group, &error)) {
        g_printerr("Failed to commit: %s\n", error->message);
        g_error_free(error);
        g_object_unref(group);
        g_object_unref(client);
        return 1;
    }

    g_print("\nService published! Press Ctrl+C to stop.\n");
    g_print("You can verify with: ls -la /run/systemd/dnssd/\n\n");

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    /* Cleanup */
    g_print("Cleaning up...\n");
    g_main_loop_unref(loop);
    g_object_unref(group);
    g_object_unref(client);

    g_print("Done.\n");
    return 0;
}
