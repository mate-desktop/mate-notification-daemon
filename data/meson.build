po_dir = join_paths(meson.source_root(), 'po')

schema_data = configuration_data()
schema_data.set('GETTEXT_PACKAGE', meson.project_name())
configure_file(
  input : 'org.mate.NotificationDaemon.gschema.xml.in',
  output : 'org.mate.NotificationDaemon.gschema.xml',
  configuration : schema_data,
  install : true,
  install_dir : join_paths(mm_datadir, 'glib-2.0', 'schemas')
)

# .desktop file

desktop_data = configuration_data()
desktop_data.set('VERSION', meson.project_version())
desktop_data.set('LIBEXECDIR', mm_execdir)
desktop_data.set('APPLET_LOCATION', APPLET_LOCATION)
desktop_data.set('ENABLE_IN_PROCESS', enable_process)
desktop_in_file = configure_file(
  input : 'mate-notification-daemon.desktop.in.in',
  output : 'mate-notification-daemon.desktop.in',
  configuration : desktop_data
)

i18n.merge_file(
  input : desktop_in_file,
  output : 'mate-notification-daemon.desktop',
  type : 'desktop',
  po_dir : po_dir,
  install : true,
  install_dir: join_paths(get_option('sysconfdir'), 'xdg', 'autostart')
)

desktop_in_file = configure_file(
  input : 'org.mate.applets.MateNotificationApplet.desktop.in.in',
  output : 'org.mate.applets.MateNotificationApplet.desktop.in',
  configuration : desktop_data
)

i18n.merge_file(
  input : desktop_in_file,
  output : 'org.mate.applets.MateNotificationApplet.desktop',
  type : 'desktop',
  po_dir : po_dir,
  install : true,
  install_dir : join_paths(mm_datadir, 'mate-panel', 'applets')
)

i18n.merge_file(
  input : 'mate-notification-properties.desktop.in',
  output : 'mate-notification-properties.desktop',
  type : 'desktop',
  po_dir : po_dir,
  install : true,
  install_dir : join_paths(mm_datadir, 'applications')
)

# .service files

service_data = configuration_data()
service_data.set('LIBEXECDIR', mm_execdir)

configure_file(
  input : 'org.mate.panel.applet.MateNotificationAppletFactory.service.in',
  output : 'org.mate.panel.applet.MateNotificationAppletFactory.service',
  configuration : service_data,
  install : true,
  install_dir : join_paths(mm_datadir, 'dbus-1/services')
)

configure_file(
  input : 'org.freedesktop.mate.Notifications.service.in',
  output : 'org.freedesktop.mate.Notifications.service',
  configuration : service_data,
  install : true,
  install_dir : join_paths(mm_datadir, 'dbus-1/services')
)

subdir('icons')
