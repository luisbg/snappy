/*
 * snappy - 0.1 beta
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
cut_long_filename (gchar *filename)
{
	gchar *ret;
	gint c;
	gint max_size = 34;

	for (c = 0; filename[c] != '\0'; c++);

	if (c >= max_size)
	{
		gchar short_filename[max_size];

		for (c = 0; c < max_size; c++)
		{
			short_filename[c] = filename[c];
		}
		short_filename[max_size] = '\0';
		ret = g_filename_to_utf8 (short_filename, max_size, NULL, NULL, NULL);
	} else {
		ret = g_locale_to_utf8 (filename, -1, NULL, NULL, NULL);
	}

	if (ret == NULL) g_print ("really?\n");
	return ret;
}

gchar *
clean_uri (gchar *input_arg)
{
	GFile *gfile;
	gchar *filepath;

	gfile = g_file_new_for_commandline_arg (input_arg);
	if (g_file_has_uri_scheme (gfile, "archive") != FALSE)
	{
		g_print ("ERROR: %s isn't a file\n", input_arg);
	}

	filepath = g_file_get_path (gfile);

	return filepath;
}
