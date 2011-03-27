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

#define VERSION "0.1"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <clutter/clutter.h>
#include <clutter-gst/clutter-gst.h>

#include "user_interface.h"
#include "gst_engine.h"
#include "utils.h"

int
main (int argc, char *argv[])
{
  UserInterface *ui = NULL;
  GstEngine *engine = NULL;
  ClutterActor *texture;
  gchar *fileuri;
  int ret = 0;
  gboolean fullscreen = FALSE, version = FALSE;
  guint c, index, pos = 0;
  gchar *file_list[argc];
  GOptionEntry entries[] = {
    {"fullscreen", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &fullscreen,
        "Fullscreen mode", NULL},
    {"version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
        "Print version", NULL},
    {NULL}
  };
  GOptionContext *context;
  GError *err = NULL;

  if (!g_thread_supported ())
    g_thread_init (NULL);

  context = g_option_context_new ("<media file> - Play movie files");
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, gst_init_get_option_group ());
  g_option_context_add_group (context, clutter_get_option_group ());

  // Command line arguments.
  if (!g_option_context_parse (context, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", err->message);
    g_error_free (err);
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

  // User Interface
  ui = g_new0 (UserInterface, 1);
  ui->fullscreen = fullscreen;

  clutter_gst_init (&argc, &argv);

  // Gstreamer
  engine = g_new0 (GstEngine, 1);
  engine->media_width = -1;
  engine->media_height = -1;
  engine->direction_foward = TRUE;
  engine->prev_done = TRUE;
  engine->second = GST_SECOND;
  ui->engine = engine;

  engine->player = gst_element_factory_make ("playbin2", "playbin2");
  if (engine->player == NULL) {
    g_print ("ERROR: Failed to create playbin element\n");
    ret = 1;
    goto quit;
  }

  texture = clutter_texture_new ();
  engine->sink = clutter_gst_video_sink_new (CLUTTER_TEXTURE (texture));
  g_object_set (G_OBJECT (engine->player), "video-sink", engine->sink, NULL);
  engine->bus = gst_pipeline_get_bus (GST_PIPELINE (engine->player));
  gst_bus_add_watch (engine->bus, bus_call, ui);
  gst_object_unref (engine->bus);
  ui->texture = texture;

  fileuri = clean_uri (file_list[0]);
  g_print ("Loading: %s\n", fileuri);
  engine->uri = NULL;
  asprintf (&engine->uri, "file://%s", fileuri);
  g_object_set (G_OBJECT (engine->player), "uri", engine->uri, NULL);
  engine->fileuri = fileuri;
  ui->fileuri = fileuri;
  gst_element_set_state (engine->player, GST_STATE_PAUSED);
  engine->playing = FALSE;
  engine->media_duration = -1;

  gst_element_set_state (engine->player, GST_STATE_PLAYING);
  engine->playing = TRUE;
  clutter_main ();

  gst_element_set_state (engine->player, GST_STATE_NULL);
  gst_object_unref (engine->player);

  screensaver_enable (ui->screensaver, TRUE);
  screensaver_free (ui->screensaver);

quit:
  g_option_context_free (context);

  return ret;
}
