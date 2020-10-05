enginedir = $(libdir)/mate-notification-daemon/engines

engine_LTLIBRARIES = libstandard.la

libstandard_la_SOURCES = theme.c
libstandard_la_CFLAGS = $(WARN_CFLAGS)
libstandard_la_LDFLAGS = -module -avoid-version -no-undefined
libstandard_la_LIBADD  = $(THEME_LIBS)

AM_CPPFLAGS = $(THEME_CFLAGS)

-include $(top_srcdir)/git.mk
