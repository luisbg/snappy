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

#include <string.h>
#include <clutter/clutter.h>
#include <clutter-gst/clutter-gst.h>

#include "user_interface.h"
#include "utils.h"

// Declaration of static functions
static void center_controls (UserInterface * ui);
static gboolean controls_timeout_cb (gpointer data);
static gboolean event_cb (ClutterStage * stage, ClutterEvent * event,
    gpointer data);
static void load_controls (UserInterface * ui);
static gboolean penalty_box (gpointer data);
static gchar * position_ns_to_str (gint64 nanoseconds);
static void progress_timing (UserInterface * ui);
static gboolean progress_update_text (gpointer data);
static gboolean progress_update_seekbar (gpointer data);
static void size_change (ClutterStage * stage, gpointer * data);
static void show_controls (UserInterface * ui, gboolean vis);
static void toggle_fullscreen (UserInterface * ui);
static void toggle_playing (UserInterface * ui, GstEngine * engine);
static void update_controls_size (UserInterface * ui);


/* ---------------------- static functions ----------------------- */

static void
center_controls (UserInterface * ui)
{
  gfloat x, y;

  x = (ui->stage_width - clutter_actor_get_width (ui->control_box)) / 2;
  y = ui->stage_height - (ui->stage_height / 3);

  clutter_actor_set_position (ui->control_box, x, y);
}

static gboolean
controls_timeout_cb (gpointer data)
{
  UserInterface *ui = data;

  ui->controls_timeout = -1;

  clutter_stage_hide_cursor (CLUTTER_STAGE (ui->stage));
  if (!ui->keep_showing_controls) {
    show_controls (ui, FALSE);
  }

  return FALSE;
}

static gboolean
event_cb (ClutterStage * stage, ClutterEvent * event, gpointer data)
{
  UserInterface *ui = (UserInterface *) data;
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
          g_object_get (G_OBJECT (ui->engine->player), "mute", &muteval, NULL);
          g_object_set (G_OBJECT (ui->engine->player), "mute", !muteval, NULL);
          handled = TRUE;
          break;
        }

        case CLUTTER_9:
        case CLUTTER_0:
        {
          gdouble volume;
          g_object_get (G_OBJECT (ui->engine->player), "volume", &volume, NULL);
          // Volume Down
          if (keyval == CLUTTER_9 && volume > 0.0) {
            g_object_set (G_OBJECT (ui->engine->player), "volume",
                volume -= 0.05, NULL);

            // Volume Up
          } else if (keyval == CLUTTER_0 && volume < 1.0) {
            g_object_set (G_OBJECT (ui->engine->player), "volume",
                volume += 0.05, NULL);
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
          gst_element_query_position (ui->engine->player, &fmt, &pos);
          // Seek 1 minute foward
          if (keyval == CLUTTER_Up) {
            pos += 60 * GST_SECOND;

            // Seek 1 minute back
          } else if (keyval == CLUTTER_Down) {
            pos -= 60 * GST_SECOND;

            // Seek 10 seconds back
          } else if (keyval == CLUTTER_Left) {
            pos -= 10 * GST_SECOND;

            // Seek 10 seconds foward
          } else if (keyval == CLUTTER_Right) {
            pos += 10 * GST_SECOND;
          }

          /* clamp the timestamp to be within the media */
          pos = CLAMP (pos, 0, ui->engine->media_duration);

          gst_element_seek_simple (ui->engine->player, fmt,
              GST_SEEK_FLAG_FLUSH, pos);

          gfloat progress = (float) pos / ui->engine->media_duration;
          clutter_actor_set_size (ui->control_seekbar,
              progress * ui->seek_width, ui->seek_height);
	  progress_update_text (ui);

          handled = TRUE;
          break;
        }
        case CLUTTER_r:
          // rotate texture 90 degrees.
          handled = TRUE;
          break;

        case CLUTTER_c:
          // show or hide controls
          penalty_box (ui);
          ui->keep_showing_controls = !ui->controls_showing;
          show_controls (ui, !ui->controls_showing);

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

        actor = clutter_stage_get_actor_at_pos (stage, CLUTTER_PICK_ALL,
            bev->x, bev->y);
        if (actor == ui->control_play_toggle) {
          toggle_playing (ui, ui->engine);
        }
        else if (actor == ui->control_seek1 ||
            actor == ui->control_seek2 || actor == ui->control_seekbar) {
          gfloat x, y, dist;
          gint64 progress;

          clutter_actor_get_transformed_position (ui->control_seekbar, &x, &y);
          dist = bev->x - x;
          dist = CLAMP (dist, 0, ui->seek_width);

          if (ui->engine->media_duration == -1) {
            update_media_duration (ui->engine);
          }

          progress = ui->engine->media_duration * (dist / ui->seek_width);
          gst_element_seek_simple (ui->engine->player, GST_FORMAT_TIME,
              GST_SEEK_FLAG_FLUSH, progress);
          clutter_actor_set_size (ui->control_seekbar, dist, ui->seek_height);
	  progress_update_text (ui);
        }
        else if (actor == ui->control_bg || actor == ui->control_title
            || actor == ui->control_pos)
        {
          ui->keep_showing_controls = !ui->keep_showing_controls;

          if (ui->keep_showing_controls)
          {
            clutter_stage_hide_cursor (CLUTTER_STAGE (ui->stage));
          } else {
            penalty_box (ui);
            show_controls (ui, FALSE);
          }
        }
        else if (actor == ui->texture || actor == ui->stage)
        {
          if (!ui->penalty_box_active)
          {
            penalty_box (ui);
            show_controls (ui, FALSE);
          }
        }
      }
      handled = TRUE;
      break;
    }

    case CLUTTER_MOTION:
    {
      if (!ui->penalty_box_active)
        show_controls (ui, TRUE);
      handled = TRUE;
      break;
    }
  }

  return handled;
}

