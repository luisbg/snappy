NULL =

public_headers = \
		utils.h \
		user_interface.h \
		gst_engine.h

c_sources = \
	utils.c \
	user_interface.c \
	gst_engine.c \
	snappy.c

CLEANFILES =

bin_PROGRAMS = snappy

snappy_SOURCES = $(c_sources)
snappy_CFLAGS = $(CLUTTER_CFLAGS) $(GSTREAMER_0_10_CFLAGS) $(CLUTTER_GST_CFLAGS) $(GIO_CFLAGS) $(XSCRNSAVER_CFLAGS) -DSNAPPY_DATA_DIR="\"$(pkgdatadir)\""
snappy_LDADD = $(CLUTTER_LIBS) $(GSTREAMER_0_10_LIBS) $(CLUTTER_GST_LIBS) $(GIO_LIBS) $(XSCRNSAVER_LIBS)

noinst_HEADERS = $(public_headers)
