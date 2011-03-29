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

#include <clutter-gst/clutter-gst.h>

#include "user_interface.h"
#include "gst_engine.h"

#define SAVE_POSITION_MIN_DURATION 300 * 1000   // don't save >5 minute files
#define SAVE_POSITION_THRESHOLD 0.05    // percentage threshold

/* -------------------- static functions --------------------- */

gboolean
add_uri_unfinished_playback (GstEngine * engine, gchar * uri, gint64 position)
{
  guint hash_key;
  gint64 duration;
  const gchar *config_dir;
  gchar *path, *data, *key;
  FILE *file;
  GKeyFile *keyfile;
  GKeyFileFlags flags;

  duration = engine->media_duration;
  if (duration < SAVE_POSITION_MIN_DURATION ||
      (duration - position) < (duration * SAVE_POSITION_THRESHOLD) ||
      (position < duration * SAVE_POSITION_THRESHOLD)) {
    // remove in case position is already stored and close
    remove_uri_unfinished_playback (engine, uri);
    return FALSE;
  }

  keyfile = g_key_file_new ();
  flags = G_KEY_FILE_KEEP_COMMENTS;
  hash_key = g_str_hash (uri);
  key = g_strdup_printf ("%d", hash_key);

  // config file path
  config_dir = g_get_user_config_dir ();
  path = g_strdup_printf ("%s/snappy/config", config_dir);

  g_key_file_load_from_file (keyfile, path, flags, NULL);
  // if file doesn't exist it uses the newly created one
  g_key_file_set_int64 (keyfile, "unfinished", key, position);

  // save gkeyfile to a file
  data = g_key_file_to_data (keyfile, NULL, NULL);
  file = fopen (path, "w");
  fputs (data, file);
  fclose (file);

  g_free (data);
  g_free (path);

  return TRUE;
}

gboolean
remove_uri_unfinished_playback (GstEngine * engine, gchar * uri)
{
  guint hash_key;
  const gchar *config_dir;
  gchar *path, *data, *key;
  FILE *file;
  GKeyFile *keyfile;
  GKeyFileFlags flags;

  keyfile = g_key_file_new ();
  flags = G_KEY_FILE_KEEP_COMMENTS;
  hash_key = g_str_hash (uri);
  key = g_strdup_printf ("%d", hash_key);

  // config file path
  config_dir = g_get_user_config_dir ();
  path = g_strdup_printf ("%s/snappy/config", config_dir);

  // remove key if gkeyfile exists
  if (g_key_file_load_from_file (keyfile, path, flags, NULL))
    g_key_file_remove_key (keyfile, "unfinished", key, NULL);

  // save gkeyfile to a file
  data = g_key_file_to_data (keyfile, NULL, NULL);
  file = fopen (path, "w");
  fputs (data, file);
  fclose (file);

  g_free (data);
  g_free (path);

  return TRUE;
}

gint64
uri_is_unfinished_playback (GstEngine * engine, gchar * uri)
{
  guint hash_key;
  gint64 position = -1;
  const gchar *config_dir;
  gchar *path, *key;
  GKeyFile *keyfile;
  GKeyFileFlags flags;

  keyfile = g_key_file_new ();
  flags = G_KEY_FILE_KEEP_COMMENTS;
  hash_key = g_str_hash (uri);
  key = g_strdup_printf ("%d", hash_key);

  // config file path
  config_dir = g_get_user_config_dir ();
  path = g_strdup_printf ("%s/snappy/config", config_dir);

  if (g_key_file_load_from_file (keyfile, path, flags, NULL))
    if (g_key_file_has_group (keyfile, "unfinished"))
      if (g_key_file_has_key (keyfile, "unfinished", key, NULL))
        position = g_key_file_get_int64 (keyfile, "unfinished", key, NULL);

  g_key_file_free (keyfile);
  g_free (path);

  return position;
}

/* -------------------- non-static functions --------------------- */

gboolean
add_uri_unfinished (GstEngine * engine)
{
  gint64 position;

  position = query_position (engine);
  add_uri_unfinished_playback (engine, engine->uri, position);

  return TRUE;
}

gboolean
at_the_eos (GstEngine * engine)
{
  gboolean ret = TRUE;
  gint64 position;

  position = query_position (engine);
  if (position < engine->media_duration)
    ret = FALSE;

  return ret;
}