static void
load_controls (UserInterface * ui)
{
  // Check icon files exist
  gchar *vid_panel_png;
  gchar *icon_files[3];
  gchar *duration_str;
  gint c;
  ClutterColor control_color1 = { 0x12, 0x12, 0x12, 0xff };
  ClutterColor control_color2 = { 0xcc, 0xcc, 0xcc, 0xff };
  ClutterLayoutManager *controls_layout;
  ClutterLayoutManager *main_box_layout;
  ClutterLayoutManager *info_box_layout;
  ClutterActor *info_box;
  ClutterLayoutManager *seek_box_layout;
  ClutterActor *seek_box;

  vid_panel_png = g_strdup_printf ("%s%s", SNAPPY_DATA_DIR,
      "/vid-panel.png");
  ui->play_png = g_strdup_printf ("%s%s", SNAPPY_DATA_DIR,
      "/media-actions-start.png");
  ui->pause_png = g_strdup_printf ("%s%s", SNAPPY_DATA_DIR,
      "/media-actions-pause.png");

  icon_files[0] = vid_panel_png;
  icon_files[1] = ui->play_png;
  icon_files[2] = ui->pause_png;

  for (c = 0; c < 3; c++) {
    if (!g_file_test (icon_files[c], G_FILE_TEST_EXISTS)) {
      g_print ("Icon file doesn't exist, are you sure you have "
          " installed snappy correctly?\nThis file needed is: %s\n",
          icon_files[c]);

    }
  }

  // Controls layout management
  controls_layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_FIXED,
      CLUTTER_BIN_ALIGNMENT_FIXED);
  ui->control_box = clutter_box_new (controls_layout);

  // Controls background
  ui->control_bg = clutter_texture_new_from_file (vid_panel_png, NULL);
  g_free (vid_panel_png);
  clutter_container_add_actor (CLUTTER_CONTAINER (ui->control_box),
      ui->control_bg);

  // Controls play toggle
  main_box_layout = clutter_box_layout_new ();
  clutter_box_layout_set_vertical (CLUTTER_BOX_LAYOUT (main_box_layout), FALSE);
  ui->main_box = clutter_box_new (main_box_layout);
  clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (main_box_layout),
      CTL_SPACING);
  ui->control_play_toggle = clutter_texture_new_from_file (ui->pause_png, NULL);
  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (main_box_layout),
      ui->control_play_toggle, FALSE,        /* expand */
      FALSE,                                 /* x-fill */
      FALSE,                                 /* y-fill */
      CLUTTER_BOX_ALIGNMENT_START,           /* x-align */
      CLUTTER_BOX_ALIGNMENT_CENTER);         /* y-align */
  clutter_actor_set_position (ui->main_box, CTL_BORDER, CTL_BORDER);
  clutter_container_add_actor (CLUTTER_CONTAINER (ui->control_box),
      ui->main_box);
  g_assert (ui->control_bg && ui->control_play_toggle);

  // Controls title
  info_box_layout = clutter_box_layout_new ();
  clutter_box_layout_set_vertical (CLUTTER_BOX_LAYOUT (info_box_layout), TRUE);
  info_box = clutter_box_new (info_box_layout);

  ui->control_title = clutter_text_new_full ("Sans 32px",
      cut_long_filename (ui->filename, ui->title_length), &control_color1);
  clutter_text_set_max_length (CLUTTER_TEXT (ui->control_title),
      ui->title_length);
  clutter_box_pack (CLUTTER_BOX (info_box), ui->control_title, "x-align",
      CLUTTER_BOX_ALIGNMENT_CENTER, NULL);

  // Controls seek
  seek_box_layout = clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_FIXED,
      CLUTTER_BIN_ALIGNMENT_FIXED);
  seek_box = clutter_box_new (seek_box_layout);

  ui->control_seek1 = clutter_rectangle_new_with_color (&control_color1);
  clutter_container_add_actor (CLUTTER_CONTAINER (seek_box), ui->control_seek1);

  ui->control_seek2 = clutter_rectangle_new_with_color (&control_color2);
  clutter_container_add_actor (CLUTTER_CONTAINER (seek_box), ui->control_seek2);

  ui->control_seekbar = clutter_rectangle_new_with_color (&control_color1);
  clutter_container_add_actor (CLUTTER_CONTAINER (seek_box), ui->control_seekbar);

  clutter_box_pack (CLUTTER_BOX (info_box), seek_box, "x-fill", FALSE,
      "y-fill", TRUE, NULL);

  clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (main_box_layout), info_box,
      FALSE,                            /* expand */
      FALSE,                            /* x-fill */
      FALSE,                            /* y-fill */
      CLUTTER_BOX_ALIGNMENT_START,      /* x-align */
      CLUTTER_BOX_ALIGNMENT_CENTER);    /* y-align */

  // Controls position text
  duration_str = g_strdup_printf ("0:00:00/%s", ui->duration_str);
  ui->control_pos = clutter_text_new_full ("Sans 22px", duration_str,
      &control_color1);
  clutter_box_pack (CLUTTER_BOX (info_box), ui->control_pos, "x-align",
      CLUTTER_BOX_ALIGNMENT_END, NULL);

  clutter_actor_lower_bottom (ui->control_bg);

  update_controls_size (ui);
}

