dnl Process this file with autoconf to create configure.

dnl ################################################################
dnl # Initialize autoconf
dnl ################################################################
AC_INIT([mate-notification-daemon], [1.26.0], [https://mate-desktop.org])
AC_PREREQ(2.63)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_MACRO_DIR([m4])

dnl ################################################################
dnl # Version information
dnl ################################################################
NOTIFICATION_DAEMON_MAJOR_VERSION=1
NOTIFICATION_DAEMON_MINOR_VERSION=3
NOTIFICATION_DAEMON_MICRO_VERSION=0
NOTIFICATION_DAEMON_DEVEL_VERSION=0

NOTIFICATION_DAEMON_VERSION=$NOTIFICATION_DAEMON_MAJOR_VERSION.$NOTIFICATION_DAEMON_MINOR_VERSION.$NOTIFICATION_DAEMON_MICRO_VERSION

if test "x$NOTIFICATION_DAEMON_DEVEL_VERSION" != "x0"; then
	if test "x$NOTIFICATION_DAEMON_MICRO_VERSION" = "x0"; then
		NOTIFICATION_DAEMON_MICRO_VERSION=$NOTIFICATION_DAEMON_DEVEL_VERSION
		NOTIFICATION_DAEMON_VERSION=$NOTIFICATION_DAEMON_MAJOR_VERSION.$NOTIFICATION_DAEMON_MINOR_VERSION.$NOTIFICATION_DAEMON_DEVEL_VERSION
	else
		NOTIFICATION_DAEMON_VERSION=$NOTIFICATION_DAEMON_VERSION.$NOTIFICATION_DAEMON_DEVEL_VERSION
	fi
fi

AC_DEFINE_UNQUOTED(NOTIFICATION_DAEMON_MAJOR_VERSION, $NOTIFICATION_DAEMON_MAJOR_VERSION,
	[mate-notification-daemon major version.])
AC_DEFINE_UNQUOTED(NOTIFICATION_DAEMON_MINOR_VERSION, $NOTIFICATION_DAEMON_MINOR_VERSION,
	[mate-notification-daemon minor version.])
AC_DEFINE_UNQUOTED(NOTIFICATION_DAEMON_MICRO_VERSION, $NOTIFICATION_DAEMON_MICRO_VERSION,
	[mate-notification-daemon micro version.])
AC_DEFINE_UNQUOTED(NOTIFICATION_DAEMON_VERSION, "$NOTIFICATION_DAEMON_VERSION",
	[mate-notification-daemon version.])

dnl ################################################################
dnl # Initialize automake
dnl ################################################################
AM_INIT_AUTOMAKE([1.10 no-dist-gzip dist-xz foreign check-news])
AM_MAINTAINER_MODE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl ################################################################
dnl # Check for some standard stuff.
dnl ################################################################
# Checks for programs
AC_PROG_CC
AC_PROG_CPP
AC_PROG_MAKE_SET
AC_PROG_LN_S
AC_PROG_INSTALL

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

dnl ---------------------------------------------------------------------------
dnl Warning flags
dnl ---------------------------------------------------------------------------
MATE_MAINTAINER_MODE_DEFINES
MATE_COMPILE_WARNINGS

dnl ---------------------------------------------------------------------------
dnl Initialize Libtool
dnl ---------------------------------------------------------------------------
LT_PREREQ([2.2.6])
LT_INIT


dnl ---------------------------------------------------------------------------
dnl i18n stuff
dnl ---------------------------------------------------------------------------

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19.8])
AM_CONDITIONAL([USE_NLS], [test "x${USE_NLS}" = "xyes"])

AC_SUBST([GETTEXT_PACKAGE], [mate-notification-daemon])
AC_DEFINE([GETTEXT_PACKAGE], [PACKAGE_TARNAME], [Define to the gettext package name.])


dnl ---------------------------------------------------------------------------
dnl GLIB_COMPILE_RESOURCES
dnl ---------------------------------------------------------------------------
AC_ARG_VAR([GLIB_COMPILE_RESOURCES],[glib-compile-resources bin])
AC_PATH_PROG([GLIB_COMPILE_RESOURCES],[glib-compile-resources],[])
if test -z "$GLIB_COMPILE_RESOURCES"; then
  AC_MSG_ERROR([glib-compile-resources not found])
fi

dnl ---------------------------------------------------------------------------
dnl XMLLINT
dnl ---------------------------------------------------------------------------
AC_ARG_VAR([XMLLINT],[xmllint bin])
AC_PATH_PROG([XMLLINT],[xmllint],[])
if test -z "$XMLLINT"; then
  AC_MSG_ERROR([xmllint not found])
fi

dnl ---------------------------------------------------------------------------
dnl Requirements for the themes
dnl ---------------------------------------------------------------------------

GTK_REQUIRED=3.22.0
XML_REQUIRED=2.9.0

PKG_CHECK_MODULES(THEME,
  gtk+-3.0 >= $GTK_REQUIRED
  libxml-2.0 >= $XML_REQUIRED
)
AC_SUBST(THEME_CFLAGS)
AC_SUBST(THEME_LIBS)

