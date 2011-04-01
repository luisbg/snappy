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

#ifndef __USER_INTERFACE_H__
#define __USER_INTERFACE_H__

#include "gst_engine.h"
#include "screensaver.h"

#define CTL_SHOW_SEC 3
#define CTL_FADE_DURATION 250

#define CTL_BORDER 20
#define SHADOW_CORRECT 15
#define CTL_SPACING 10

#define PLAY_TOGGLE_RATIO 10
#define TITLE_RATIO 25
#define SEEK_WIDTH_RATIO 2.2
#define SEEK_HEIGHT_RATIO 45
#define SEEK_BORDER 5
#define POS_RATIO 38
#define VOLUME_ICON_RATIO 30
#define VOLUME_WIDTH_RATIO 5
#define VOLUME_HEIGHT_RATIO 60

#define TITLE_LENGTH 40

#define SECOND 1000
#define NANOSEC 1000000000
#define MILISEC 1000000
#define SEC_IN_HOUR 3600
#define SEC_IN_MIN 60

#define PENALTY_TIME 500

G_BEGIN_DECLS typedef struct _UserInterface UserInterface;

struct _UserInterface
{
  gboolean controls_showing, keep_showing_controls;
  gboolean fullscreen, penalty_box_active;
  gboolean rotated;

  gint title_length, controls_timeout, progress_id;
  guint media_width, media_height;
  guint stage_width, stage_height;
  gfloat seek_width, seek_height;
  gfloat volume_width, volume_height;

  gchar *filename, *fileuri;
  gchar *play_png, *pause_png;
  gchar *volume_low_png, *volume_high_png;
  gchar *duration_str;

  ClutterColor stage_color, control_color1, control_color2;

  ClutterActor *stage;
  ClutterActor *texture;
  ClutterActor *control_box;
  ClutterActor *control_bg, *control_title, *control_play_toggle;
  ClutterActor *control_seek1, *control_seek2, *control_seekbar;
  ClutterActor *control_pos;
  ClutterActor *volume_box;
  ClutterActor *volume_low, *volume_high;
  ClutterActor *vol_int, *vol_int_bg, *volume_point;
  ClutterActor *main_box;

  GstEngine *engine;
  ScreenSaver *screensaver;
};

// Declaration of non-static functions
gboolean interface_load_uri (UserInterface * ui, gchar * uri);
void load_user_interface (UserInterface * ui);
gboolean update_controls (UserInterface * ui);

G_END_DECLS
#endif /* __USER_INTERFACE_H__ */
