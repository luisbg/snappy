/*
 * gcc `pkg-config --libs --cflags clutter-1.0 clutter-glx-1.0 gstreamer-0.10 clutter-gst-0.10` dogme.c -o dogme
 * or (if your pkg-config has clutter-gst-1.0 instead of clutter-gst-0.10
 * gcc `pkg-config --libs --cflags clutter-1.0 clutter-glx-1.0 gstreamer-0.10 clutter-gst-1.0` dogme.c -o dogme
 *
 * Dogme video player.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-13    01
 * USA
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <clutter/clutter.h>
#include <clutter-gst/clutter-gst.h>
#include "dogme.h"

static void
show_controls (UserInterface *ui, gboolean vis)
{
	if (vis == TRUE && ui->controls_showing == TRUE)
	{
		if (ui->controls_timeout == 0)
		{
			ui->controls_timeout =
				g_timeout_add_seconds (3, controls_timeout_cb, ui);
		}

		return;
	}

	if (vis == TRUE && ui->controls_showing == FALSE)
	{
		ui->controls_showing = TRUE;

		clutter_stage_show_cursor (CLUTTER_STAGE (ui->stage));
		clutter_actor_animate (ui->control, CLUTTER_EASE_OUT_QUINT, 250,
			"opacity", 224,
			NULL);

		return;
	}

	if (vis == FALSE && ui->controls_showing == TRUE)
	{
		ui->controls_showing = FALSE;

		clutter_stage_hide_cursor (CLUTTER_STAGE (ui->stage));
		clutter_actor_animate (ui->control, CLUTTER_EASE_OUT_QUINT, 250,
			"opacity", 0,
			NULL);
		return;
	}
}

static gboolean
controls_timeout_cb (gpointer data)
{
	UserInterface *ui = data;

	ui->controls_timeout = 0;
	show_controls (ui, FALSE);

	return FALSE;
}

static void
center_controls (UserInterface *ui)
{
	gfloat x, y;

	x = (ui->stage_width - clutter_actor_get_width (ui->control)) / 2;
	y = ui->stage_height - (ui->stage_height / 3);

	g_debug ("stage width = %.2d, height = %.2d\n", ui->stage_width,
		ui->stage_height);
	g_debug ("setting x = %.2f, y = %.2f, width = %.2f\n",
		x, y, clutter_actor_get_width (ui->control));

	clutter_actor_set_position (ui->control, x, y);
}

static void
toggle_playing (UserInterface *ui, GstEngine *engine)
{
	if (engine->playing) {
		gst_element_set_state (engine->player, GST_STATE_PAUSED);
		engine->playing = FALSE;
		clutter_actor_hide (ui->control_pause);
		clutter_actor_show (ui->control_play);
	} else {
		gst_element_set_state (engine->player, GST_STATE_PLAYING);
		engine->playing = TRUE;
		clutter_actor_hide (ui->control_play);
		clutter_actor_show (ui->control_pause);
	}
}

static void
toggle_fullscreen (UserInterface *ui)
{
	if (ui->fullscreen) {
		clutter_stage_set_fullscreen (CLUTTER_STAGE (ui->stage), FALSE);
		ui->fullscreen = FALSE;
	} else {
		clutter_stage_set_fullscreen (CLUTTER_STAGE (ui->stage), TRUE);
		ui->fullscreen = TRUE;
	}
}

static void
size_change (ClutterStage *stage,
			gpointer		*data)
{
	UserInterface *ui = (UserInterface*)data;

	gfloat stage_width, stage_height;
	gfloat new_width, new_height;
	gfloat video_width, video_height;
	gfloat center, aratio;

	video_width = ui->engine->video_width;
	video_height = ui->engine->video_height;

	stage_width = clutter_actor_get_width (ui->stage);
	stage_height = clutter_actor_get_height (ui->stage);
	ui->stage_width = stage_width;
	ui->stage_height = stage_height;

	new_width = stage_width;
	new_height = stage_height;
	if (video_height <= video_width)
	{
		aratio = video_height / video_width;
		new_height = new_width * aratio;
		center = (stage_height - new_height) / 2;
		clutter_actor_set_position (CLUTTER_ACTOR (ui->texture), 0, center);
	} else {
		aratio = video_width / video_height;
		new_width = new_height * aratio;
		center = (stage_width - new_width) / 2;
		clutter_actor_set_position (CLUTTER_ACTOR (ui->texture), center, 0);
	}

	clutter_actor_set_size (CLUTTER_ACTOR (ui->texture),
							new_width, new_height);
	center_controls(ui);
}

static gboolean
event_cb (ClutterStage *stage,
		ClutterEvent *event,
		gpointer data)
{
	UserInterface *ui = (UserInterface*)data;
	gboolean handled = FALSE;

	switch (event->type) {
		case CLUTTER_KEY_PRESS:
		{
			ClutterVertex center = { 0, };
			ClutterAnimation *animation = NULL;

			center.x - clutter_actor_get_width (ui->texture) / 2;
			guint keyval = clutter_event_get_key_symbol (event);
			switch (keyval) {
				case CLUTTER_q:
				case CLUTTER_Escape:
					clutter_main_quit ();
					break;
				case CLUTTER_f:
					// Fullscreen button
					toggle_fullscreen (ui);
					handled = TRUE;
					break;
				case CLUTTER_space:
					// Spacebar
					toggle_playing (ui, ui->engine);
					handled = TRUE;
					break;
				case CLUTTER_8:
				{
					// Mute button
					gboolean muteval;
					g_object_get (G_OBJECT (ui->engine->player), "mute",
									&muteval, NULL);
					g_object_set (G_OBJECT (ui->engine->player), "mute",
									! muteval, NULL);
					if (muteval) {
						g_debug ("Unmute stream\n");
					} else {
						g_debug ("Mute stream\n");
					}
					handled = TRUE;
					break;
				}

				case CLUTTER_9:
				case CLUTTER_0:
				{
					gdouble volume;
					g_object_get (G_OBJECT (ui->engine->player), "volume",
									&volume, NULL);
					// Volume Down
					if (keyval == CLUTTER_9 && volume > 0.0) {
						g_object_set (G_OBJECT (ui->engine->player), "volume",
										volume -= 0.05, NULL);
						g_debug ("Volume down: %f", volume);

					// Volume Up
					} else if (keyval == CLUTTER_0 && volume < 1.0) {
						g_object_set (G_OBJECT (ui->engine->player), "volume",
										volume += 0.05, NULL);
						g_debug ("Volume up: %f", volume);
					}
					handled = TRUE;
					break;
				}

				case CLUTTER_Up:
				case CLUTTER_Down:
				case CLUTTER_Left:
				case CLUTTER_Right:
				{
					gint64 pos;
					GstFormat fmt = GST_FORMAT_TIME;
					gst_element_query_position (ui->engine->player, &fmt,
												&pos);
					// Seek 1 minute foward
					if (keyval == CLUTTER_Up) {
						pos += 60 * GST_SECOND;
						g_debug("Skipping 1 minute ahead in the stream\n");

					// Seek 1 minute back
					} else if (keyval == CLUTTER_Down) {
						pos -= 60 * GST_SECOND;
						g_debug("Moving 1 minute back in the stream\n");

					// Seek 10 seconds back
					} else if (keyval == CLUTTER_Left) {
						pos -= 10 * GST_SECOND;
						g_debug("Moving 10 seconds back in the stream\n");

					// Seek 10 seconds foward
					} else if (keyval == CLUTTER_Right) {
						pos += 10 * GST_SECOND;
						g_debug("Skipping 10 seconds ahead in the stream\n");
					}

					gst_element_seek_simple (ui->engine->player,
											fmt, GST_SEEK_FLAG_FLUSH,
											pos);

					gfloat progress = (float) pos / ui->engine->video_duration;
					clutter_actor_set_size (ui->control_seekbar,
											progress * SEEK_WIDTH,
											SEEK_HEIGHT);

					handled = TRUE;
					break;
				}
				case CLUTTER_r:
					// rotate video 90 degrees.
					handled = TRUE;
					break;

				default:
					handled = FALSE;
					break;
			}
		}

		case CLUTTER_BUTTON_PRESS:
		{
			if (ui->controls_showing) {
				ClutterActor *actor;
				ClutterButtonEvent *bev = (ClutterButtonEvent *) event;

				actor = clutter_stage_get_actor_at_pos (stage,
							CLUTTER_PICK_ALL, bev->x, bev->y);
				if (actor == ui->control_pause || actor == ui->control_play) {
					toggle_playing (ui, ui->engine);
				}
				else if (actor == ui->control_seek1 ||
						actor == ui->control_seek2 ||
						actor == ui->control_seekbar) {
					gfloat x, y, dist;
					gint64 progress;

					clutter_actor_get_transformed_position (ui->control_seekbar,
															&x, &y);
					dist = bev->x - x;
					dist = CLAMP (dist, 0, SEEK_WIDTH);

					if (ui->engine->video_duration == -1)
					{
						update_video_duration (ui->engine);
					}

					progress = ui->engine->video_duration * (dist / SEEK_WIDTH);
					gst_element_seek_simple (ui->engine->player,
										GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
										progress);
					clutter_actor_set_size (ui->control_seekbar,
							dist, SEEK_HEIGHT);
				}
			}
			handled = TRUE;
			break;
		}

		case CLUTTER_MOTION:
		{
			show_controls (ui, TRUE);
			handled = TRUE;
			break;
		}
	}

	return handled;
}

static gboolean
update_video_duration (GstEngine *engine)
{
	gboolean success = FALSE;

	GstFormat fmt = GST_FORMAT_TIME;
	if (gst_element_query_duration (engine->player, &fmt,
			&engine->video_duration))
	{
		if (engine->video_duration != -1 && fmt == GST_FORMAT_TIME) {
			g_debug ("Media duration: %ld\n", engine->video_duration);
			success = TRUE;
		} else {
			g_debug ("Could not get media's duration\n");
			success = FALSE;
		}
	}

	return success;
}

static gboolean
progress_update (gpointer data)
{
	UserInterface *ui = (UserInterface*)data;
	GstEngine *engine = ui->engine;
	gfloat progress = 0.0;

	if (engine->video_duration == -1)
	{
		update_video_duration (engine);
	}

	gint64 pos;
	GstFormat fmt = GST_FORMAT_TIME;
	gst_element_query_position (engine->player, &fmt, &pos);
	progress = (float) pos / engine->video_duration;
	g_debug ("playback position progress: %f\n", progress);

	clutter_actor_set_size (ui->control_seekbar, progress * SEEK_WIDTH,
			SEEK_HEIGHT);

	return TRUE;
}

static gboolean
bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
	GstEngine *engine = (GstEngine*)data;

	switch (GST_MESSAGE_TYPE (msg)) {
		case GST_MESSAGE_EOS:
			g_debug ("End-of-stream\n");
			break;
		case GST_MESSAGE_ERROR: {
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
			if (new == GST_STATE_PAUSED)
			{
				if (engine->video_width == -1)
				{
					GstPad *p = gst_element_get_pad (engine->sink, "sink");
					GstCaps *c = gst_pad_get_negotiated_caps (p);
					if (c)
					{
						GstStructure *s = gst_caps_get_structure (c, 0);
						const GValue *widthval, *heightval;
						widthval = gst_structure_get_value (s, "width");
						heightval = gst_structure_get_value (s, "height");
						if (G_VALUE_HOLDS (widthval, G_TYPE_INT))
						{
							gint width, height;
							width = g_value_get_int (widthval);
							height = g_value_get_int (heightval);
							g_debug ("Setting width: %d, height: %d\n", width,
										height);
							engine->video_width = width;
							engine->video_height = height;
							load_user_interface (engine->ui);
						}
					}
				}
			}

			break;
		}
		default:
			break;
	}

	return TRUE;
}

static gchar *
cut_long_filename(gchar *filename)
{
	gchar *ret;
	gint c;
	gint max_size = 32;
	for (c = 0; filename[c] != '\0'; c++);

	if (c > max_size)
	{
		gchar short_filename[max_size];

		for (c = 0; c < (max_size -1); c++)
		{
			short_filename[c] = filename[c];
		}
		short_filename[max_size - 1] = '\0';
		ret = g_locale_to_utf8 (short_filename, max_size, NULL, NULL, NULL);
	} else {
		ret = g_locale_to_utf8 (filename, -1, NULL, NULL, NULL);
	}

	return ret;
}

static void
load_user_interface (UserInterface *ui)
{
	char *env;
	env = getenv("PWD");

	// Stage
	ClutterColor stage_color = { 0x00, 0x00, 0x00, 0x00 };
	ClutterColor control_color1 = { 73, 74, 77, 0xee };
	ClutterColor control_color2 = { 0xcc, 0xcc, 0xcc, 0xff };
	ui->filename = g_path_get_basename (ui->filepath);

	ui->video_width = ui->engine->video_width;
	ui->video_height = ui->engine->video_height;
	ui->stage_width = ui->engine->video_width;
	ui->stage_height = ui->engine->video_height;
	ui->stage = clutter_stage_get_default();
	clutter_stage_set_color (CLUTTER_STAGE (ui->stage), &stage_color);
	clutter_stage_set_minimum_size (CLUTTER_STAGE (ui->stage),
			ui->stage_width, ui->stage_height);
	clutter_stage_set_title (CLUTTER_STAGE (ui->stage), ui->filename);

	if (ui->fullscreen) {
		clutter_stage_set_fullscreen (CLUTTER_STAGE (ui->stage), TRUE);
	} else {
		clutter_actor_set_size (CLUTTER_ACTOR (ui->stage), ui->stage_width,
								ui->stage_height);
	}

	// Controls
	char *vid_panel_png = malloc (strlen (env) + strlen("/../img/vid-panel.png") + 2);
	sprintf (vid_panel_png, "%s%s", env, "/../img/vid-panel.png");
	char *play_png = malloc (strlen (env) + strlen("/../img/media-actions-start.png") + 2);
	sprintf (play_png, "%s%s", env, "/../img/media-actions-start.png");
	char *pause_png = malloc (strlen (env) + strlen("/../img/media-actions-pause.png") + 2);
	sprintf (pause_png, "%s%s", env, "/../img/media-actions-pause.png");

	ui->control = clutter_group_new ();
	ui->control_bg =
		clutter_texture_new_from_file (vid_panel_png, NULL);
	ui->control_play = 
		clutter_texture_new_from_file (play_png, NULL);
	ui->control_pause =
		clutter_texture_new_from_file (pause_png, NULL);

	g_assert (ui->control_bg && ui->control_play && ui->control_pause);

	ui->control_seek1   = clutter_rectangle_new_with_color (&control_color1);
	ui->control_seek2   = clutter_rectangle_new_with_color (&control_color2);
	ui->control_seekbar = clutter_rectangle_new_with_color (&control_color1);
	clutter_actor_set_opacity (ui->control_seekbar, 0x99);

	ui->control_label =
		clutter_text_new_full ("Sans Bold 24", cut_long_filename (ui->filename),
								&control_color1);
 
	clutter_actor_hide (ui->control_play);
 
	clutter_container_add (CLUTTER_CONTAINER (ui->control),
							ui->control_bg,
							ui->control_play,
							ui->control_pause,
							ui->control_seek1,
							ui->control_seek2,
							ui->control_seekbar,
							ui->control_label,
							NULL);
 
	clutter_actor_set_opacity (ui->control, 0xee);

	clutter_actor_set_position (ui->control_play, 30, 30);
	clutter_actor_set_position (ui->control_pause, 30, 30);

	clutter_actor_set_size (ui->control_seek1, SEEK_WIDTH+10, SEEK_HEIGHT+10);
	clutter_actor_set_position (ui->control_seek1, 200, 100);
	clutter_actor_set_size (ui->control_seek2, SEEK_WIDTH, SEEK_HEIGHT);
	clutter_actor_set_position (ui->control_seek2, 205, 105);
	clutter_actor_set_size (ui->control_seekbar, 0, SEEK_HEIGHT);
	clutter_actor_set_position (ui->control_seekbar, 205, 105);

	clutter_actor_set_position (ui->control_label, 200, 40);

	// Add control UI to stage
	clutter_container_add (CLUTTER_CONTAINER (ui->stage),
							ui->texture,
							ui->control,
							NULL);

	clutter_stage_hide_cursor (CLUTTER_STAGE (ui->stage));
	clutter_actor_animate (ui->control, CLUTTER_EASE_OUT_QUINT, 1000,
						"opacity", 0, NULL);

	g_signal_connect (CLUTTER_TEXTURE (ui->stage), "fullscreen",
						G_CALLBACK (size_change), ui);
	g_signal_connect (CLUTTER_TEXTURE (ui->stage), "unfullscreen",
						G_CALLBACK (size_change), ui);
	g_signal_connect (ui->stage, "event", G_CALLBACK (event_cb), ui);

	g_timeout_add (2000, progress_update, ui);

	center_controls (ui);
	clutter_actor_show(ui->stage);
}

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
	engine->video_width = -1;
	engine->video_height = -1;
	engine->ui = ui;
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
	gst_bus_add_watch (engine->bus, bus_call, engine);
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
	engine->video_duration = -1;

	gst_element_set_state (engine->player, GST_STATE_PLAYING);
	engine->playing = TRUE;
	clutter_main ();

	return 0;
}
