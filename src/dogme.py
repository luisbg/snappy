#!/usr/bin/env python

# Dogme video player.

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

""" dogme video player's main file """

import gobject
gobject.threads_init()
import gst
import clutter
import cluttergst
import os, optparse

from ui import UI

class Dogme:
	def toggle_playing_state(self):
		if self.playing:
			self.player.set_state(gst.STATE_PAUSED)
			self.playing = False
			self.ui.control_pause.hide()
			self.ui.control_play.show()

		else:
			self.player.set_state(gst.STATE_PLAYING)
			self.playing = True
			self.ui.control_play.hide()
			self.ui.control_pause.show()

	def toggle_fullscreen(self):
		new_state = not self.fullscreen
		self.ui.toggle_fullscreen(new_state)
		self.fullscreen = new_state

	def event (self, stage, event):
		# Key pressed event.
		if event.type == clutter.KEY_PRESS:
			#print "key pressed ", event.keyval, "."
			if event.keyval == 65363:
				pos = self.player.query_position(gst.FORMAT_TIME)[0]
				pos += 10 * gst.SECOND
				self.player.seek_simple(gst.FORMAT_TIME, gst.SEEK_FLAG_FLUSH, \
										pos)
				return True
			if event.keyval == 65361:
				pos = self.player.query_position(gst.FORMAT_TIME)[0]
				if pos > ( 10 * gst.SECOND):
					pos -= 10 * gst.SECOND
				else:
					pos = 0
				self.player.seek_simple(gst.FORMAT_TIME, gst.SEEK_FLAG_FLUSH, \
										pos)
				return True
			if event.keyval == 65307 or chr(event.keyval) == 'q':
				clutter.main_quit()
			elif chr(event.keyval) == 'f':
				self.toggle_fullscreen()
				return True
			elif chr(event.keyval) == ' ':
				self.toggle_playing_state()
				return True
			elif chr(event.keyval) == '9':
				volume = self.player.get_property("volume")
				if volume < 1.0:
					volume += 0.1
					self.player.set_property("volume", volume)
				return True
			elif chr(event.keyval) == '0':
				volume = self.player.get_property("volume")
				if volume > 0.0:
					volume -= 0.1
					self.player.set_property("volume", volume)
				return True
			elif chr(event.keyval) == '8':
				mute = self.player.get_property("mute")
				self.player.set_property("mute", not mute)
				return True

		# Mouse click event.
		elif event.type == clutter.BUTTON_PRESS:
			actor = self.ui.stage.get_actor_at_pos (clutter.PICK_ALL, \
							int(event.x), int(event.y))
			if (actor == self.ui.control_pause or actor == self.ui.control_play):
				self.toggle_playing_state()
			elif (actor == self.ui.control_seek1 or \
					actor == self.ui.control_seek2 or \
					actor == self.ui.control_seekbar):
				x, y = self.ui.control_seekbar.get_transformed_position()
				dist = event.x - x
				if self.duration == 0:
					self.duration = self.player.query_duration(gst.FORMAT_TIME)[0]
				progress = self.duration * (dist / self.ui.seek_W)
				self.player.seek_simple(gst.FORMAT_TIME, gst.SEEK_FLAG_FLUSH, \
										progress)
				self.ui.control_seekbar.set_size (dist, self.ui.seek_H)
			return True

		# Cursor motion event
		elif event.type == clutter.MOTION:
			self.ui.show_controls (True)
			return True

	def progress_update (self, data):
		pos = self.player.query_position(gst.FORMAT_TIME)[0]
		if self.duration == 0:
				self.duration = self.player.query_duration(gst.FORMAT_TIME)[0]
		pos = float(pos) / self.duration
		self.ui.control_seekbar.set_size (pos * self.ui.seek_W, self.ui.seek_H)

		return True

	def load_ui (self):
		if not self.ui_loaded:
			self.ui = UI (self.texture, self.mediafile, self.width, self.height)
			self.ui.stage.connect('destroy', clutter.main_quit)
			self.ui.stage.connect('event', self.event)
			self.ui.stage.connect('fullscreen', self.ui.change_on_size)
			self.ui.stage.connect('unfullscreen', self.ui.change_on_size)

			self.playing = True
			self.player.set_state(gst.STATE_PLAYING)

			self.ui.stage.show_all()

			if self.fullscreen:
				self.ui.stage.set_fullscreen(True)
			self.ui_loaded = True

	def __init__ (self, args, options):
		self.mediafile = args[0]
		self.fullscreen = options.fullscreen
		self.duration = 0
		self.width =  0
		self.height = 0
		self.ui_loaded = False

		def bus_handler(unused_bus, message):
			if message.type == gst.MESSAGE_STATE_CHANGED:
				old, new, pending = message.parse_state_changed()
				if new == gst.STATE_PAUSED:
					if self.width == 0:
						pad = self.sink.get_pad("sink")
						caps = pad.get_negotiated_caps()
						if caps:
							s = caps.get_structure(0)
							if s.has_key('width'):
								self.width = s['width']
							if s.has_key('height'):
								self.height = s['height']
							self.load_ui()
			if message.type == gst.MESSAGE_ERROR:
				pass
			return gst.BUS_PASS

		self.player = gst.element_factory_make("playbin2", "player")
		self.texture = clutter.Texture()
		self.sink = cluttergst.VideoSink(self.texture)
		self.player.set_property("video-sink", self.sink)
		self.bus = self.player.get_bus()
		self.bus.add_signal_watch()
		self.bus.connect("message", bus_handler)

		self.player.set_property("uri", "file://" + self.mediafile)
		self.player.set_state(gst.STATE_PAUSED)

		gobject.timeout_add(2000, self.progress_update, self)

		clutter.main()


if __name__ == "__main__":
	parser = optparse.OptionParser("usage: %prog [options] arg")
	parser.add_option("-f", "--fullscreen", action="store_true", dest="fullscreen", help="Start in FullScreen mode", default=False)
	(options, args) = parser.parse_args()
	if len(args) < 1:
		parser.error("You need to specify a media file.")

	print "Playing... %r" % args[0]

	dogme = Dogme (args, options)