gboolean
bus_call (GstBus * bus, GstMessage * msg, gpointer data)
{
  UserInterface *ui = (UserInterface *) data;
  GstEngine *engine = ui->engine;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      g_debug ("End-of-stream\n");
      remove_uri_unfinished_playback (engine, engine->uri);
      break;
    case GST_MESSAGE_ERROR:
    {
      gchar *debug = NULL;
      GError *err = NULL;

      gst_message_parse_error (msg, &err, &debug);

      g_debug ("Error: %s\n", err->message);
      g_error_free (err);

      if (debug) {
        g_debug ("Debug details: %s\n", debug);
        g_free (debug);
      }

      break;
    }
    case GST_MESSAGE_STATE_CHANGED:
    {
      GstState old, new, pending;
      gst_message_parse_state_changed (msg, &old, &new, &pending);
      if (new == GST_STATE_PAUSED) {
        if (engine->media_width == -1) {
          GstPad *p;
          GstCaps *c;

          p = gst_element_get_pad (engine->sink, "sink");
          c = gst_pad_get_negotiated_caps (p);
          if (c) {
            GstStructure *s;
            const GValue *widthval, *heightval;

            s = gst_caps_get_structure (c, 0);
            widthval = gst_structure_get_value (s, "width");
            heightval = gst_structure_get_value (s, "height");
            if (G_VALUE_HOLDS (widthval, G_TYPE_INT)) {
              gint width, height;
              gint64 position;

              width = g_value_get_int (widthval);
              height = g_value_get_int (heightval);
              engine->media_width = width;
              engine->media_height = height;
              update_media_duration (ui->engine);
              load_user_interface (ui);

              position = uri_is_unfinished_playback (engine, engine->uri);
              if (position != -1)
                seek (engine, position);
            }
          }
        }
      }

      break;
    }
    case GST_MESSAGE_STEP_DONE:
    {
      engine->prev_done = TRUE;
    }
    default:
      break;
  }

  return TRUE;
}

gboolean
engine_init (GstEngine * engine, GstElement * sink)
{
  engine->media_width = -1;
  engine->media_height = -1;
  engine->direction_foward = TRUE;
  engine->prev_done = TRUE;
  engine->second = GST_SECOND;

  engine->player = gst_element_factory_make ("playbin2", "playbin2");
  if (engine->player == NULL) {
    g_print ("ERROR: Failed to create playbin element\n");
    return FALSE;
  }

  engine->sink = sink;
  g_object_set (G_OBJECT (engine->player), "video-sink", engine->sink, NULL);
  engine->bus = gst_pipeline_get_bus (GST_PIPELINE (engine->player));

  return TRUE;
}

gboolean
engine_load_uri (GstEngine * engine, gchar * uri)
{
  gint64 position;

  engine->uri = uri;
  g_object_set (G_OBJECT (engine->player), "uri", uri, NULL);
  g_print ("Loading: %s\n", uri);

  return TRUE;
}

gboolean
frame_stepping (GstEngine * engine, gboolean foward)
{
  gboolean ok;
  gint64 pos;
  gdouble rate;
  GstFormat fmt;

  if (engine->prev_done) {
    engine->prev_done = FALSE;

    if (foward != engine->direction_foward) {
      engine->direction_foward = foward;

      fmt = GST_FORMAT_TIME;
      ok = gst_element_query_position (engine->player, &fmt, &pos);
      gst_element_get_state (engine->player, NULL, NULL, GST_SECOND);

      if (foward)
        rate = 1.0;
      else
        rate = -1.0;

      if (rate >= 0.0) {
        ok = gst_element_seek (engine->player, rate, fmt,
            GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
            GST_SEEK_TYPE_SET, pos, GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE);
      } else {
        ok = gst_element_seek (engine->player, rate, fmt,
            GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
            GST_SEEK_TYPE_SET, G_GINT64_CONSTANT (0), GST_SEEK_TYPE_SET, pos);
      }
      gst_element_get_state (engine->player, NULL, NULL, GST_SECOND);
    }

    fmt = GST_FORMAT_BUFFERS;
    gst_element_send_event (engine->player,
        gst_event_new_step (fmt, 1, 1.0, TRUE, FALSE));
  }

  return FALSE;
}

GstState
get_state (GstEngine * engine)
{
  GstState state;
  gst_element_get_state (engine->player, &state, NULL, GST_SECOND);

  return state;
}

gint64
query_position (GstEngine * engine)
{
  gint64 position;
  GstFormat fmt = GST_FORMAT_TIME;

  gst_element_query_position (engine->player, &fmt, &position);
  return position;
}

gboolean
seek (GstEngine * engine, gint64 position)
{
  GstFormat fmt = GST_FORMAT_TIME;

  gst_element_seek_simple (engine->player, fmt, GST_SEEK_FLAG_FLUSH, position);

  return TRUE;
}

gboolean
change_state (GstEngine * engine, gchar * state)
{
  if (state == "Playing") {
    gst_element_set_state (engine->player, GST_STATE_PLAYING);
    engine->playing = TRUE;
  } else if (state == "Paused") {
    gst_element_set_state (engine->player, GST_STATE_PAUSED);
    engine->playing = FALSE;
    engine->media_duration = -1;
  } else if (state == "Ready") {
    gst_element_set_state (engine->player, GST_STATE_READY);
    engine->playing = FALSE;
    engine->media_duration = -1;
  } else if (state == "Null") {
    gst_element_set_state (engine->player, GST_STATE_NULL);
    engine->playing = FALSE;
    engine->media_duration = -1;
  }

  return TRUE;
}

gboolean
update_media_duration (GstEngine * engine)
{
  gboolean success = FALSE;
  GstFormat fmt = GST_FORMAT_TIME;

  if (gst_element_query_duration (engine->player, &fmt,
          &engine->media_duration)) {
    if (engine->media_duration != -1 && fmt == GST_FORMAT_TIME) {
      success = TRUE;
    } else {
      g_debug ("Could not get media's duration\n");
      success = FALSE;
    }
  }

  return success;
}