static gboolean
penalty_box (gpointer data)
{
  UserInterface *ui = (UserInterface *) data;

  if (ui->penalty_box_active)
  {
    ui->penalty_box_active = FALSE;
  } else {
    g_timeout_add (PENALTY_TIME, penalty_box, ui);
    ui->penalty_box_active = TRUE;
  }

  return ui->penalty_box_active;
}

static gchar *
position_ns_to_str (gint64 nanoseconds)
{
  gint64 seconds;
  gint hours, minutes;

  seconds = nanoseconds / NANOSEC;
  hours = seconds / SEC_IN_HOUR;
  seconds = seconds - (hours * SEC_IN_HOUR);
  minutes = seconds / SEC_IN_MIN;
  seconds = seconds - (minutes * SEC_IN_MIN);

  return g_strdup_printf("%d:%02d:%02ld", hours, minutes, seconds);
}

static void
progress_timing (UserInterface * ui)
{
  gint64 duration_ns;
  gint64 timeout_ms;

  if (ui->progress_id != -1)
    g_source_remove (ui->progress_id);

  duration_ns = ui->engine->media_duration / MILISEC;
  timeout_ms = duration_ns / ui->seek_width;

  ui->progress_id = g_timeout_add (timeout_ms, progress_update_seekbar, ui);
}

