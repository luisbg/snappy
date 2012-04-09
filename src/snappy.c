/*
 * snappy - 0.2
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

#define VERSION "0.2"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <clutter/clutter.h>
#include <clutter-gst/clutter-gst.h>

#include "user_interface.h"

#ifdef ENABLE_DBUS
#include "dlna.h"
#endif

#include "gst_engine.h"
#include "utils.h"


/*               Close snappy down               */
void
close_down (UserInterface * ui, GstEngine * engine)
{
  g_print ("closing snappy\n");

  /* Save position if file isn't finished playing */
  add_uri_unfinished (engine);

  /* Close gstreamer gracefully */
  change_state (engine, "Null");

  /* Re-enable screensaver */
  screensaver_enable (ui->screensaver, TRUE);
  screensaver_free (ui->screensaver);

  gst_object_unref (G_OBJECT (engine->player));
}


/*           Process command arguments           */
gboolean
process_args (int argc, char *argv[],
    gchar * file_list[], gboolean * fullscreen, gboolean * secret,
    gchar ** suburi, gboolean * loop, GOptionContext * context)
{
  gboolean recent = FALSE, version = FALSE;
  guint c, index, pos = 0;
  GOptionEntry entries[] = {
    {"fullscreen", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, fullscreen,
        "Fullscreen mode", NULL},
    {"loop", 'l', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, loop,
        "Looping mode", NULL},
    {"recent", 'r', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &recent,
        "Show recently viewed", NULL},
    {"secret", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, secret,
        "Views not saved in recently viewed history", NULL},
    {"subtitles", 't', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_FILENAME,
        suburi, "Use this subtitle file", NULL},
    {"version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
        "Shows snappy's version", NULL},
    {NULL}
  };
  GError *err = NULL;

  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, gst_init_get_option_group ());
  g_option_context_add_group (context, clutter_get_option_group ());

  /* Check command arguments and update entry variables */
  if (!g_option_context_parse (context, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", err->message);
    g_error_free (err);
    goto quit;
  }

  /* Recently viewed uris */
  if (recent) {
    gchar **recent = NULL;

    g_print ("These are the recently viewed URIs: \n\n");

    recent = get_recently_viewed ();

    for (c = 0; recent[c] != NULL; c++) {
      if (c < 9)
        g_print ("0%d: %s \n", c + 1, recent[c]);
      else
        g_print ("%d: %s \n", c + 1, recent[c]);
    }

    goto quit;
  }

  /* Show snappy's version */
  if (version) {
    g_print ("snappy version %s\n", VERSION);
    goto quit;
  }

  /* Check that at least one URI has been introduced */
  if (argc < 2) {
    g_print ("%s", g_option_context_get_help (context, TRUE, NULL));
    goto quit;
  }

  /* Save uris in the file_list array */
  for (index = 1; index < argc; index++) {
    file_list[pos] = argv[index];
    g_debug ("Adding file: %s\n", file_list[pos]);
    pos++;
  }

  return TRUE;

quit:
  return FALSE;
}


/*            snappy's main function             */
int
main (int argc, char *argv[])
{
  UserInterface *ui = NULL;
  GstEngine *engine = NULL;
  ClutterActor *video_texture;
  GstElement *sink;

  gboolean ok, fullscreen = FALSE, loop = FALSE, secret = FALSE;
  gint ret = 0;
  guint c, index, pos = 0;
  gchar *uri;
  gchar *file_list[argc];
  gchar *suburi = NULL;
  GOptionContext *context;

#ifdef ENABLE_DBUS
  SnappyMP *mp_obj = NULL;
#endif

  if (!g_thread_supported ())
    g_thread_init (NULL);

  context = g_option_context_new ("<media file> - Play movie files");

  /* Process command arguments */
  ok = process_args (argc, argv, file_list, &fullscreen, &secret, &suburi,
      &loop, context);
  if (!ok)
    goto quit;

  /* User Interface */
  ui = g_new (UserInterface, 1);
  ui->fullscreen = fullscreen;
  interface_init (ui);
  video_texture = clutter_texture_new ();

  clutter_gst_init (&argc, &argv);

  /* Gstreamer engine */
  engine = g_new (GstEngine, 1);
  sink = gst_element_factory_make ("cluttersink", "cluttersink");
  g_object_set (G_OBJECT (sink), "texture", CLUTTER_TEXTURE (video_texture),
      NULL);

  ok = engine_init (engine, sink);
  if (!ok)
    goto quit;

  engine->secret = secret;
  engine->loop = loop;

  ui->engine = engine;
  ui->texture = video_texture;

  gst_bus_add_watch (engine->bus, bus_call, ui);
  gst_object_unref (engine->bus);

  /* Get uri to load */
  uri = clean_uri (file_list[0]);

  /* Load engine and start interface */
  engine_load_uri (engine, uri);
  interface_start (ui, uri);

  /* Load subtitle file if available */
  if (suburi != NULL) {
    suburi = clean_uri (suburi);
    set_subtitle_uri (engine, suburi);
  }

  /* Start playing */
  change_state (engine, "Paused");
  change_state (engine, "Playing");

#ifdef ENABLE_DBUS
  /* Start MPRIS Dbus object */
  mp_obj = g_new (SnappyMP, 1);
  mp_obj->engine = engine;
  mp_obj->ui = ui;
  load_dlna (mp_obj);
#endif

  /* Main loop */
  clutter_main ();

  /* Close snappy */
  close_down (ui, engine);
#ifdef ENABLE_DBUS
  close_dlna (mp_obj);
#endif

quit:
  g_option_context_free (context);

  return ret;
}
