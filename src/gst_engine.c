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

#include <clutter-gst/clutter-gst.h>
#include <gst/pbutils/pbutils.h>
#include <string.h>

#include "user_interface.h"
#include "gst_engine.h"
#include "utils.h"

#define SAVE_POSITION_MIN_DURATION 300 * 1000   // don't save >5 minute files
#define SAVE_POSITION_THRESHOLD 0.05    // percentage threshold

#define RECENTLY_VIEWED_MAX 50

// GstPlayFlags flags from playbin2. It is the policy of GStreamer to
// not publicly expose element-specific enums. That's why this
// GstPlayFlags enum has been copied here.
typedef enum
{
  GST_PLAY_FLAG_VIDEO = 0x00000001,
  GST_PLAY_FLAG_AUDIO = 0x00000002,
  GST_PLAY_FLAG_TEXT = 0x00000004,
  GST_PLAY_FLAG_VIS = 0x00000008,
  GST_PLAY_FLAG_SOFT_VOLUME = 0x00000010,
  GST_PLAY_FLAG_NATIVE_AUDIO = 0x00000020,
  GST_PLAY_FLAG_NATIVE_VIDEO = 0x00000040,
  GST_PLAY_FLAG_DOWNLOAD = 0x00000080,
  GST_PLAY_FLAG_BUFFERING = 0x000000100
} GstPlayFlags;

// Declaration of static functions
gboolean add_uri_to_history (gchar * uri);
gboolean add_uri_unfinished_playback (GstEngine * engine, gchar * uri,
    gint64 position);
gboolean discover (GstEngine * engine, gchar * uri);
static void handle_element_message (GstEngine * engine, GstMessage * msg);
gboolean is_stream_seakable (GstEngine * engine);
gint64 is_uri_unfinished_playback (GstEngine * engine, gchar * uri);
static void print_tag (const GstTagList * list, const gchar * tag,
    gpointer unused);
void remove_uri_unfinished_playback (GstEngine * engine, gchar * uri);
void stream_done (GstEngine * engine, UserInterface * ui);
static void write_key_file_to_file (GKeyFile * keyfile, const char *path);

/* -------------------- static functions --------------------- */

/*         Add URI to recently viewed list       */
gboolean
add_uri_to_history (gchar * uri)
{
  gboolean ret;
  const gchar *config_dir;
  gchar *path, *clean_uri;
  gchar **history_keys;
  gsize length;
  GKeyFile *keyfile;
  GKeyFileFlags flags;

  keyfile = g_key_file_new ();
  flags = G_KEY_FILE_KEEP_COMMENTS;

  /* Config file path */
  config_dir = g_get_user_config_dir ();
  path = g_strdup_printf ("%s/snappy/history", config_dir);

  /* Keynames can't include brackets */
  clean_uri = clean_brackets_in_uri (uri);

  if (g_key_file_load_from_file (keyfile, path, flags, NULL)) {
    /* Set uri in history */
    if (g_key_file_has_group (keyfile, "history")) {
      if (!g_key_file_has_key (keyfile, "history", clean_uri, NULL)) {
        /* Uri is not already in history */
        history_keys = g_key_file_get_keys (keyfile, "history", &length, NULL);

        if (length >= RECENTLY_VIEWED_MAX) {
          /* Remove first uri of the list */
          g_key_file_remove_key (keyfile, "history", history_keys[0], NULL);
        }
      }
    } else {
      /* If group "history" doesn't exist create it and populate it */
      g_key_file_set_boolean (keyfile, "history", clean_uri, TRUE);
    }

    /* g_get_real_time () is not available until glib 2.28.0 */
#if GLIB_CHECK_VERSION (2, 28, 0)
    g_key_file_set_int64 (keyfile, "history", clean_uri, g_get_real_time ());
#else
    {
      GTimeVal time;

      g_get_current_time (&time);
      g_key_file_set_int64 (keyfile, "history", clean_uri,
          (gint64) time.tv_sec);
    }
#endif

    /* Save gkeyfile to a file  */
    write_key_file_to_file (keyfile, path);
    g_free (path);

    ret = TRUE;
  } else {
    ret = FALSE;
  }

  return ret;
}


