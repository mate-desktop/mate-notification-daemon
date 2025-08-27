define generate_resources_deps
	$(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/$(1))
endef

bin_PROGRAMS = mate-notification-properties

if ENABLE_IN_PROCESS
pkglib_LTLIBRARIES = libmate-notification-applet.la
else
libexec_PROGRAMS = mate-notification-applet
endif
NULL =

mate_notification_properties_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/daemon \
	$(NOTIFICATION_CAPPLET_CFLAGS) \
	-DENGINES_DIR=\"$(libdir)/mate-notification-daemon/engines\" \
	-DNOTIFICATION_LOCALEDIR=\"$(datadir)/locale\" \
	$(WARN_CFLAGS) \
	$(NULL)

mate_notification_properties_LDADD = \
	$(NOTIFICATION_CAPPLET_LIBS) \
	$(NULL)

mate_notification_properties_resources_xml   = org.mate.notifications.properties.gresource.xml
mate_notification_properties_resources_deps  = $(call generate_resources_deps $(mate_notification_properties_resources_xml))
mate_notification_properties_resources_files = mate-notification-properties-resources.h mate-notification-properties-resources.c
$(mate_notification_properties_resources_files): $(mate_notification_properties_resources_xml) Makefile $(mate_notification_properties_resources_deps)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) $< \
		--target $@ --sourcedir $(srcdir) --generate --c-name notification_properties
mate_notification_properties_SOURCES = \
	$(mate_notification_properties_resources_files) \
	../common/constants.h \
	mate-notification-properties.c \
	$(NULL)

man_MANS = mate-notification-properties.1

if ENABLE_IN_PROCESS

libmate_notification_applet_la_SOURCES = \
	$(mate_notification_applet_resources_files) \
	../common/constants.h \
	mate-notification-applet.c \
	mate-notification-applet-dbus.h \
	mate-notification-applet-dbus.c \
	$(NULL)

libmate_notification_applet_la_CFLAGS = \
	-I$(top_srcdir)/src/common \
	$(NOTIFICATION_APPLET_CFLAGS) \
	-DMATELOCALEDIR=\"$(datadir)/locale\" \
	-DRESOURCE_PATH=\""/org/mate/panel/applet/notifications/"\" \
	$(WARN_CFLAGS) \
	$(NULL)

libmate_notification_applet_la_LIBADD = \
	$(NOTIFICATION_APPLET_LIBS) \
	$(NULL)

else
mate_notification_applet_SOURCES = \
	$(mate_notification_applet_resources_files) \
	../common/constants.h \
	mate-notification-applet.c \
	mate-notification-applet-dbus.h \
	mate-notification-applet-dbus.c \
	$(NULL)

mate_notification_applet_CFLAGS = \
	-I$(top_srcdir)/src/common \
	$(NOTIFICATION_APPLET_CFLAGS) \
	-DMATELOCALEDIR=\"$(datadir)/locale\" \
	-DRESOURCE_PATH=\""/org/mate/panel/applet/notifications/"\" \
	$(WARN_CFLAGS) \
	$(NULL)

mate_notification_applet_LDADD = \
	$(NOTIFICATION_APPLET_LIBS) \
	$(NULL)

endif

mate_notification_applet_resources_xml   = org.mate.panel.applet.notifications.gresource.xml
mate_notification_applet_resources_deps  = $(call generate_resources_deps $(mate_notification_applet_resources_xml))
mate_notification_applet_resources_files = mate-notification-applet-resources.h mate-notification-applet-resources.c
$(mate_notification_applet_resources_files): $(mate_notification_applet_resources_xml) Makefile $(mate_notification_applet_resources_deps)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) $< \
		--target $@ --sourcedir $(srcdir) --generate --c-name notification_applet


BUILT_SOURCES = \
	$(mate_notification_properties_resources_files) \
	$(mate_notification_applet_resources_files) \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

EXTRA_DIST = \
	$(man_MANS) \
	$(pixmap_DATA) \
	$(mate_notification_applet_resources_xml) \
	$(mate_notification_properties_resources_xml) \
	mate-notification-applet-menu.xml \
	mate-notification-properties.ui \
	mate-notification-applet-dbus.h \
	$(NULL)

-include $(top_srcdir)/git.mk