static gboolean
progress_update_text (gpointer data)
{
  UserInterface *ui = (UserInterface *) data;

  if (ui->controls_showing) {
    GstEngine *engine = ui->engine;
    gchar *duration_str;
    gint64 pos;
    GstFormat fmt = GST_FORMAT_TIME;

    gst_element_query_position (engine->player, &fmt, &pos);
    duration_str = g_strdup_printf ("%s/%s", position_ns_to_str (pos),
        ui->duration_str);
    clutter_text_set_text (CLUTTER_TEXT (ui->control_pos), duration_str);
  }

  return TRUE;
}

static gboolean
progress_update_seekbar (gpointer data)
{
  UserInterface *ui = (UserInterface *) data;

  if (ui->controls_showing) {
    GstEngine *engine = ui->engine;
    gint64 pos;
    gfloat progress = 0.0;
    GstFormat fmt = GST_FORMAT_TIME;

    if (engine->media_duration == -1) {
      update_media_duration (engine);
    }

    gst_element_query_position (engine->player, &fmt, &pos);
    progress = (float) pos / engine->media_duration;

    clutter_actor_set_size (ui->control_seekbar, progress * ui->seek_width,
        ui->seek_height);
  }

  return TRUE;
}

static void
size_change (ClutterStage * stage, gpointer * data)
{
  UserInterface *ui = (UserInterface *) data;

  gfloat stage_width, stage_height;
  gfloat new_width, new_height;
  gfloat media_width, media_height;
  gfloat center, aratio;

  media_width = ui->engine->media_width;
  media_height = ui->engine->media_height;

  stage_width = clutter_actor_get_width (ui->stage);
  stage_height = clutter_actor_get_height (ui->stage);
  ui->stage_width = stage_width;
  ui->stage_height = stage_height;

  new_width = stage_width;
  new_height = stage_height;
  if (media_height <= media_width) {
    aratio = media_height / media_width;
    new_height = new_width * aratio;
    center = (stage_height - new_height) / 2;
    clutter_actor_set_position (CLUTTER_ACTOR (ui->texture), 0, center);
  } else {
    aratio = media_width / media_height;
    new_width = new_height * aratio;
    center = (stage_width - new_width) / 2;
    clutter_actor_set_position (CLUTTER_ACTOR (ui->texture), center, 0);
  }

  clutter_actor_set_size (CLUTTER_ACTOR (ui->texture), new_width, new_height);
  update_controls_size (ui);
  center_controls (ui);
  progress_timing (ui);
}

static void
show_controls (UserInterface * ui, gboolean vis)
{
  gboolean cursor;

  if (vis == TRUE && ui->controls_showing == TRUE) {
    // ToDo: add 3 more seconds to the controls hiding delay
    g_object_get (G_OBJECT (ui->stage), "cursor-visible", &cursor, NULL);
    if (!cursor)
      clutter_stage_show_cursor (CLUTTER_STAGE (ui->stage));
    if (ui->controls_timeout == -1) {
      ui->controls_timeout = g_timeout_add_seconds (CTL_SHOW_SEC,
          controls_timeout_cb, ui);
    }
  }

  else if (vis == TRUE && ui->controls_showing == FALSE) {
    ui->controls_showing = TRUE;

    progress_update_seekbar (ui);
    progress_update_text (ui);
    clutter_stage_show_cursor (CLUTTER_STAGE (ui->stage));
    clutter_actor_animate (ui->control_box, CLUTTER_EASE_OUT_QUINT,
        CTL_FADE_DURATION, "opacity", 0xa0, NULL);

    if (ui->controls_timeout == -1) {
      ui->controls_timeout = g_timeout_add_seconds (CTL_SHOW_SEC,
          controls_timeout_cb, ui);
    }
  }
  
  else if (vis == FALSE && ui->controls_showing == TRUE) {
    ui->controls_showing = FALSE;

    clutter_stage_hide_cursor (CLUTTER_STAGE (ui->stage));
    clutter_actor_animate (ui->control_box, CLUTTER_EASE_OUT_QUINT,
        CTL_FADE_DURATION, "opacity", 0, NULL);
  }
}

static void
toggle_fullscreen (UserInterface * ui)
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
toggle_playing (UserInterface * ui, GstEngine * engine)
{
  if (engine->playing) {
    gst_element_set_state (engine->player, GST_STATE_PAUSED);
    engine->playing = FALSE;

    clutter_texture_set_from_file (CLUTTER_TEXTURE (ui->control_play_toggle),
        ui->play_png, NULL);
  } else {
    gst_element_set_state (engine->player, GST_STATE_PLAYING);
    engine->playing = TRUE;

    clutter_texture_set_from_file (CLUTTER_TEXTURE (ui->control_play_toggle),
        ui->pause_png, NULL);
  }
}

