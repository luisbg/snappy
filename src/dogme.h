/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-13    01
 * USA
 */

#define SEEK_HEIGHT 20
#define SEEK_WIDTH 640

typedef struct GstEngine GstEngine;
typedef struct UserInterface UserInterface;

struct GstEngine
{
	gchar		*uri, *filepath;
	guint		media_width, media_height;
	gboolean	playing;
	gint64		media_duration;

	GstElement	*player;
	GstElement	*sink;

	GstBus		*bus;
	UserInterface *ui;
};

struct UserInterface
{
	gchar		 *filename, *filepath;
	ClutterActor *stage;

	ClutterColor stage_color, control_color1, control_color2;
	
	ClutterActor *texture;
	ClutterActor *control;
	ClutterActor *control_bg, *control_label, *control_play,
			*control_pause, *control_seek1, *control_seek2,
			*control_seekbar;

	gboolean	controls_showing, fullscreen;
	guint		controls_timeout;

	guint		media_width, media_height;
	guint		stage_width, stage_height;

	GstEngine   *engine;
	
};

static void show_controls (UserInterface *ui, gboolean vis);
static gboolean controls_timeout_cb (gpointer data);
static void center_controls (UserInterface *ui);
static void toggle_playing (UserInterface *ui, GstEngine *engine);
static void toggle_fullscreen (UserInterface *ui);
static void size_change (ClutterStage *stage, gpointer *data);
static gboolean event_cb (ClutterStage *stage, ClutterEvent *event,
							gpointer data);
static gboolean update_media_duration (GstEngine *engine);
static gboolean progress_update (gpointer data);
static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data);
static gchar * cut_long_filename (gchar *filename);
static void load_user_interface (UserInterface *ui);
