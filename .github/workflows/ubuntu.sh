#!/usr/bin/bash

set -eo pipefail

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Ubuntu
requires=(
	ccache # Use ccache to speed up build
)

# https://git.launchpad.net/ubuntu/+source/mate-notification-daemon/tree/debian/control
requires+=(
	autoconf-archive
	autopoint
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
apt-get update -y
infoend

infobegin "Install dependency packages"
env DEBIAN_FRONTEND=noninteractive \
	apt-get install --assume-yes \
	${requires[@]}
infoend
