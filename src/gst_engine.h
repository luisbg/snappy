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

#ifndef __GST_ENGINE_H__
#define __GST_ENGINE_H__

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstEngine GstEngine;

struct _GstEngine
{
  gboolean playing, direction_foward, prev_done;
  gboolean has_started;
  gboolean has_video, has_audio;
  gboolean loop;
  gboolean secret;
  gboolean queries_blocked;

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
gboolean change_state (GstEngine * engine, gchar * state);
gboolean engine_init (GstEngine * engine, GstElement * sink);
gboolean engine_load_uri (GstEngine * engine, gchar * uri);
gboolean engine_open_uri (GstEngine * engine, gchar * uri);
gboolean engine_play (GstEngine * engine);
gboolean engine_seek (GstEngine * engine, gint64 position);
gboolean engine_stop (GstEngine * engine);
gboolean engine_volume (GstEngine * engine, gdouble level);
gboolean frame_stepping (GstEngine * engine, gboolean foward);
gchar **get_recently_viewed ();
GstState get_state (GstEngine * engine);
gint64 query_position (GstEngine * engine);
gboolean set_subtitle_uri (GstEngine * engine, gchar *suburi);
gboolean toggle_streams (GstEngine * engine, gboolean video_stream);
gboolean toggle_subtitles (GstEngine * engine);
gboolean update_media_duration (GstEngine * engine);

G_END_DECLS
#endif /* __GST_ENGINE_H__ */
