libexec_PROGRAMS = mate-notification-daemon

mate_notification_daemon_SOURCES = \
	$(mate_notification_daemon_built_sources) \
	../common/constants.h \
	daemon.c \
	daemon.h \
	engines.c \
	engines.h \
	stack.c \
	stack.h \
	sound.c \
	sound.h \
	mnd-daemon.c

if ENABLE_WAYLAND
mate_notification_daemon_SOURCES += \
	wayland.h \
	wayland.c
endif

mate_notification_daemon_LDADD = $(NOTIFICATION_DAEMON_LIBS)

mate_notification_daemon_CFLAGS = $(WARN_CFLAGS)

mate_notification_daemon_built_sources =	\
	mnd-dbus-generated.h	\
	mnd-dbus-generated.c	\
	$(NULL)

$(mate_notification_daemon_built_sources) : Makefile.am $(srcdir)/notificationdaemon.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.freedesktop. \
		--c-namespace NotifyDaemon \
		--generate-c-code mnd-dbus-generated \
		$(srcdir)/notificationdaemon.xml

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/common \
	$(NOTIFICATION_DAEMON_CFLAGS) \
	-DENGINES_DIR=\"$(libdir)/mate-notification-daemon/engines\"

EXTRA_DIST = notificationdaemon.xml
DISTCLEANFILES = \
	$(mate_notification_daemon_built_sources)

-include $(top_srcdir)/git.mk
