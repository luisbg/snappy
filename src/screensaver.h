/*
 * snappy - 0.1 beta
 *
 * Copyright (C) 2011 Collabora Multimedia Ltd.
 * <sebastian.droege@collabora.co.uk>
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

#ifndef __SCREENSAVER_H__
#define __SCREENSAVER_H__

#include <glib.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

typedef struct _ScreenSaver ScreenSaver;

ScreenSaver * screensaver_new (ClutterStage * stage);
void screensaver_enable (ScreenSaver * screensaver, gboolean enable);
void screensaver_free (ScreenSaver * screensaver);

G_END_DECLS

#endif /* __SCREENSAVER_H__ */
