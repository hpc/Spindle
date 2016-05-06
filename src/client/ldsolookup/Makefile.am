pkglibexec_PROGRAMS = print_ldso_entry
pkglib_LTLIBRARIES = libprint_ldso_audit.la

print_ldso_entry_SOURCES = print_ldso_entry.c
libprint_ldso_audit_la_SOURCES = print_ldso_audit.c

print_ldso_entry_CPPFLAGS = -DLIBEXECDIR=\"$(pkglibexecdir)\"

libprint_ldso_audit_la_LDFLAGS = -shared -avoid-version

if BGQ_BLD
print_ldso_entry_LDFLAGS = -dynamic
endif