/* Add URI's last playback position to the unfinished list */
gboolean
add_uri_unfinished_playback (GstEngine * engine, gchar * uri, gint64 position)
{
  guint hash_key;
  gint64 duration;
  const gchar *config_dir;
  gchar *path, *key;
  GKeyFile *keyfile;
  GKeyFileFlags flags;

  duration = engine->media_duration;
  if (duration < SAVE_POSITION_MIN_DURATION ||
      (duration - position) < (duration * SAVE_POSITION_THRESHOLD) ||
      (position < duration * SAVE_POSITION_THRESHOLD)) {
    /* Remove in case position is already stored and close */
    remove_uri_unfinished_playback (engine, uri);
    return FALSE;
  }

  /* New keyfile with uri as hash key */
  keyfile = g_key_file_new ();
  flags = G_KEY_FILE_KEEP_COMMENTS;
  hash_key = g_str_hash (uri);
  key = g_strdup_printf ("%d", hash_key);

  /* Config file path */
  config_dir = g_get_user_config_dir ();
  path = g_strdup_printf ("%s/snappy/history", config_dir);

  /* Store uri and position in key file */
  g_key_file_load_from_file (keyfile, path, flags, NULL);
  g_key_file_set_int64 (keyfile, "unfinished", key, position);

  /* Save gkeyfile to a file, if file doesn't exist it creates a new one */
  write_key_file_to_file (keyfile, path);
  g_free (path);

  return TRUE;
}


/* Discover URI's properties: duration and dimensions */
gboolean
discover (GstEngine * engine, gchar * uri)
{
  gint timeout = 10;
  GstDiscoverer *dc;
  GstDiscovererInfo *info;
  GstDiscovererStreamInfo *s_info;
  GstDiscovererVideoInfo *v_info;
  GError *error = NULL;
  GList *list;
  GstPlayFlags flags;

  /* new GST Discoverer */
  dc = gst_discoverer_new (timeout * GST_SECOND, &error);
  if (G_UNLIKELY (error)) {
    GST_WARNING ("Error in GST Discoverer initializing: %s\n", error->message);
    g_error_free (error);
    return FALSE;
  }

  /* Discover URI */
  info = gst_discoverer_discover_uri (dc, uri, &error);
  if (G_UNLIKELY (error)) {
    GST_WARNING ("Error discovering URI: %s\n", error->message);
    g_error_free (error);
    return FALSE;
  }
  /* Check if it has a video stream */
  list = gst_discoverer_info_get_video_streams (info);
  engine->has_video = (g_list_length (list) > 0);
  gst_discoverer_stream_info_list_free (list);

  /* Check if it has an audio stream */
  list = gst_discoverer_info_get_audio_streams (info);
  engine->has_audio = (g_list_length (list) > 0);
  gst_discoverer_stream_info_list_free (list);

  /* If it has any stream, get duration */
  if (engine->has_video || engine->has_audio)
    engine->media_duration = gst_discoverer_info_get_duration (info);

  GST_DEBUG ("Found video %d, audio %d", engine->has_video, engine->has_audio);

  /* If it has video stream, get dimensions */
  if (engine->has_video) {
    list = gst_discoverer_info_get_video_streams (info);
    v_info = (GstDiscovererVideoInfo *) list->data;
    engine->media_width = gst_discoverer_video_info_get_width (v_info);
    GST_DEBUG ("video width: %d", engine->media_width);
    engine->media_height = gst_discoverer_video_info_get_height (v_info);
    GST_DEBUG ("video height: %d", engine->media_height);

  } else {
    /* If only audio stream, play visualizations */
    g_object_get (G_OBJECT (engine->player), "flags", &flags, NULL);
    g_object_set (G_OBJECT (engine->player), "flags",
        flags | GST_PLAY_FLAG_VIS, NULL);
  }

  gst_discoverer_info_unref (info);

  return TRUE;
}

/* Handle GST_ELEMENT_MESSAGEs */
static void
handle_element_message (GstEngine * engine, GstMessage * msg)
{
  GstNavigationMessageType nav_msg_type = gst_navigation_message_get_type (msg);

  switch (nav_msg_type) {
    case GST_NAVIGATION_MESSAGE_COMMANDS_CHANGED:{
      GST_DEBUG ("Navigation message commands changed");
      if (is_stream_seakable (engine)) {
        update_media_duration (engine);
      }

      break;
    }

    default:{
      break;
    }
  }
}

