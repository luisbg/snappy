/*
 * gcc `pkg-config --libs --cflags clutter-1.0 clutter-glx-1.0 gstreamer-0.10
 * clutter-gst-0.10` utils.c user_interface.c gst_engine.c dogme.c -o dogme
 *
 * or (if your pkg-config has clutter-gst-1.0 instead of clutter-gst-0.10
 *
 * gcc `pkg-config --libs --cflags clutter-1.0 clutter-glx-1.0 gstreamer-0.10
 * clutter-gst-1.0` utils.c user_interface.c gst_engine.c dogme.c -o dogme
 *
 * Dogme media player.
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <clutter/clutter.h>
#include <clutter-gst/clutter-gst.h>

#include "user_interface.h"
#include "gst_engine.h"


int main (int argc, char *argv[])
{
	// Command line arguments.
	if (argc < 2)
	{
		g_print ("Usage: %s [options] <media_file>\n", argv[0]);
		return EXIT_FAILURE;
	}
	gboolean fullscreen = FALSE;
	guint c, index, pos = 0;
	gchar *file_list[argc];
	while ((c = getopt (argc, argv, "f")) != -1)
		switch (c)
		{
			case 'f':
				g_debug ("fullscreen!\n");
				fullscreen = TRUE;
				break;
		}
	for (index = optind; index < argc; index++)
	{
		g_debug ("Adding file: %s\n", argv[index]);
		file_list[pos] = argv[index];
		pos++;
	}

	// User Interface
	UserInterface *ui = NULL;
	ui = g_new0(UserInterface, 1);
	ui->fullscreen = fullscreen;

	clutter_gst_init (&argc, &argv);

	// Gstreamer
	GstEngine *engine = NULL;
	engine = g_new0(GstEngine, 1);
	engine->media_width = -1;
	engine->media_height = -1;
	ui->engine = engine;

	engine->player = gst_element_factory_make ("playbin2", "playbin2");
	if (engine->player == NULL){
		g_print ("ERROR: Failed to create playbin element\n");
		return 1;
	} 
	ClutterActor *texture = clutter_texture_new ();
	engine->sink = clutter_gst_video_sink_new (CLUTTER_TEXTURE (texture));
	g_object_set (G_OBJECT (engine->player), "video-sink", engine->sink, NULL);
	engine->bus = gst_pipeline_get_bus (GST_PIPELINE (engine->player));
	gst_bus_add_watch (engine->bus, bus_call, ui);
	gst_object_unref (engine->bus);
	ui->texture = texture;

	gchar *filepath = file_list[0];
	engine->uri = NULL;
	asprintf(&engine->uri, "file://%s", filepath);
	g_object_set (G_OBJECT (engine->player), "uri", engine->uri, NULL);
	engine->filepath = filepath;
	ui->filepath = filepath;
	GstStateChange ret;
	gst_element_set_state (engine->player, GST_STATE_PAUSED);
	engine->playing = FALSE;
	engine->media_duration = -1;

	gst_element_set_state (engine->player, GST_STATE_PLAYING);
	engine->playing = TRUE;
	clutter_main ();

	return 0;
}
