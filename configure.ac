AC_PREREQ(2.60)
dnl initialize autoconf
dnl when going to/from release please set the nano (fourth number) right !
dnl releases only do Wall, cvs and prerelease does Werror too
AC_INIT([snappy],
    [0.1],
    NULL,
    snappy)

AC_CONFIG_SRCDIR([src/snappy.c])
AM_CONFIG_HEADER([config.h])
AC_CONFIG_AUX_DIR([build-aux])

dnl initialize automake
AM_INIT_AUTOMAKE([1.9 dist-bzip2 no-dist-gzip])

dnl use pretty build output with automake >= 1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],
  [AM_DEFAULT_VERBOSITY=1
   AC_SUBST(AM_DEFAULT_VERBOSITY)])
m4_define([_LT_AC_TAGCONFIG],[])

dnl find a compiler
AC_PROG_CC
AC_STDC_HEADERS
AM_PROG_LIBTOOL

GST_0_10_REQS=0.10.20
CLUTTER_REQS=1.2.0
CLUTTER_GST_1_0_REQS=1.0.0
CLUTTER_GST_0_10_REQS=0.10.0
GLIB_REQ=2.10.0
GIO_REQUIRED=2.26
XSCRNSAVER_REQS=1.2.0

PKG_CHECK_MODULES([GSTREAMER_0_10], \
    [gstreamer-0.10 >= $GST_0_10_REQS 
    gstreamer-base-0.10 >= $GST_0_10_REQS
    gstreamer-plugins-base-0.10 >= $GST_0_10_REQS])
AC_SUBST(GSTREAMER_0_10_CFLAGS)
AC_SUBST(GSTREAMER_0_10_LIBS)

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

PKG_CHECK_MODULES(XSCRNSAVER, xscrnsaver >= $XSCRNSAVER_REQS)
AC_SUBST(XSCRNSAVER_CFLAGS)
AC_SUBST(XSCRNSAVER_LIBS)

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
	Vala bindings              : ${enable_vala}
	Python bindings:           : ${HAVE_PYTHON_BINDINGS}

snappy configured. Type 'make' to build.
"