/* Query if the current stream is seakable */
gboolean
is_stream_seakable (GstEngine * engine)
{
  GstQuery *query;
  gboolean res;

  query = gst_query_new_seeking (GST_FORMAT_TIME);
  if (gst_element_query (engine->player, query)) {
    gst_query_parse_seeking (query, NULL, &res, NULL, NULL);
    GST_DEBUG ("seeking query says the stream is %s seekable",
        (res) ? "" : " not");
  } else {
    GST_DEBUG ("seeking query failed");
  }

  gst_query_unref (query);
  return res;
}

/* Check if URI is in the unfinished playback list */
gint64
is_uri_unfinished_playback (GstEngine * engine, gchar * uri)
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

  /* Config file path */
  config_dir = g_get_user_config_dir ();
  path = g_strdup_printf ("%s/snappy/history", config_dir);

  /* If URI hash key is in the list, load position */
  if (g_key_file_load_from_file (keyfile, path, flags, NULL))
    if (g_key_file_has_group (keyfile, "unfinished"))
      if (g_key_file_has_key (keyfile, "unfinished", key, NULL))
        position = g_key_file_get_int64 (keyfile, "unfinished", key, NULL);

  g_key_file_free (keyfile);
  g_free (path);

  return position;
}


/*  Print message tags from elements  */
static void
print_tag (const GstTagList * list, const gchar * tag, gpointer unused)
{
  gint i, count;

  count = gst_tag_list_get_tag_size (list, tag);

  for (i = 0; i < count; i++) {
    gchar *str;

    if (gst_tag_get_type (tag) == G_TYPE_STRING) {
      if (!gst_tag_list_get_string_index (list, tag, i, &str))
        g_assert_not_reached ();
    } else {
      str =
          g_strdup_value_contents (gst_tag_list_get_value_index (list, tag, i));
    }

    if (i == 0) {
      g_print ("  %15s: %s\n", gst_tag_get_nick (tag), str);
    } else {
      g_print ("                 : %s\n", str);
    }

    g_free (str);
  }
}


/*    Remove URI from unfinished playback list   */
void
remove_uri_unfinished_playback (GstEngine * engine, gchar * uri)
{
  guint hash_key;
  const gchar *config_dir;
  gchar *path, *data, *key;
  GError *error = NULL;
  GKeyFile *keyfile;
  GKeyFileFlags flags;

  keyfile = g_key_file_new ();
  flags = G_KEY_FILE_KEEP_COMMENTS;
  hash_key = g_str_hash (uri);
  key = g_strdup_printf ("%d", hash_key);

  /* Config file path */
  config_dir = g_get_user_config_dir ();

  /* Self config directory, i.e. .config/snappy/ */
  struct stat st_self;
  if ( 0 != stat(g_strdup_printf("%s/snappy", config_dir), &st_self)) {
     if ( 0 != mkdir( g_strdup_printf("%s/snappy", config_dir), 0777))
       perror("Failed to create ~/.config/snappy/ directory");
  }
  else if (!S_ISDIR(st_self.st_mode)) {
     errno = ENOTDIR;
     perror("~/config/snappy/ already exists");
  }

  /* History file */
  path = g_strdup_printf ("%s/snappy/history", config_dir);

  /* Remove key from history file */
  if (g_key_file_load_from_file (keyfile, path, flags, NULL))
    g_key_file_remove_key (keyfile, "unfinished", key, NULL);

  /* Save gkeyfile to a file */
  data = g_key_file_to_data (keyfile, NULL, NULL);
  g_file_set_contents (path, data, strlen (data), &error);
  if (error != NULL) {
    GST_WARNING ("Failed to write history file to %s: %s", path, error->message);
    g_error_free (error);
  }

  g_free (data);
  g_free (path);

  return;
}

/*    When Stream or segment is done play next or loop     */
void
stream_done (GstEngine * engine, UserInterface * ui)
{
  /* When URI is done or looping remove from unfinished list */
  remove_uri_unfinished_playback (engine, engine->uri);

  if (engine->loop && (interface_is_it_last (ui))) {
    engine_seek (engine, 0, TRUE);
  } else {
    interface_play_next_or_prev (ui, TRUE);
  }
}

