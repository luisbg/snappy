/*
 * snappy - 0.1 beta
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

#ifndef __USER_INTERFACE_H__
#define __USER_INTERFACE_H__

#include "gst_engine.h"

#define CTL_BORDER 20
#define SHADOW_CORRECT 15
#define CTL_SPACING 10

#define SEEK_HEIGHT 20
#define SEEK_WIDTH 640
#define SEEK_BORDER 5

G_BEGIN_DECLS

typedef struct _UserInterface UserInterface;

struct _UserInterface
{
	gchar        *filename, *fileuri;
	gchar        *play_png, *pause_png;
	ClutterActor *stage;

	ClutterColor stage_color, control_color1, control_color2;

	ClutterActor *texture;
	ClutterActor *control_box;
	ClutterActor *control_bg, *control_title, *control_play_toggle;
	ClutterActor *control_seek1, *control_seek2, *control_seekbar;

	gboolean    controls_showing, fullscreen;
	guint       controls_timeout;

	guint       media_width, media_height;
	guint       stage_width, stage_height;


	GstEngine   *engine;
};

// Declaration of non-static functions
void load_user_interface (UserInterface *ui);

G_END_DECLS

#endif /* __USER_INTERFACE_H__ */
