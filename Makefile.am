SUBDIRS = data po src

# Point to our macro directory and pick up user flags from the environment
ACLOCAL_AMFLAGS  = -I m4 ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	AUTHORS NEWS COPYING README \
	ChangeLog \
	autogen.sh \
	configure.ac \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

$(OBJECTS): libtool
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

DISTCHECK_CONFIGURE_FLAGS = --disable-icon-update

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

MAINTAINERCLEANFILES = \
        $(srcdir)/INSTALL \
        $(srcdir)/aclocal.m4 \
        $(srcdir)/autoscan.log \
        $(srcdir)/compile \
        $(srcdir)/config.guess \
        $(srcdir)/config.h.in \
        $(srcdir)/config.sub \
        $(srcdir)/configure.scan \
        $(srcdir)/depcomp \
        $(srcdir)/install-sh \
        $(srcdir)/ltmain.sh \
        $(srcdir)/missing \
        $(srcdir)/mkinstalldirs \
        $(srcdir)/omf.make \
        $(srcdir)/xmldocs.make \
        $(srcdir)/gtk-doc.make \
        $(srcdir)/ChangeLog \
        `find "$(srcdir)" -type f -name Makefile.in -print`

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