static void
write_key_file_to_file (GKeyFile * keyfile, const char *path)
{
  gchar *data, *dir;
  GError *error = NULL;

  dir = g_path_get_dirname (path);
  g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
  g_free (dir);

  data = g_key_file_to_data (keyfile, NULL, NULL);
  g_file_set_contents (path, data, strlen (data), &error);
  if (error != NULL) {
    GST_WARNING ("Failed to write history file to %s: %s", path, error->message);
    g_error_free (error);
  }

  g_free (data);
}

/* -------------------- non-static functions --------------------- */


/*           Add URI to uninished list           */
gboolean
add_uri_unfinished (GstEngine * engine)
{
  gboolean ret;
  gint64 position;

  position = query_position (engine);
  ret = add_uri_unfinished_playback (engine, engine->uri, position);

  return ret;
}


/*     Check if playback is at End of Stream     */
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


/*          Gstreamer pipeline bus call          */
gboolean
bus_call (GstBus * bus, GstMessage * msg, gpointer data)
{
  UserInterface *ui = (UserInterface *) data;
  GstEngine *engine = ui->engine;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_STATE_CHANGED:
    {
      GstState old, new, pending;

      GST_DEBUG ("State changed");
      gst_message_parse_state_changed (msg, &old, &new, &pending);
      if (new == GST_STATE_PLAYING) {
        /* If loading file */
        if (!engine->has_started) {
          gint64 position;

          /* Check if URI was left unfinished, if so seek to last position */
          position = is_uri_unfinished_playback (engine, engine->uri);
          if (position != -1) {
            engine_seek (engine, position, TRUE);
          }

          if (!engine->secret)
            add_uri_to_history (engine->uri);
          else
            g_print ("Secret mode. Not saving uri in history.\n");

          interface_update_controls (ui);
          engine->has_started = TRUE;
        }
      }

      break;
    }

    case GST_MESSAGE_TAG:
    {
      GstTagList *tags;

      GST_DEBUG ("Tag received");
      if (ui->tags) {
        gst_message_parse_tag (msg, &tags);
        if (tags) {
          g_print ("%s\n",
              GST_STR_NULL (GST_ELEMENT_NAME (GST_MESSAGE_SRC (msg))));

          gst_tag_list_foreach (tags, print_tag, NULL);
          gst_tag_list_free (tags);
          tags = NULL;
        }
      }
      break;
    }

    case GST_MESSAGE_EOS:
    {
      GST_DEBUG ("End of stream");
      stream_done (engine, ui);

      break;
    }

    case GST_MESSAGE_SEGMENT_DONE:
    {
      GST_DEBUG ("Segment done");
      stream_done (engine, ui);

      break;
    }

    case GST_MESSAGE_STEP_DONE:
    {
      GST_DEBUG ("Step done");
      engine->prev_done = TRUE;
      break;
    }

    case GST_MESSAGE_ASYNC_DONE:
      GST_DEBUG ("Async done");
      engine->queries_blocked = FALSE;
      break;

    case GST_MESSAGE_DURATION:
    {
      GST_DEBUG ("Message duration received");
      update_media_duration (engine);

      break;
    }

    case GST_MESSAGE_ELEMENT:
    {
      handle_element_message (engine, msg);
      break;
    }

    case GST_MESSAGE_ERROR:
    {
      /* Parse and share Gst Error */
      gchar *debug = NULL;
      GError *err = NULL;

      gst_message_parse_error (msg, &err, &debug);

      GST_DEBUG ("Error: %s", err->message);
      g_error_free (err);

      if (debug) {
        GST_DEBUG ("Debug details: %s", debug);
        g_free (debug);
      }

      break;
    }

    default:
      break;
  }

  return TRUE;
}


/*             Change pipeline state             */
gboolean
change_state (GstEngine * engine, gchar * state)
{
  gboolean ok = TRUE;
  GstStateChangeReturn change;

  if (!g_strcmp0 (state, "Playing")) {
    change = gst_element_set_state (engine->player, GST_STATE_PLAYING);
    engine->playing = TRUE;
    engine->queries_blocked = FALSE;
  } else if (!g_strcmp0 (state, "Paused")) {
    change = gst_element_set_state (engine->player, GST_STATE_PAUSED);
    engine->playing = FALSE;
    engine->queries_blocked = FALSE;
  } else if (!g_strcmp0 (state, "Ready")) {
    change = gst_element_set_state (engine->player, GST_STATE_READY);
    engine->playing = FALSE;
    engine->media_duration = -1;
    engine->queries_blocked = TRUE;
  } else if (!g_strcmp0 (state, "Null")) {
    change = gst_element_set_state (engine->player, GST_STATE_NULL);
    engine->playing = FALSE;
    engine->media_duration = -1;
    engine->queries_blocked = TRUE;
  }

  if (change == GST_STATE_CHANGE_FAILURE)
    ok = FALSE;

  return ok;
}


