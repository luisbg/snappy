/*
 * snappy - 1.0
 *
 * Copyright (C) 2011-2014 Collabora Ltd.
 * <luis@debethencourt.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <glib.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include <string.h>

G_BEGIN_DECLS

gchar * cut_long_filename (gchar * filename, gint length);
gchar * clean_uri (gchar * input_arg);
gchar * clean_brackets_in_uri (gchar * uri);
gchar * strip_filename_extension (gchar * filename);

G_END_DECLS
#endif /* __UTILS_H__ */
