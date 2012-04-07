/*
 * snappy - 0.2
 *
 * Copyright (C) 2011 Collabora Multimedia Ltd.
 * <luis.debethencourt@collabora.co.uk>
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

#include <glib.h>
#include <gio/gio.h>


gchar *
cut_long_filename (gchar * filename, gint length)
{
  gchar *ret;
  gint c;
  gchar short_filename[length];

  for (c = 0; filename[c] != '\0'; c++);

  if (c >= length) {
    for (c = 0; c < length; c++) {
      short_filename[c] = filename[c];
    }
    short_filename[length] = '\0';
    ret = g_filename_to_utf8 (short_filename, length, NULL, NULL, NULL);
  } else {
    ret = g_locale_to_utf8 (filename, -1, NULL, NULL, NULL);
  }

  if (ret == NULL)
    g_print ("no filename. really?\n");
  return ret;
}

gchar *
clean_uri (gchar * input_arg)
{
  GFile *gfile;
  gchar *fileuri;

  if (gst_uri_is_valid (input_arg))
    fileuri = g_strdup (input_arg);
  else {
    gfile = g_file_new_for_commandline_arg (input_arg);
    if (g_file_has_uri_scheme (gfile, "archive") != FALSE) {
      g_print ("ERROR: %s isn't a file\n", input_arg);
    }

    fileuri = g_file_get_path (gfile);
    fileuri = g_strdup_printf ("file://%s", fileuri);
  }

  return fileuri;
}

gchar *
clean_brackets_in_uri (gchar * uri)
{
  gchar *clean_uri;
  gchar **split;

  split = g_strsplit (uri, "[", 0);
  clean_uri = g_strjoinv (NULL, split);
  g_strfreev (split);
  split = g_strsplit (clean_uri, "]", 0);
  g_free (clean_uri);
  clean_uri = g_strjoinv (NULL, split);
  g_strfreev (split);

  return clean_uri;
}