static void
update_controls_size (UserInterface * ui)
{
  gchar *font_name;
  gfloat ctl_width, ctl_height, title_width;

  clutter_actor_set_size (ui->control_play_toggle,
      ui->stage_width / PLAY_TOGGLE_RATIO,
      ui->stage_width / PLAY_TOGGLE_RATIO);

  font_name = g_strdup_printf ("Sans %dpx", (ui->stage_height / TITLE_RATIO));
  clutter_text_set_font_name (CLUTTER_TEXT (ui->control_title), font_name);

  ui->seek_width = ui->stage_width / SEEK_WIDTH_RATIO;
  title_width = clutter_actor_get_width (CLUTTER_ACTOR (ui->control_title));
  if (ui->seek_width < title_width)
    ui->seek_width = title_width;
  ui->seek_height = ui->stage_height / SEEK_HEIGHT_RATIO;

  clutter_actor_set_size (ui->control_seek1, ui->seek_width + (SEEK_BORDER * 2),
      ui->seek_height + (SEEK_BORDER * 2));
  clutter_actor_set_position (ui->control_seek1, 0, 0);

  clutter_actor_set_size (ui->control_seek2, ui->seek_width, ui->seek_height);
  clutter_actor_set_position (ui->control_seek2, SEEK_BORDER, SEEK_BORDER);

  progress_update_seekbar (ui);
  clutter_actor_set_position (ui->control_seekbar, SEEK_BORDER, SEEK_BORDER);

  clutter_actor_get_size (ui->main_box, &ctl_width, &ctl_height);
  clutter_actor_set_size (ui->control_bg, ctl_width + (CTL_BORDER * 2)
      + SHADOW_CORRECT, ctl_height + (CTL_BORDER * 2));

  font_name = g_strdup_printf ("Sans %dpx",
      (ui->stage_height / POS_RATIO));
  clutter_text_set_font_name (CLUTTER_TEXT (ui->control_pos), font_name);
}


/* -------------------- non-static functions --------------------- */

void
load_user_interface (UserInterface * ui)
{
  ClutterColor stage_color = { 0x00, 0x00, 0x00, 0x00 };

  // Init UserInterface structure variables
  ui->filename = g_path_get_basename (ui->fileuri);
  ui->media_width = ui->engine->media_width;
  ui->media_height = ui->engine->media_height;
  ui->stage_width = ui->engine->media_width;
  ui->stage_height = ui->engine->media_height;
  ui->stage = clutter_stage_get_default ();
  ui->controls_showing = FALSE;
  ui->keep_showing_controls = FALSE;
  ui->penalty_box_active = FALSE;
  ui->controls_timeout = -1;
  ui->seek_width = ui->stage_width / SEEK_WIDTH_RATIO;
  ui->seek_height = ui->stage_height / SEEK_HEIGHT_RATIO;
  ui->progress_id = -1;
  ui->title_length = TITLE_LENGTH;
  ui->duration_str = position_ns_to_str (ui->engine->media_duration);

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
  load_controls (ui);

  // Add video texture and control UI to stage
  clutter_container_add (CLUTTER_CONTAINER (ui->stage), ui->texture,
      ui->control_box, NULL);

  clutter_stage_hide_cursor (CLUTTER_STAGE (ui->stage));
  clutter_actor_animate (ui->control_box, CLUTTER_EASE_OUT_QUINT, SECOND,
      "opacity", 0, NULL);

  g_signal_connect (CLUTTER_STAGE (ui->stage), "fullscreen",
      G_CALLBACK (size_change), ui);
  g_signal_connect (CLUTTER_STAGE (ui->stage), "unfullscreen",
      G_CALLBACK (size_change), ui);
  g_signal_connect (ui->stage, "event", G_CALLBACK (event_cb), ui);

  center_controls (ui);
  progress_timing (ui);

  g_timeout_add (SECOND, progress_update_text, ui);

  clutter_actor_show (ui->stage);
}
