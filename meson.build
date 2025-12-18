# resolve-avahi-compat - Meson build file
#
# A drop-in replacement for avahi-gobject using systemd-resolved

project('resolve-avahi-compat', 'c',
  version : '0.1.0',
  default_options : [
    'warning_level=2',
    'c_std=gnu11',
  ],
  license : 'LGPL-2.1-or-later',
)

# Dependencies
glib_dep = dependency('glib-2.0', version : '>=2.56')
gobject_dep = dependency('gobject-2.0', version : '>=2.56')
gio_dep = dependency('gio-2.0', version : '>=2.56')
libsystemd_dep = dependency('libsystemd', version : '>=259')

# Source files
sources = [
  'ga-client.c',
  'ga-enums.c',
  'ga-error.c',
  'ga-service-browser.c',
  'ga-service-resolver.c',
  'ga-record-browser.c',
  'ga-entry-group.c',
]

# Headers
headers = [
  'resolve-avahi-compat.h',
  'ga-client.h',
  'ga-enums.h',
  'ga-error.h',
  'ga-service-browser.h',
  'ga-service-resolver.h',
  'ga-record-browser.h',
  'ga-entry-group.h',
]

# Build the library
lib = library('resolve-avahi-compat',
  sources,
  dependencies : [glib_dep, gobject_dep, gio_dep, libsystemd_dep],
  install : true,
  version : meson.project_version(),
)

# Install headers
install_headers(headers, subdir : 'resolve-avahi-compat')

# Avahi-compatible wrapper headers for drop-in replacement
# These allow applications using #include <avahi-gobject/ga-client.h> etc. to work

avahi_gobject_headers = [
  'avahi-gobject/ga-client.h',
  'avahi-gobject/ga-enums.h',
  'avahi-gobject/ga-error.h',
  'avahi-gobject/ga-service-browser.h',
  'avahi-gobject/ga-service-resolver.h',
  'avahi-gobject/ga-record-browser.h',
  'avahi-gobject/ga-entry-group.h',
]

avahi_common_headers = [
  'avahi-common/address.h',
  'avahi-common/cdecl.h',
  'avahi-common/defs.h',
  'avahi-common/error.h',
  'avahi-common/gccmacro.h',
  'avahi-common/malloc.h',
  'avahi-common/strlst.h',
  'avahi-common/watch.h',
]

avahi_glib_headers = [
  'avahi-glib/glib-malloc.h',
  'avahi-glib/glib-watch.h',
]

avahi_client_headers = [
  'avahi-client/client.h',
  'avahi-client/lookup.h',
  'avahi-client/publish.h',
]

# Install avahi-compatible wrapper headers
install_headers(avahi_gobject_headers, subdir : 'resolve-avahi-compat/avahi-gobject')
install_headers(avahi_common_headers, subdir : 'resolve-avahi-compat/avahi-common')
install_headers(avahi_glib_headers, subdir : 'resolve-avahi-compat/avahi-glib')
install_headers(avahi_client_headers, subdir : 'resolve-avahi-compat/avahi-client')

# Generate pkg-config file
pkg = import('pkgconfig')
pkg.generate(lib,
  name : 'resolve-avahi-compat',
  description : 'GObject-based Avahi-compatible API using systemd-resolved',
  requires : ['glib-2.0', 'gobject-2.0', 'gio-2.0', 'libsystemd'],
)

# Create a dependency for use as subproject
# Use '..' so that includes like <resolve-avahi-compat/ga-client.h> work
resolve_avahi_compat_dep = declare_dependency(
  link_with : lib,
  include_directories : include_directories('..'),
  dependencies : [glib_dep, gobject_dep, gio_dep, libsystemd_dep],
)

# Examples (optional)
if get_option('examples')
  executable('example-browse',
    'example-browse.c',
    link_with : lib,
    dependencies : [glib_dep, gobject_dep, gio_dep, libsystemd_dep],
  )

  executable('example-publish',
    'example-publish.c',
    link_with : lib,
    dependencies : [glib_dep, gobject_dep, gio_dep],
  )
endif