/*               Cycle through streams           */
void
cycle_streams (GstEngine * engine, guint streamid)
{
  gboolean last_stream = FALSE;
  gint current;
  gint streams;
  gchar *n;
  gchar *c;

  switch (streamid) {
    case STREAM_AUDIO:
      n = "n-audio";
      c = "current-audio";
      break;
    case STREAM_TEXT:
      n = "n-text";
      c = "current-text";
      break;
    case STREAM_VIDEO:
      n = "n-video";
      c = "current-video";
      break;
  }

  g_object_get (G_OBJECT (engine->player), n, &streams, NULL);
  g_object_get (G_OBJECT (engine->player), c, &current, NULL);

  if (current < (streams - 1)) {
    current++;
  } else {
    current = 0;
    last_stream = TRUE;
  }

  g_object_set (G_OBJECT (engine->player), c, current, NULL);

  return;
}


/*            Init GstEngine variables           */
gboolean
engine_init (GstEngine * engine, GstElement * sink)
{
  engine->playing = FALSE;
  engine->direction_foward = TRUE;
  engine->prev_done = TRUE;

  engine->has_started = FALSE;
  engine->has_video = FALSE;
  engine->has_audio = FALSE;
  engine->loop = FALSE;
  engine->secret = FALSE;
  engine->queries_blocked = TRUE;

  engine->media_width = 600;
  engine->media_height = 400;
  engine->media_duration = -1;
  engine->second = GST_SECOND;

  engine->uri = NULL;

  gchar *version_str;

  GST_DEBUG_CATEGORY_INIT (_snappy_gst_debug, "snappy", 0,
      "snappy media player");

  /* Make playbin2 element */
  engine->player = gst_element_factory_make ("playbin2", "playbin2");
  if (engine->player == NULL) {
    g_print ("ERROR: Failed to create playbin element\n");
    return FALSE;
  }

  /* Set Clutter texture as playbin2's videos sink */
  engine->sink = sink;
  g_object_set (G_OBJECT (engine->player), "video-sink", engine->sink, NULL);
  engine->bus = gst_pipeline_get_bus (GST_PIPELINE (engine->player));

  engine->navigation = GST_NAVIGATION (gst_bin_get_by_interface (GST_BIN (engine->player),
                                                                 GST_TYPE_NAVIGATION));

  return TRUE;
}


/*               Load URI to engine              */
void
engine_load_uri (GstEngine * engine, gchar * uri)
{
  engine->uri = uri;

  /* Loading a new URI means we haven't started playing this URI yet */
  engine->has_started = FALSE;
  engine->queries_blocked = TRUE;

  discover (engine, uri);

  g_print ("Loading: %s\n", uri);
  g_object_set (G_OBJECT (engine->player), "uri", uri, NULL);

  return;
}


/*               Open Uri in engine              */
void
engine_open_uri (GstEngine * engine, gchar * uri)
{
  /* Need to set back to Ready state so Playbin2 loads uri */
  engine->uri = uri;

  g_print ("Open uri: %s\n", uri);
  gst_element_set_state (engine->player, GST_STATE_READY);
  g_object_set (G_OBJECT (engine->player), "uri", uri, NULL);

  discover (engine, uri);

  return;
}


/*                  Set to Playing               */
gboolean
engine_play (GstEngine * engine)
{
  gboolean ok = TRUE;
  GstStateChangeReturn change;

  change = gst_element_set_state (engine->player, GST_STATE_PLAYING);
  engine->has_started = TRUE;
  engine->playing = TRUE;
  engine->queries_blocked = FALSE;

  if (change == GST_STATE_CHANGE_FAILURE)
    ok = FALSE;

  return ok;
}


