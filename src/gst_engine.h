/*
 * snappy - 0.1
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

#ifndef __GST_ENGINE_H__
#define __GST_ENGINE_H__

G_BEGIN_DECLS typedef struct _GstEngine GstEngine;

struct _GstEngine
{
  gboolean playing, direction_foward, prev_done;

  guint media_width, media_height;
  gint64 media_duration;
  gint64 second;

  gchar *uri;

  GstElement *player;
  GstElement *sink;

  GstBus *bus;
};

// Declaration of non-static functions
gboolean add_uri_unfinished (GstEngine * engine);
gboolean at_the_eos (GstEngine * engine);
gboolean bus_call (GstBus * bus, GstMessage * msg, gpointer data);
gboolean engine_load (GstEngine * engine, GstElement * sink);
gboolean engine_load_uri (GstEngine * engine, gchar * uri);
gboolean frame_stepping (GstEngine * engine, gboolean foward);
GstState get_state (GstEngine * engine);
gint64 query_position (GstEngine * engine);
gboolean seek (GstEngine * engine, gint64 position);
gboolean change_state (GstEngine * engine, gchar * state);
gboolean update_media_duration (GstEngine * engine);

G_END_DECLS
#endif /* __GST_ENGINE_H__ */
