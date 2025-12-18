# resolve-avahi-compat

A drop-in replacement library for `avahi-gobject` that uses `systemd-resolved` instead of the Avahi daemon for mDNS/DNS-SD service discovery and resolution.

## Overview

This library provides the same GObject-based API as `avahi-gobject` but communicates with `systemd-resolved` via its Varlink interface instead of requiring the Avahi daemon.

> **⚠️ Important:** This library requires [systemd#40133](https://github.com/systemd/systemd/pull/40133), which is not yet merged. You will need a patched version of systemd until this PR is included in a release.

## Features

### Supported (via systemd-resolved)

- **Service Browsing** (`GaServiceBrowser`): Discover mDNS services on the local network
- **Service Resolution** (`GaServiceResolver`): Resolve service names to IP addresses and ports
- **Record Browsing** (`GaRecordBrowser`): Query DNS records (one-shot queries)
- **Client Management** (`GaClient`): Connection management to systemd-resolved
- **Service Publishing** (`GaEntryGroup`): Publish services via `.dnssd` files (see below)

### Service Publishing via .dnssd Files

Service publishing is implemented by writing `.dnssd` configuration files to `/run/systemd/dnssd/` as documented in [systemd.dnssd(5)](https://www.freedesktop.org/software/systemd/man/latest/systemd.dnssd.html). After files are created, systemd-resolved is signaled to reload its configuration via D-Bus.

**Requirements for publishing:**
- Write access to `/run/systemd/dnssd/` (may require appropriate permissions)
- systemd-resolved must be configured with `MulticastDNS=yes` or `MulticastDNS=resolve`
- The D-Bus `ReloadDNSSD` method must be accessible

**Supported operations:**
- Publishing services with SRV and TXT records
- Dynamic TXT record updates via `ga_entry_group_service_set()` + `ga_entry_group_service_thaw()`
- Multiple services per entry group

**Limitation:**
- Raw record publishing (`ga_entry_group_add_record()`) is not supported - systemd-resolved's .dnssd file format only supports service (SRV/TXT) records, not arbitrary DNS record types (A, AAAA, PTR, etc.)

### Not Supported

- **Raw Record Publishing**: Adding arbitrary DNS records via `ga_entry_group_add_record()` is not supported by systemd-resolved's configuration file format. This is a fundamental limitation of how systemd-resolved handles mDNS publishing.

## Requirements

- GLib 2.56+
- systemd 259+ (for Varlink API with `BrowseServices` ifindex=0 support)
  - **Note:** Requires [systemd#40133](https://github.com/systemd/systemd/pull/40133)
- `libsystemd` development headers

## Building

### With Meson

```bash
cd resolve-avahi-compat
meson setup builddir
meson compile -C builddir
meson install -C builddir
```

## Usage

The API mirrors `avahi-gobject`. You can use either the native includes or the Avahi-compatible includes:

### Include Paths

The library provides Avahi-compatible headers allowing existing code to compile unchanged:

```c
/* These includes work with both Avahi and resolve-avahi-compat */
#include <avahi-gobject/ga-client.h>
#include <avahi-gobject/ga-service-browser.h>

/* Low-level Avahi C API compatibility */
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>

/* Common Avahi definitions */
#include <avahi-common/address.h>
#include <avahi-common/strlst.h>
#include <avahi-common/error.h>
#include <avahi-common/defs.h>

/* GLib integration */
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>
```

### Switching from Avahi in Meson

To switch an existing project from `avahi-gobject` to `resolve-avahi-compat`, update your `meson.build`:

**Before (using Avahi):**
```meson
avahi_client_dep = dependency('avahi-client')
avahi_gobject_dep = dependency('avahi-gobject')
avahi_glib_dep = dependency('avahi-glib')

executable('myapp',
  'main.c',
  dependencies: [avahi_client_dep, avahi_gobject_dep, avahi_glib_dep],
)
```

**After (using resolve-avahi-compat):**
```meson
# Option 1: As an installed system library
resolve_avahi_dep = dependency('resolve-avahi-compat')

executable('myapp',
  'main.c',
  dependencies: [resolve_avahi_dep],
)
```

**Or as a Meson subproject:**
```meson
# Option 2: As a subproject (add to subprojects/ directory)
avahi_compat_proj = subproject('resolve-avahi-compat')
resolve_avahi_dep = avahi_compat_proj.get_variable('resolve_avahi_compat_dep')

executable('myapp',
  'main.c',
  dependencies: [resolve_avahi_dep],
)
```

No changes to your C source files are required—the compatibility headers provide the same API.

### Example: Service Discovery

```c
#include <resolve-avahi-compat/ga-client.h>
#include <resolve-avahi-compat/ga-service-browser.h>

static void on_new_service(GaServiceBrowser *browser,
                           gint interface,
                           GaProtocol protocol,
                           const gchar *name,
                           const gchar *type,
                           const gchar *domain,
                           GaLookupResultFlags flags,
                           gpointer user_data) {
    g_print("Found service: %s (type: %s, domain: %s)\n", name, type, domain);
}

int main(int argc, char *argv[]) {
    GError *error = NULL;
    
    GaClient *client = ga_client_new(GA_CLIENT_FLAG_NO_FLAGS);
    if (!ga_client_start(client, &error)) {
        g_printerr("Failed to start client: %s\n", error->message);
        g_error_free(error);
        return 1;
    }
    
    GaServiceBrowser *browser = ga_service_browser_new("_http._tcp");
    g_signal_connect(browser, "new-service", G_CALLBACK(on_new_service), NULL);
    
    if (!ga_service_browser_attach(browser, client, &error)) {
        g_printerr("Failed to start browsing: %s\n", error->message);
        g_error_free(error);
        return 1;
    }
    
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    
    g_object_unref(browser);
    g_object_unref(client);
    
    return 0;
}
```

### Example: Service Publishing

```c
#include <resolve-avahi-compat/ga-client.h>
#include <resolve-avahi-compat/ga-entry-group.h>

static void on_state_changed(GaEntryGroup *group,
                              GaEntryGroupState state,
                              gpointer user_data) {
    switch (state) {
        case GA_ENTRY_GROUP_STATE_ESTABLISHED:
            g_print("Service published successfully\n");
            break;
        case GA_ENTRY_GROUP_STATE_FAILURE:
            g_print("Failed to publish service\n");
            break;
        default:
            break;
    }
}

int main(int argc, char *argv[]) {
    GError *error = NULL;

    GaClient *client = ga_client_new(GA_CLIENT_FLAG_NO_FLAGS);
    if (!ga_client_start(client, &error)) {
        g_printerr("Failed to start client: %s\n", error->message);
        return 1;
    }

    GaEntryGroup *group = ga_entry_group_new();
    g_signal_connect(group, "state-changed", G_CALLBACK(on_state_changed), NULL);

    if (!ga_entry_group_attach(group, client, &error)) {
        g_printerr("Failed to attach: %s\n", error->message);
        return 1;
    }

    /* Add a service */
    GaEntryGroupService *service = ga_entry_group_add_service(
        group, "My Web Server", "_http._tcp", 8080, &error, NULL);

    if (!service) {
        g_printerr("Failed to add service: %s\n", error->message);
        return 1;
    }

    /* Add TXT records */
    ga_entry_group_service_set(service, "path", "/api", NULL);
    ga_entry_group_service_set(service, "version", "1.0", NULL);

    /* Commit (writes .dnssd file and signals systemd-resolved) */
    if (!ga_entry_group_commit(group, &error)) {
        g_printerr("Failed to commit: %s\n", error->message);
        return 1;
    }

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    g_object_unref(group);
    g_object_unref(client);

    return 0;
}
```

## API Differences from avahi-gobject

### Drop-in Compatibility Headers

The library provides complete Avahi header compatibility for seamless migration:

| Avahi Header | Compatibility Header | Description |
|--------------|---------------------|-------------|
| `<avahi-gobject/ga-client.h>` | ✅ Full support | GObject client API |
| `<avahi-gobject/ga-service-browser.h>` | ✅ Full support | Service discovery |
| `<avahi-gobject/ga-service-resolver.h>` | ✅ Full support | Service resolution |
| `<avahi-gobject/ga-record-browser.h>` | ✅ Full support | DNS record queries |
| `<avahi-gobject/ga-entry-group.h>` | ✅ Full support | Service publishing |
| `<avahi-gobject/ga-enums.h>` | ✅ Full support | Enumerations |
| `<avahi-gobject/ga-error.h>` | ✅ Full support | Error codes |
| `<avahi-client/client.h>` | ✅ Type aliases | Low-level C API |
| `<avahi-client/lookup.h>` | ✅ Type aliases | Browser/resolver types |
| `<avahi-client/publish.h>` | ✅ Type aliases | Entry group types |
| `<avahi-common/address.h>` | ✅ Full support | Address types |
| `<avahi-common/strlst.h>` | ✅ Full support | String list functions |
| `<avahi-common/error.h>` | ✅ Full support | AVAHI_ERR_* codes |
| `<avahi-common/defs.h>` | ✅ Full support | Constants and flags |
| `<avahi-common/malloc.h>` | ✅ Maps to GLib | Memory allocation |
| `<avahi-common/watch.h>` | ✅ Stub | Poll abstraction |
| `<avahi-common/cdecl.h>` | ✅ Full support | C++ macros |
| `<avahi-common/gccmacro.h>` | ✅ Full support | GCC attributes |
| `<avahi-glib/glib-watch.h>` | ✅ Stub | GLib main loop |
| `<avahi-glib/glib-malloc.h>` | ✅ Maps to GLib | GLib allocator |

### Type Compatibility

The library re-exports all types with both `Ga*` and `Avahi*` prefixes for maximum compatibility:

| This Library | Avahi Equivalent | Notes |
|--------------|------------------|-------|
| `GaClient` | `AvahiClient` | Type alias provided |
| `GaServiceBrowser` | `AvahiServiceBrowser` | Type alias provided |
| `GaServiceResolver` | `AvahiServiceResolver` | Type alias provided |
| `GaRecordBrowser` | `AvahiRecordBrowser` | Type alias provided |
| `GaEntryGroup` | `AvahiEntryGroup` | Type alias provided |
| `GaIfIndex` | `AvahiIfIndex` | Both defined |
| `GaProtocol` | `AvahiProtocol` | Both defined |
| `GaAddress` | `AvahiAddress` | Struct-compatible |
| `GaStringList` | `AvahiStringList` | Both defined |
| `GaClientState` | `AvahiClientState` | Both defined |
| `GaClientFlags` | `AvahiClientFlags` | Both defined |
| `GaPublishFlags` | `AvahiPublishFlags` | Both defined |
| `GaLookupResultFlags` | `AvahiLookupResultFlags` | Both defined |
| `GaLookupFlags` | `AvahiLookupFlags` | Both defined |
| `GaBrowserEvent` | `AvahiBrowserEvent` | Both defined |
| `GaResolverEvent` | `AvahiResolverEvent` | Both defined |

### Constant Compatibility

Constants are also aliased:

**Interface and Protocol:**
- `GA_IF_UNSPEC` = `AVAHI_IF_UNSPEC`
- `GA_PROTO_INET` = `AVAHI_PROTO_INET`
- `GA_PROTO_INET6` = `AVAHI_PROTO_INET6`
- `GA_PROTO_UNSPEC` = `AVAHI_PROTO_UNSPEC`

**Client States:**
- `AVAHI_CLIENT_S_REGISTERING`, `AVAHI_CLIENT_S_RUNNING`, `AVAHI_CLIENT_S_COLLISION`
- `AVAHI_CLIENT_FAILURE`, `AVAHI_CLIENT_CONNECTING`

**Client Flags:**
- `AVAHI_CLIENT_NO_FLAGS`, `AVAHI_CLIENT_IGNORE_USER_CONFIG`, `AVAHI_CLIENT_NO_FAIL`

**DNS Types and Classes:**
- `AVAHI_DNS_TYPE_A`, `AVAHI_DNS_TYPE_AAAA`, `AVAHI_DNS_TYPE_TXT`, `AVAHI_DNS_TYPE_SRV`, etc.
- `AVAHI_DNS_CLASS_IN`

**Error Codes:**
- All `AVAHI_ERR_*` codes map to corresponding `GA_ERROR_*` values

**Other:**
- `AVAHI_PUBLISH_*`, `AVAHI_LOOKUP_*`, `AVAHI_BROWSER_*`, `AVAHI_RESOLVER_*`
- `AVAHI_DEFAULT_TTL`, `AVAHI_DEFAULT_TTL_HOST_NAME`

### String List Functions

The `avahi_string_list_*` functions are provided as macros mapping to GLib:

```c
avahi_string_list_new()           /* → ga_string_list_new() */
avahi_string_list_free()          /* → ga_string_list_free() */
avahi_string_list_add()           /* → ga_string_list_add() */
avahi_string_list_add_pair()      /* → ga_string_list_add_pair() */
avahi_string_list_find()          /* → ga_string_list_find() */
avahi_string_list_get_pair()      /* → ga_string_list_get_pair() */
avahi_string_list_get_next()      /* → ga_string_list_get_next() */
avahi_string_list_get_text()      /* → ga_string_list_get_text() */
avahi_string_list_get_size()      /* → ga_string_list_get_size() */
avahi_string_list_length()        /* → ga_string_list_length() */
avahi_string_list_to_string()     /* → ga_string_list_to_string() */
```

### Signal Differences

Signals emit the same parameters and the signal signatures are fully compatible.

## systemd-resolved Configuration

Ensure mDNS is enabled in systemd-resolved:

```bash
# Check current configuration
resolvectl mdns

# Enable mDNS on an interface
sudo resolvectl mdns <interface> yes
```

Or configure in `/etc/systemd/resolved.conf`:

```ini
[Resolve]
MulticastDNS=yes
```

## License

LGPL-2.1-or-later (same as avahi-gobject)

## Migrating from Avahi

For applications currently using `avahi-gobject`:

1. **Update meson.build**: Replace `avahi-gobject`/`avahi-client`/`avahi-glib` dependencies with `resolve-avahi-compat` (see "Switching from Avahi in Meson" above)
2. **No code changes needed**: The Avahi-compatible headers allow existing code to compile unchanged

For applications using the low-level `avahi-client` API, note that this library provides type aliases but implements the GObject-based API internally. The `avahi-client/*.h` headers are provided for source compatibility with applications that include them.

## See Also

- [systemd-resolved](https://www.freedesktop.org/software/systemd/man/systemd-resolved.service.html)
- [Varlink](https://varlink.org/)
- [Avahi](https://avahi.org/)
