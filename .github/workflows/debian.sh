#!/usr/bin/bash

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Debian
requires=(
	ccache # Use ccache to speed up build
)

requires+=(
	autoconf-archive
	autopoint
	gcc
	git
	gobject-introspection
	gtk-doc-tools
	libcanberra-gtk3-dev
	libdconf-dev
	libexempi-dev
	libexif-dev
	libgail-3-dev
	libgirepository1.0-dev
	libglib2.0-dev
	libgtk-3-dev
	libmate-desktop-dev
	libmate-panel-applet-dev
	libnotify-dev
	libpango1.0-dev
	libstartup-notification0-dev
	libwnck-3-dev
	libx11-dev
	libxml2-dev
	libxml2-utils
	mate-common
	quilt
	shared-mime-info
)

infobegin "Update system"
apt-get update -qq
infoend

infobegin "Install dependency packages"
env DEBIAN_FRONTEND=noninteractive \
	apt-get install --assume-yes \
	${requires[@]}
infoend
