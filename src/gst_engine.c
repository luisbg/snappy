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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <clutter-gst/clutter-gst.h>

#include "user_interface.h"
#include "gst_engine.h"


/* -------------------- non-static functions --------------------- */

gboolean
bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
	UserInterface *ui = (UserInterface*)data;
	GstEngine *engine = ui->engine;

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
				if (engine->media_width == -1)
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
							engine->media_width = width;
							engine->media_height = height;
							load_user_interface (ui);
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

gboolean
update_media_duration (GstEngine *engine)
{
	gboolean success = FALSE;

	GstFormat fmt = GST_FORMAT_TIME;
	if (gst_element_query_duration (engine->player, &fmt,
			&engine->media_duration))
	{
		if (engine->media_duration != -1 && fmt == GST_FORMAT_TIME) {
			g_debug ("Media duration: %ld\n", engine->media_duration);
			success = TRUE;
		} else {
			g_debug ("Could not get media's duration\n");
			success = FALSE;
		}
	}

	return success;
}
