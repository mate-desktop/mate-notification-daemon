AM_CPPFLAGS = $(THEME_CFLAGS)

enginedir = $(libdir)/mate-notification-daemon/engines
engine_LTLIBRARIES = libcoco.la

libcoco_la_SOURCES = coco-theme.c
libcoco_la_CFLAGS = $(WARN_CFLAGS)
libcoco_la_LDFLAGS = -module -avoid-version -no-undefined
libcoco_la_LIBADD = $(THEME_LIBS)
