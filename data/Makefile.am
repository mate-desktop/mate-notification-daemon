icon16dir = $(datadir)/icons/hicolor/16x16/apps
icon22dir = $(datadir)/icons/hicolor/22x22/apps
icon24dir = $(datadir)/icons/hicolor/24x24/apps
icon32dir = $(datadir)/icons/hicolor/32x32/apps
icon48dir = $(datadir)/icons/hicolor/48x48/apps
iconscalabledir = $(datadir)/icons/hicolor/scalable/apps

icon16_DATA = icons/16x16/mate-notification-properties.png
icon22_DATA = icons/22x22/mate-notification-properties.png
icon24_DATA = icons/24x24/mate-notification-properties.png
icon32_DATA = icons/32x32/mate-notification-properties.png
icon48_DATA = icons/48x48/mate-notification-properties.png
iconscalable_DATA = icons/scalable/mate-notification-properties.svg

servicedir       = $(DBUS_SERVICES_DIR)
service_DATA     = org.freedesktop.mate.Notifications.service org.mate.panel.applet.MateNotificationAppletFactory.service
service_in_files = $(service_DATA:=.in)

autostartdir   = $(sysconfdir)/xdg/autostart
autostart_in_files = mate-notification-daemon.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

$(autostart_DATA): $(autostart_in_files)
if USE_NLS
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@
else
	$(AM_V_GEN) sed '/^# Translators/d' < $< > $@
endif

appletdir        = $(datadir)/mate-panel/applets
applet_DATA      = org.mate.applets.MateNotificationApplet.mate-panel-applet
applet_in_files  = $(applet_DATA:.mate-panel-applet=.desktop.in)

$(applet_DATA): $(applet_in_files)
if USE_NLS
	$(AM_V_GEN) $(MSGFMT) --desktop --keyword=Name --keyword=Description --template $< -d $(top_srcdir)/po -o $@
else
	$(AM_V_GEN) cp -f $< $@
endif

desktopdir       = $(datadir)/applications
desktop_DATA     = mate-notification-properties.desktop
desktop_in_files = $(desktop_DATA:.desktop=.desktop.in)

$(desktop_DATA): $(desktop_in_files)
if USE_NLS
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@
else
	$(AM_V_GEN) sed '/^# Translators/d' < $< > $@
endif

gsettings_SCHEMAS = org.mate.NotificationDaemon.gschema.xml
gsettingsschema_in_files = $(gsettings_SCHEMAS:=.in)
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(autostart_in_files) \
	$(desktop_in_files) \
	$(gsettingsschema_in_files) \
	$(icon16_DATA) \
	$(icon22_DATA) \
	$(icon24_DATA) \
	$(icon32_DATA) \
	$(icon48_DATA) \
	$(iconscalable_DATA)

CLEANFILES = \
	$(autostart_DATA) \
	$(applet_DATA) \
	$(desktop_DATA) \
	$(gsettings_SCHEMAS)

DISTCLEANFILES = \
	$(applet_in_files) \
	$(service_DATA)

gtk_update_icon_cache = $(UPDATE_ICON_CACHE) -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)" && ICON_UPDATE; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

-include $(top_srcdir)/git.mk
