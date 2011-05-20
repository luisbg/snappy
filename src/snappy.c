/*
 * snappy - 0.2 beta
 *
 * Copyright (C) 2011 Collabora Multimedia Ltd.
 * <luis.debethencourt@collabora.com>
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

#define VERSION "0.2 beta"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <clutter/clutter.h>
#include <clutter-gst/clutter-gst.h>

#include "user_interface.h"
#include "dlna.h"
#include "gst_engine.h"
#include "utils.h"

void
close_down (UserInterface * ui, GstEngine * engine)
{
  g_print ("closing snappy\n");

  // save position if file isn't finished playing
  add_uri_unfinished (engine);

  change_state (engine, "Null");

  screensaver_enable (ui->screensaver, TRUE);
  screensaver_free (ui->screensaver);

  gst_object_unref (G_OBJECT (engine->player));
}

void
open_uri_callback (SnappyMP * self, gpointer user_data)
{
  g_print ("received the open-uri signal!\n");
}

gboolean
config_load ()
{
  const gchar *config_dir;
  gchar *path;

  config_dir = g_get_user_config_dir ();
  path = g_strdup_printf ("%s/snappy/", config_dir);

  if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
    g_print ("%s doesn't exist\n", path);
    g_mkdir_with_parents (path, 0700);
  }

  return TRUE;
}

gboolean
process_args (int argc, char *argv[],
    gchar * file_list[], gboolean * fullscreen, GOptionContext * context)
{
  gboolean recent = FALSE, version = FALSE;
  guint c, index, pos = 0;
  GOptionEntry entries[] = {
    {"fullscreen", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, fullscreen,
        "Fullscreen mode", NULL},
    {"recent", 'r', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &recent,
        "Recently played", NULL},
    {"version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
        "Print version", NULL},
    {NULL}
  };
  GError *err = NULL;

  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, gst_init_get_option_group ());
  g_option_context_add_group (context, clutter_get_option_group ());

  // Command line arguments.
  if (!g_option_context_parse (context, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", err->message);
    g_error_free (err);
    goto quit;
  }

  if (recent) {
    gchar **recent = NULL;

    g_print ("These are the recently played URIs: \n\n");

    recent = get_recently_played ();

    for (c = 0; recent[c] != NULL; c++)
      g_print ("%d: %s \n", c + 1, recent[c]);

    goto quit;
  }

  if (version) {
    g_print ("snappy version %s\n", VERSION);
    goto quit;
  }
  // File arguments
  if (argc < 2) {
    g_print ("%s", g_option_context_get_help (context, TRUE, NULL));
    goto quit;
  }

  for (index = 1; index < argc; index++) {
    file_list[pos] = argv[index];
    g_debug ("Adding file: %s\n", file_list[pos]);
    pos++;
  }

  return TRUE;

quit:
  return FALSE;
}

int
main (int argc, char *argv[])
{
  UserInterface *ui = NULL;
  GstEngine *engine = NULL;
  SnappyMP *mp_obj = NULL;
  ClutterActor *video_texture;
  GstElement *sink;

  gboolean ok, fullscreen = FALSE;
  gint ret = 0;
  guint c, index, pos = 0;
  gchar *fileuri, *uri;
  gchar *file_list[argc];
  GOptionContext *context;

  if (!g_thread_supported ())
    g_thread_init (NULL);

  context = g_option_context_new ("<media file> - Play movie files");

  ok = process_args (argc, argv, file_list, &fullscreen, context);
  if (!ok)
    goto quit;
  config_load ();

  // User Interface
  ui = g_new (UserInterface, 1);
  ui->fullscreen = fullscreen;
  video_texture = clutter_texture_new ();

  clutter_gst_init (&argc, &argv);

  // Gstreamer
  engine = g_new (GstEngine, 1);
  engine->media_width = -1;
  engine->media_height = -1;
  ui->engine = engine;
  sink = clutter_gst_video_sink_new (CLUTTER_TEXTURE (video_texture));

  ok = engine_init (engine, sink);
  if (!ok)
    goto quit;
  ui->texture = video_texture;
  gst_bus_add_watch (engine->bus, bus_call, ui);
  gst_object_unref (engine->bus);

  if (gst_uri_is_valid (file_list[0]))
    uri = g_strdup (file_list[0]);
  else {
    fileuri = clean_uri (file_list[0]);
    uri = g_strdup_printf ("file://%s", fileuri);
  }

  engine_load_uri (engine, uri);
  interface_load_uri (ui, uri);
  load_user_interface (ui);

  change_state (engine, "Paused");
  change_state (engine, "Playing");

  mp_obj = g_new (SnappyMP, 1);
  mp_obj->engine = engine;
  mp_obj->ui = ui;
  load_dlna (mp_obj);

  clutter_main ();

  close_down (ui, engine);
  close_dlna (mp_obj);

quit:
  g_option_context_free (context);

  return ret;
}
