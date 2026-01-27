#!/usr/bin/bash

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Fedora
requires=(
	ccache # Use ccache to speed up build
)

requires+=(
	autoconf-archive
	desktop-file-utils
	file
	git
	libcanberra-devel
	libnotify-devel
	libwnck3-devel
	libxml2-devel
	make
	mate-common
	mate-desktop-devel
	mate-panel-devel
)

infobegin "Update system"
dnf update -y
infoend

infobegin "Install dependency packages"
dnf install -y ${requires[@]}
infoend
