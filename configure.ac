AC_PREREQ(2.60)
dnl initialize autoconf
dnl when going to/from release please set the nano (fourth number) right !
dnl releases only do Wall, cvs and prerelease does Werror too
AC_INIT([snappy],
    [0.3.7],
    NULL,
    snappy)
AM_MAINTAINER_MODE

AC_CONFIG_SRCDIR([src/snappy.c])
AM_CONFIG_HEADER([config.h])
AC_CONFIG_AUX_DIR([build-aux])

dnl initialize automake
AM_INIT_AUTOMAKE([1.11.1 no-dist-gzip dist-xz dist-bzip2 tar-ustar])

dnl use pretty build output with automake >= 1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],
  [AM_DEFAULT_VERBOSITY=1
   AC_SUBST(AM_DEFAULT_VERBOSITY)])
m4_define([_LT_AC_TAGCONFIG],[])

dnl find a compiler
AC_PROG_CC
AM_PROG_CC_C_O
AC_STDC_HEADERS
AM_PROG_LIBTOOL

dnl instruct libtool machinery that the package supports DLLs
AC_LIBTOOL_WIN32_DLL

AC_CANONICAL_HOST

# keep one single GST_REQ, core/base should be same versions anyway
GST_REQ=1.0.0
CLUTTER_REQS=1.12.0
CLUTTER_GST_REQS=2.0.0
CLUTTER_GTK_REQS=1.0.2
GTK_REQS=3.5.0
GIO_REQ=2.32

PKG_CHECK_MODULES([GST], \
    [gstreamer-1.0 >= $GST_REQ
    gstreamer-base-1.0 >= $GST_REQ
    gstreamer-plugins-base-1.0 >= $GST_REQ
    gstreamer-pbutils-1.0 >= $GST_REQ
    gstreamer-video-1.0 >= $GST_REQ])

AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

PKG_CHECK_MODULES([CLUTTER],
    [clutter-1.0 >= $CLUTTER_REQS])
AC_SUBST(CLUTTER_CFLAGS)
AC_SUBST(CLUTTER_LIBS)

PKG_CHECK_MODULES([CLUTTER_GST], [clutter-gst-2.0 >= $CLUTTER_GST_REQS])
AC_SUBST(CLUTTER_GST_CFLAGS)
AC_SUBST(CLUTTER_GST_LIBS)

PKG_CHECK_MODULES([CLUTTER_GTK], [clutter-gtk-1.0 >= $CLUTTER_GTK_REQS])
AC_SUBST(CLUTTER_GTK_CFLAGS)
AC_SUBST(CLUTTER_GTK_LIBS)

PKG_CHECK_MODULES([GTK], [gtk+-3.0 >= $GTK_REQS])
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

PKG_CHECK_MODULES(GIO, gio-2.0 >= $GIO_REQ)
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

case "$host" in
  *-*-mingw*|*-*-cygwin*)
    PKG_CHECK_MODULES(CLUTTER_WIN32, clutter-win32-1.0,
      [AC_DEFINE(HAVE_WIN32, 1, [Define if Clutter Win32 is available.])],
      [])
    ;;
  *-*-darwin*)
    PKG_CHECK_MODULES(CLUTTER_OSX, clutter-osx-1.0,
      [AC_DEFINE(HAVE_OSX, 1, [Define if Clutter OSX is available.])],
      [
        PKG_CHECK_MODULES(CLUTTER_X11, clutter-x11-1.0,
          [AC_DEFINE(HAVE_X11, 1, [Define if Clutter X11 is available.])],
          [])
      ])
    ;;
  *)
    PKG_CHECK_MODULES(CLUTTER_X11, clutter-x11-1.0,
      [AC_DEFINE(HAVE_X11, 1, [Define if Clutter X11 is available.])],
      [])
    ;;
esac

PKG_CHECK_MODULES(XTEST, xtst,
    [AC_DEFINE(HAVE_XTEST, 1, [Define if XTest is available.])], [true])
AC_SUBST(XTEST_CFLAGS)
AC_SUBST(XTEST_LIBS)

AC_ARG_ENABLE([dbus],
    AS_HELP_STRING([--enable-dbus], [enable DBus support to disable the screensaver (default=yes)]),
      [],
      [enable_dbus=yes])

if test "x$enable_dbus" = "xyes"; then
  PKG_CHECK_MODULES(GIO_2_26, gio-2.0 >= 2.26,
      [AC_DEFINE(ENABLE_DBUS, 1, [Define if DBus support is enabled.])],
      [AC_MSG_ERROR([GIO >= 2.26 not found, which is required for DBus support])])
fi

dnl our libraries and install dirs use major.minor as a version
GST_MAJORMINOR=$PACKAGE_VERSION_MAJOR.$PACKAGE_VERSION_MINOR
dnl we override it here if we need to for the release candidate of new series
GST_MAJORMINOR=0.10
AC_SUBST(GST_MAJORMINOR)

AM_PROG_LIBTOOL

dnl Add parameters for aclocal
AC_SUBST(ACLOCAL_AMFLAGS, "-I m4 -I common/m4")
AC_CONFIG_MACRO_DIR([m4])

ROOT_UNINSTALLED_DIR="`pwd`"
AC_DEFINE_UNQUOTED(SHARE_UNINSTALLED_DIR,"`pwd`/$srcdir/data",[path to source data dir])
AC_DEFINE_UNQUOTED(SHARE_UNINSTALLED_BUILDDIR,"`pwd`/data",[path to built source data dir])
AC_SUBST(ROOT_UNINSTALLED_DIR)

dnl set license and copyright notice
GST_LICENSE="LGPL"
AC_DEFINE_UNQUOTED(GST_LICENSE, "$GST_LICENSE", [GStreamer license])
AC_SUBST(GST_LICENSE)

dnl keep this alphabetic per directory, please
AC_CONFIG_FILES([
Makefile
data/Makefile
data/icons/Makefile
data/icons/256x256/Makefile
data/icons/48x48/Makefile
src/Makefile
])
AC_OUTPUT

echo "

Configuration
	Version                    : ${VERSION}
	Source code location       : ${srcdir}
	Prefix                     : ${prefix}
	Compiler                   : ${CC}

snappy configured. Type 'make' to build.
"
