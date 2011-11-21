AC_PREREQ(2.60)
dnl initialize autoconf
dnl when going to/from release please set the nano (fourth number) right !
dnl releases only do Wall, cvs and prerelease does Werror too
AC_INIT([snappy],
    [0.2],
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

AC_MSG_CHECKING([for Win32 host])
case "$host" in
  *-*-mingw*)
    os_win32=yes
    ;;
  *)
    os_win32=no
    ;;
esac
AC_MSG_RESULT([$os_win32])

GST_0_10_REQS=0.10.20
GST_PBUTILS=0.10.30
CLUTTER_REQS=1.2.0
CLUTTER_GST_1_0_REQS=1.0.0
CLUTTER_GST_0_10_REQS=0.10.0
GLIB_REQ=2.10.0
GIO_REQUIRED=2.24

PKG_CHECK_MODULES([GSTREAMER_0_10], \
    [gstreamer-0.10 >= $GST_0_10_REQS 
    gstreamer-base-0.10 >= $GST_0_10_REQS
    gstreamer-plugins-base-0.10 >= $GST_0_10_REQS])
AC_SUBST(GSTREAMER_0_10_CFLAGS)
AC_SUBST(GSTREAMER_0_10_LIBS)

PKG_CHECK_MODULES([GST_PBUTILS],
    [gstreamer-pbutils-0.10 >= $GST_PBUTILS])
AC_SUBST(GST_PBUTILS_CFLAGS)
AC_SUBST(GST_PBUTILS_LIBS)

PKG_CHECK_MODULES([CLUTTER], 
    [clutter-1.0 >= $CLUTTER_REQS])
AC_SUBST(CLUTTER_CFLAGS)
AC_SUBST(CLUTTER_LIBS)

PKG_CHECK_MODULES([CLUTTER_GST], 
    [clutter-gst-1.0 >= $CLUTTER_GST_1_0_REQS],
		[
			HAVE_CLUTTER_GST_1_0="yes"
		], HAVE_CLUTTER_GST_1_0="no")

if test "x$HAVE_CLUTTER_GST_1_0" = "xno"; then
	dnl If we don't have clutter-gst-1.0, then check for clutter-gst-0.10
	PKG_CHECK_MODULES(CLUTTER_GST_0_10,
	[clutter-gst-0.10 >= $CLUTTER_GST_0_10_REQS])
	CLUTTER_GST_CFLAGS="\$(CLUTTER_GST_0_10_CFLAGS)"
	CLUTTER_GST_LIBS="\$(CLUTTER_GST_0_10_LIBS)"
fi
AC_SUBST(CLUTTER_GST_CFLAGS)
AC_SUBST(CLUTTER_GST_LIBS)

AC_SUBST([GLIB_REQ])

PKG_CHECK_MODULES(GIO, gio-2.0 >= $GIO_REQUIRED)
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

if test "$os_win32" = "xyes"; then
  PKG_CHECK_MODULES(CLUTTER_WIN32, clutter-win32-1.0,
      [AC_DEFINE(HAVE_WIN32, 1, [Define if Clutter Win32 is available.])],
      [])
else
  PKG_CHECK_MODULES(CLUTTER_X11, clutter-x11-1.0,
      [AC_DEFINE(HAVE_X11, 1, [Define if Clutter X11 is available.])],
      [])
fi

PKG_CHECK_MODULES(XTEST, xtst,
    [AC_DEFINE(HAVE_XTEST, 1, [Define if XTest is available.])], [true])
AC_SUBST(XTEST_CFLAGS)
AC_SUBST(XTEST_LIBS)

AC_ARG_ENABLE([dbus],
    AS_HELP_STRING([--enable-dbus], [enable DBus support to disable the screensaver (default=no)]),
      [],
      [enable_dbus=no])

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
