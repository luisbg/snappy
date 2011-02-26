# Copyright (C) 2011 Collabora Multimedia Ltd.
# <luis.debethencourt@collabora.co.uk>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
# USA

import clutter
import os
import gobject


class UI:
	def __init__ (self, video_texture, mediafile, width, height):
		self.video_width = width
		self.video_height = height
		self.video_texture = video_texture
		self.color1 = clutter.Color(73, 74, 77, 0xee);
		self.color2 = clutter.Color(0xcc, 0xcc, 0xcc, 0xff);
		self.black = clutter.Color(0, 0, 0, 0)
		self.controls_showing = False
		self.controls_timeout = 0
		self.seek_H = 20
		self.seek_W = 640

		self.stage = clutter.stage_get_default()
		self.stage.set_color(self.black)
		self.stage.set_size(self.video_width, self.video_height)
		filename = os.path.split(mediafile)[1]
		self.stage.set_title('dogme~ ' + filename)

		self.controls = clutter.Group()
		self.control_bg = clutter.Texture(filename="img/vid-panel.png")
		self.control_play = clutter.Texture(filename="img/media-actions-start.png")
		self.control_pause = clutter.Texture(filename="img/media-actions-pause.png")
		self.control_seek1 = clutter.Rectangle(color=self.color1)
		self.control_seek2 = clutter.Rectangle(color=self.color2)
		self.control_seekbar = clutter.Rectangle(color=self.color1)
		self.control_seekbar.set_opacity(0x99)

		self.control_label = clutter.Text()
		self.control_label.set_font_name("Sans Bold 24")
		if len(filename) > 30:
			filename = filename[:29]
		self.control_label.set_text(filename)

		self.controls.hide()
		self.controls.add(self.control_bg, \
						self.control_play, \
						self.control_pause, \
						self.control_seek1, \
						self.control_seek2, \
						self.control_seekbar, \
						self.control_label)
		self.controls.set_opacity (0xee)

		self.control_play.hide()
		self.control_play.set_position (30, 30)
		self.control_pause.set_position (30, 30)

		self.control_seek1.set_size(self.seek_W + 10, self.seek_H + 10)
		self.control_seek1.set_position (200, 100)
		self.control_seek2.set_size(self.seek_W, self.seek_H)
		self.control_seek2.set_position (205, 105)
		self.control_seekbar.set_size(0, self.seek_H / 5)
		self.control_seekbar.set_position (205, 105)
		self.control_label.set_position (200, 40)

		self.stage.add(self.video_texture, self.controls)
		width, height = self.stage.get_size()
		self.center_controls(width, height)

		self.stage.hide_cursor()
		self.controls.animate(clutter.AnimationMode(clutter.EASE_OUT_QUINT), \
							1000, "opacity", 0)

	def controls_timeout_cb (self, data):
		self.controls_timeout = 0
		self.show_controls (False)

		return False

	def show_controls (self, visibility):
		if (visibility == True and self.controls_showing == True):
			if self.controls_timeout == 0:
				self.controls_timeout = gobject.timeout_add (3000, \
											self.controls_timeout_cb, self)
			return
		if (visibility == True and self.controls_showing == False):
			self.controls_showing = True
			self.stage.show_cursor()
			self.controls.animate(
					clutter.AnimationMode(clutter.EASE_OUT_QUINT), \
					250, "opacity", 224)
			return
		if (visibility == False and self.controls_showing == True):
			self.controls_showing = False
			self.stage.hide_cursor()
			self.controls.animate(
					clutter.AnimationMode(clutter.EASE_OUT_QUINT), \
					250, "opacity", 0)
			return

	def center_controls (self, width, height):
		x = (width - self.controls.get_width()) / 2
		y = height - (height / 3)

		self.controls.set_position (x, y)

	def toggle_fullscreen (self, mode):
		if mode:
			self.stage.set_fullscreen(True)
		else:
			self.stage.set_fullscreen(False)

	def change_on_size (self, data):
		stage_width, stage_height = self.stage.get_size()
		new_width, new_height = stage_width, stage_height

		if self.video_height <= self.video_width:
			aratio = self.video_height / float(self.video_width)
			new_height = new_width * aratio
			center = (stage_height - new_height) / 2
			self.video_texture.set_position (0, center)
		else:
			aratio = self.video_width / float(self.video_height)
			new_width = new_height * aratio
			center = (stage_width - new_width) / 2
			self.video_texture.set_position (center, 0)

		self.video_texture.set_size (new_width, new_height)
		self.center_controls(stage_width, stage_height)