/*            Seek engine to position            */
gboolean
engine_seek (GstEngine * engine, gint64 position, gboolean accurate)
{
  gboolean ok;
  GstFormat fmt = GST_FORMAT_TIME;
  GstSeekFlags flags;

  if (accurate) {
    flags =
        GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_ACCURATE;
  } else {
    flags =
        GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_KEY_UNIT;
  }

  ok = gst_element_seek_simple (engine->player, fmt,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_KEY_UNIT,
      position);

  engine->queries_blocked = TRUE;

  return ok;
}


/*                 Stop playback                 */
gboolean
engine_stop (GstEngine * engine)
{
  gboolean ok = TRUE;
  GstStateChangeReturn change;

  change = gst_element_set_state (engine->player, GST_STATE_READY);
  engine->playing = FALSE;
  engine->queries_blocked = TRUE;

  if (change == GST_STATE_CHANGE_FAILURE)
    ok = FALSE;

  return ok;
}


/*                   Set volume                  */
void
engine_volume (GstEngine * engine, gdouble level)
{
  g_object_set (G_OBJECT (engine->player), "volume", level, NULL);

  return;
}


/*       Move one step foward or backwards       */
gboolean
frame_stepping (GstEngine * engine, gboolean foward)
{
  gboolean ok;
  gint64 pos;
  gdouble rate;
  GstFormat fmt;

  /* Continue if previous frame step is done */
  if (engine->prev_done) {
    engine->prev_done = FALSE;

    if (foward != engine->direction_foward) {
      /* Change of direction needed */
      engine->direction_foward = foward;

      fmt = GST_FORMAT_TIME;
      ok = gst_element_query_position (engine->player, &fmt, &pos);
      gst_element_get_state (engine->player, NULL, NULL, GST_SECOND);

      if (foward)
        rate = 1.0;
      else
        rate = -1.0;

      /* Seek with current position and reverse rate */
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

    /* Send new step event */
    fmt = GST_FORMAT_BUFFERS;
    gst_element_send_event (engine->player,
        gst_event_new_step (fmt, 1, 1.0, TRUE, FALSE));

    engine->queries_blocked = TRUE;
  }

  return ok;
}


/*            Get recently viewed URIs           */
gchar **
get_recently_viewed ()
{
  const gchar *config_dir;
  gchar *path;
  gchar **recent = NULL;
  gsize length;
  GKeyFile *keyfile;
  GKeyFileFlags flags;

  keyfile = g_key_file_new ();
  flags = G_KEY_FILE_KEEP_COMMENTS;

  /* Config file path */
  config_dir = g_get_user_config_dir ();
  path = g_strdup_printf ("%s/snappy/history", config_dir);

  /* Get keys */
  if (g_key_file_load_from_file (keyfile, path, flags, NULL))
    if (g_key_file_has_group (keyfile, "history"))
      recent = g_key_file_get_keys (keyfile, "history", &length, NULL);

  return recent;
}


/*               Get pipeline state              */
GstState
get_state (GstEngine * engine)
{
  GstState state;
  gst_element_get_state (engine->player, &state, NULL, GST_SECOND);

  return state;
}


/*             Query playback position           */
gint64
query_position (GstEngine * engine)
{
  gboolean ok;
  gint64 position;
  GstFormat fmt = GST_FORMAT_TIME;

  ok = gst_element_query_position (engine->player, &fmt, &position);

  if (!ok)
    position = 0;

  return position;
}


/*                 Set subtitle file             */
void
set_subtitle_uri (GstEngine * engine, gchar * suburi)
{
  g_print ("Loading subtitles: %s\n");
  g_object_set (G_OBJECT (engine->player), "suburi", suburi, NULL);

  return;
}


/*               Toggle subtitles                */
void
toggle_subtitles (GstEngine * engine)
{
  gint flags;
  gboolean sub_state;

  g_object_get (G_OBJECT (engine->player), "flags", &flags, NULL);
  sub_state = flags & (1 << 2);

  if (sub_state) {              // If subtitles on, cycle streams and if last turn off
    cycle_streams (engine, STREAM_TEXT);
    flags &= ~(1 << 2);
    g_object_set (G_OBJECT (engine->player), "flags", flags, NULL);

  } else {                      // If subtitles off, turn them on
    flags |= (1 << 2);
    g_object_set (G_OBJECT (engine->player), "flags", flags, NULL);
  }

  return;
}


/*          Update duration of URI streams       */
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
      GST_DEBUG ("Could not get media's duration");
      success = FALSE;
    }
  }

  return success;
}