dnl ---------------------------------------------------------------------------
dnl Requirements for the daemon
dnl ---------------------------------------------------------------------------
REQ_GLIB_VERSION=2.50.0
REQ_LIBCANBERRA_GTK_VERSION=0.4
PKG_CHECK_MODULES(GMODULE,gmodule-2.0,[GMODULE_ADD="gmodule-2.0"],[GMODULE_ADD=""])
pkg_modules="
	gtk+-3.0 >= $GTK_REQUIRED, \
	glib-2.0 >= $REQ_GLIB_VERSION, \
	gio-2.0 >= $REQ_GLIB_VERSION, \
        $GMODULE_ADD \
        libcanberra-gtk3 >= $REQ_LIBCANBERRA_GTK_VERSION \
"
PKG_CHECK_MODULES(NOTIFICATION_DAEMON, $pkg_modules)

dnl ---------------------------------------------------------------------------
dnl Requirements for panel applet
dnl ---------------------------------------------------------------------------
LIBPANEL4_REQUIRED=1.17.0
LIBMATE_DESKTOP_REQUIRED=1.17.0
PKG_CHECK_MODULES(NOTIFICATION_APPLET, mate-desktop-2.0 >= $LIBMATE_DESKTOP_REQUIRED libmatepanelapplet-4.0 >= $LIBPANEL4_REQUIRED gtk+-3.0 >= $GTK_REQUIRED glib-2.0 >= $REQ_GLIB_VERSION)
AC_SUBST(NOTIFICATION_APPLET_CFLAGS)
AC_SUBST(NOTIFICATION_APPLET_LIBS)

dnl Will call AC_SUBSET on NOTIFICATION_DAEMON_* below after appending X11 and Wayland flags

dnl ---------------------------------------------------------------------------
dnl Support X11, Wayland or both for the daemon
dnl ---------------------------------------------------------------------------

dnl $enable_x11 and $enable_wayland will be set to "yes", "no" or "auto"
AC_ARG_ENABLE(x11,
	[AS_HELP_STRING([--enable-x11],
		[Explicitly enable or disable X11 support
		(default is to enable only if X development libraries are detected)])],
	[enable_x11=$enableval],
	[enable_x11=auto])

AC_ARG_ENABLE(wayland,
	[AS_HELP_STRING([--enable-wayland],
		[Explicitly enable or disable Wayland support
		(default is to enable only if Wayland client development library is detected)])],
	[enable_wayland=$enableval],
	[enable_wayland=auto])

dnl Check if we have gtk-layer-shell installed, and thus should build with Wayland support
have_wayland=no
if test "x$enable_wayland" != "xno"; then
	PKG_CHECK_MODULES(NOTIFICATION_DAEMON_WAYLAND, gtk-layer-shell-0, [
		have_wayland=yes
		AC_DEFINE(HAVE_WAYLAND, 1, [Have the Wayland development library])
		NOTIFICATION_DAEMON_CFLAGS="$NOTIFICATION_DAEMON_CFLAGS $NOTIFICATION_DAEMON_WAYLAND_CFLAGS"
		NOTIFICATION_DAEMON_LIBS="$NOTIFICATION_DAEMON_LIBS $NOTIFICATION_DAEMON_WAYLAND_LIBS"
	], [
		if test "x$enable_wayland" = "xyes"; then
			AC_MSG_ERROR([Wayland enabled but GTK Layer Shell library not found])
		fi
	])
fi

dnl Check if we have the X development libraries
have_x11=no
if test "x$enable_x11" != "xno"; then
	x11_pkg_modules="libwnck-3.0, x11"
	PKG_CHECK_MODULES(NOTIFICATION_DAEMON_X11, $x11_pkg_modules, [
		have_x11=yes
		AC_DEFINE(HAVE_X11, 1, [Have the X11 development library])
		NOTIFICATION_DAEMON_CFLAGS="$NOTIFICATION_DAEMON_CFLAGS $NOTIFICATION_DAEMON_X11_CFLAGS"
		NOTIFICATION_DAEMON_LIBS="$NOTIFICATION_DAEMON_LIBS $NOTIFICATION_DAEMON_X11_LIBS"
	], [
		if test "x$enable_x11" = "xyes"; then
			AC_MSG_ERROR([X development libraries not found])
		fi
	])
fi

AM_CONDITIONAL(ENABLE_WAYLAND, [test "x$have_wayland" = "xyes"])

if test "x$have_x11" != "xyes" -a "x$have_wayland" != "xyes"; then
  AC_MSG_ERROR([At least one backend must be enabled (--enable-x11 or --enable-wayland)])
fi

AC_SUBST(NOTIFICATION_DAEMON_CFLAGS)
AC_SUBST(NOTIFICATION_DAEMON_LIBS)

dnl ---------------------------------------------------------------------------
dnl Requirements for the setup tool
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(NOTIFICATION_CAPPLET, glib-2.0 >= $REQ_GLIB_VERSION gio-2.0 >= $REQ_GLIB_VERSION gtk+-3.0 >= $GTK_REQUIRED libnotify)
AC_SUBST(NOTIFICATION_CAPPLET_CFLAGS)
AC_SUBST(NOTIFICATION_CAPPLET_LIBS)

