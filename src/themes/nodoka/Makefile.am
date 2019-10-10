AM_CPPFLAGS = $(THEME_CFLAGS)

enginedir = $(libdir)/mate-notification-daemon/engines
engine_LTLIBRARIES = libnodoka.la

libnodoka_la_SOURCES = nodoka-theme.c
libnodoka_la_CFLAGS = $(WARN_CFLAGS)
libnodoka_la_LDFLAGS = -module -avoid-version -no-undefined
libnodoka_la_LIBADD =  $(THEME_LIBS)