gdk_modules="
	gdk-3.0 >= $GTK_REQUIRED, \
	gdk-pixbuf-2.0 >= 2.22.0 \
"
PKG_CHECK_MODULES(GDK, $gdk_modules, have_gdk=yes,
[
	have_gdk=no
	AC_MSG_WARN("Some test apps will not be built")
])

AM_CONDITIONAL(HAVE_GDK, test "x$have_gdk" = "xyes")

AC_SUBST(GDK_CFLAGS)
AC_SUBST(GDK_LIBS)

dnl
dnl Enable gsettings schema macros
dnl

GLIB_GSETTINGS

dnl
dnl Check the D-BUS version.
dnl

AC_MSG_CHECKING([dbus version])
DBUS_VERSION=`$PKG_CONFIG --modversion dbus-1`
DBUS_MAJOR_VER=`echo $DBUS_VERSION | cut -d. -f 1`
DBUS_MINOR_VER=`echo $DBUS_VERSION | cut -d. -f 2`
DBUS_MICRO_VER=`echo $DBUS_VERSION | cut -d. -f 3`

AC_MSG_RESULT($DBUS_VERSION)
AC_DEFINE_UNQUOTED(DBUS_MAJOR_VER, $DBUS_MAJOR_VER, [D-BUS major version.])
AC_DEFINE_UNQUOTED(DBUS_MINOR_VER, $DBUS_MINOR_VER, [D-BUS minor version.])
AC_DEFINE_UNQUOTED(DBUS_MICRO_VER, $DBUS_MICRO_VER, [D-BUS micro version.])

AC_DEFINE_UNQUOTED(DBUS_API_SUBJECT_TO_CHANGE, ,DBUS API is subject to change)

AS_AC_EXPAND(SYSCONFDIR, $sysconfdir)
AS_AC_EXPAND(LIBDIR, $libdir)
AS_AC_EXPAND(DATADIR, $datadir)
AS_AC_EXPAND(LIBEXECDIR, $libexecdir)

AC_ARG_WITH(dbus-sys, [  --with-dbus-sys=<dir>   where D-BUS system.d directory is])
AC_ARG_WITH(dbus-services, [  --with-dbus-services=<dir>   where D-BUS services directory is])

if ! test -z "$with_dbus_sys" ; then
	DBUS_SYS_DIR="$with_dbus_sys"
else
	DBUS_SYS_DIR="$SYSCONFDIR/dbus-1/system.d"
fi

if ! test -z "$with_dbus_services" ; then
	DBUS_SERVICES_DIR="$with_dbus_services"
else
	DBUS_SERVICES_DIR="$DATADIR/dbus-1/services"
fi

AC_SUBST(DBUS_SYS_DIR)
AC_SUBST(DBUS_SERVICES_DIR)
AC_DEFINE_UNQUOTED(DBUS_SYSTEMD_DIR, "$DBUS_SYS_DIR", [Where system.d dir for DBUS is])
AC_DEFINE_UNQUOTED(DBUS_SERVICES_DIR, "$DBUS_SERVICES_DIR", [Where services dir for DBUS is])

dnl ---------------------------------------------------------------------------
dnl Enabel/disable icon cache generation
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(icon-update, AS_HELP_STRING([--disable-icon-update],
                                        [Disable icon cache update]))
if (test "$enable_icon_update" != no); then
        AC_PATH_PROG(UPDATE_ICON_CACHE, [gtk-update-icon-cache])
fi
AM_CONDITIONAL([ICON_UPDATE], [test -n "$UPDATE_ICON_CACHE"])

AC_CONFIG_FILES([
Makefile
data/Makefile
data/org.freedesktop.mate.Notifications.service
data/org.mate.applets.MateNotificationApplet.desktop.in
data/org.mate.NotificationDaemon.gschema.xml
data/org.mate.panel.applet.MateNotificationAppletFactory.service
po/Makefile.in
src/Makefile
src/daemon/Makefile
src/capplet/Makefile
src/themes/Makefile
src/themes/coco/Makefile
src/themes/nodoka/Makefile
src/themes/slider/Makefile
src/themes/standard/Makefile
])

AC_OUTPUT

echo "
Configure summary:

    ${PACKAGE_STRING}
    `echo $PACKAGE_STRING | sed "s/./=/g"`

    prefix ......................: ${prefix}
    exec_prefix .................: ${exec_prefix}
    bindir ......................: ${bindir}
    datadir .....................: ${datadir}
    source code location ........: ${srcdir}
    compiler ....................: ${CC}
    cflags ......................: ${CFLAGS}
    warning flags ...............: ${WARN_CFLAGS}
    Wayland support .............: ${have_wayland}
    X11 support .................: ${have_x11}
    Native Language support .....: ${USE_NLS}

    dbus-1 system.d .............: $DBUS_SYS_DIR
    dbus-1 services .............: $DBUS_SERVICES_DIR

"
