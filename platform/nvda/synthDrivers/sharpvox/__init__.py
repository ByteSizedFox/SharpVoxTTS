import ctypes
import os
import queue
import threading

import addonHandler
import config
import nvwave
import synthDriverHandler
from logHandler import log
from speech.commands import IndexCommand

addonHandler.initTranslation()

AUDIO_CB = ctypes.CFUNCTYPE(
	None, ctypes.POINTER(ctypes.c_int16), ctypes.c_int32, ctypes.c_void_p)

_MIN_RATE = 40
_MAX_RATE = 600
_MIN_PITCH = 40
_MAX_PITCH = 800


def _load_dll():
	dll_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sharpvox.dll")
	return ctypes.CDLL(dll_path)


class SynthDriver(synthDriverHandler.SynthDriver):
	name = "sharpvox"
	description = "SharpVox TTS"

	supportedCommands = {IndexCommand}
	supportedNotifications = {
		synthDriverHandler.synthIndexReached,
		synthDriverHandler.synthDoneSpeaking,
	}
	supportedSettings = (
		synthDriverHandler.SynthDriver.RateSetting(),
		synthDriverHandler.SynthDriver.PitchSetting(),
		synthDriverHandler.SynthDriver.VolumeSetting(),
	)

	@classmethod
	def check(cls):
		try:
			dll = _load_dll()
			dll.sharpvox_create.restype = ctypes.c_void_p
			dll.sharpvox_destroy.argtypes = [ctypes.c_void_p]
			h = dll.sharpvox_create()
			if not h:
				log.error("SharpVox check(): sharpvox_create() returned NULL")
				return False
			dll.sharpvox_destroy(h)
			return True
		except Exception:
			log.error("SharpVox check(): failed", exc_info=True)
			return False

	def __init__(self):
		super().__init__()
		self._dll = _load_dll()
		self._dll.sharpvox_create.restype = ctypes.c_void_p
		self._dll.sharpvox_destroy.argtypes = [ctypes.c_void_p]
		self._dll.sharpvox_speak.argtypes = [
			ctypes.c_void_p, ctypes.c_char_p, AUDIO_CB, ctypes.c_void_p,
		]
		self._dll.sharpvox_stop.argtypes = [ctypes.c_void_p]
		self._dll.sharpvox_is_speaking.argtypes = [ctypes.c_void_p]
		self._dll.sharpvox_is_speaking.restype = ctypes.c_int
		self._dll.sharpvox_set_rate.argtypes = [ctypes.c_void_p, ctypes.c_int32]
		self._dll.sharpvox_get_rate.argtypes = [ctypes.c_void_p]
		self._dll.sharpvox_get_rate.restype = ctypes.c_int32
		self._dll.sharpvox_set_pitch.argtypes = [ctypes.c_void_p, ctypes.c_int32]
		self._dll.sharpvox_get_pitch.argtypes = [ctypes.c_void_p]
		self._dll.sharpvox_get_pitch.restype = ctypes.c_int32
		self._dll.sharpvox_set_volume.argtypes = [ctypes.c_void_p, ctypes.c_float]
		self._dll.sharpvox_get_volume.argtypes = [ctypes.c_void_p]
		self._dll.sharpvox_get_volume.restype = ctypes.c_float
		self._dll.sharpvox_get_sample_rate.argtypes = [ctypes.c_void_p]
		self._dll.sharpvox_get_sample_rate.restype = ctypes.c_int32

		self._handle = self._dll.sharpvox_create()
		if not self._handle:
			raise RuntimeError("sharpvox_create failed")

		sr = self._dll.sharpvox_get_sample_rate(self._handle)
		self._player = nvwave.WavePlayer(
			channels=1, samplesPerSec=sr, bitsPerSample=16,
			outputDevice=config.conf["audio"]["outputDevice"])

		self._rate = 50
		self._pitch = 50
		self._volume = 100
		self._applyRate()
		self._applyPitch()
		self._applyVolume()

		self._stopSignal = object()
		self._cancelled = False
		self._drainDone = threading.Event()
		self._drainDone.set()
		self._queue = queue.Queue()
		self._thread = threading.Thread(target=self._run, daemon=True)
		self._thread.start()

	def _run(self):
		while True:
			item = self._queue.get()
			if item is self._stopSignal:
				return
			try:
				self._speakNow(item)
			except Exception:
				log.error("SharpVox: speak failed", exc_info=True)
			synthDriverHandler.synthDoneSpeaking.notify(synth=self)

	def _speakNow(self, text):
		if not text:
			return
		self._drainDone.clear()

		def on_audio(samples, count, _ud):
			if self._cancelled:
				return
			self._player.feed(ctypes.string_at(samples, count * 2))

		cb = AUDIO_CB(on_audio)
		self._dll.sharpvox_speak(self._handle, text.encode("utf-8"), cb, None)
		if not self._cancelled:
			self._player.idle()
		self._drainDone.set()

	def speak(self, speechSequence):
		text_parts = []
		indexes = []
		for item in speechSequence:
			if isinstance(item, str):
				text_parts.append(item)
			elif isinstance(item, IndexCommand):
				indexes.append(item.index)
		text = "".join(text_parts)
		if text:
			self._queue.put(text)
		for idx in indexes:
			synthDriverHandler.synthIndexReached.notify(synth=self, index=idx)

	def cancel(self):
		try:
			while True:
				self._queue.get_nowait()
		except queue.Empty:
			pass
		self._cancelled = True
		self._dll.sharpvox_stop(self._handle)
		self._drainDone.wait()
		self._player.stop()
		self._cancelled = False

	def pause(self, switch):
		self._player.pause(switch)

	def terminate(self):
		self.cancel()
		self._queue.put(self._stopSignal)
		self._thread.join()
		self._player.close()
		self._dll.sharpvox_destroy(self._handle)
		self._handle = None

	def _get_rate(self):
		return self._rate

	def _set_rate(self, v):
		self._rate = max(0, min(100, int(v)))
		self._applyRate()

	def _applyRate(self):
		frac = self._rate / 100.0
		wpm = int(round(_MIN_RATE + frac * (_MAX_RATE - _MIN_RATE)))
		self._dll.sharpvox_set_rate(self._handle, wpm)

	def _get_pitch(self):
		return self._pitch

	def _set_pitch(self, v):
		self._pitch = max(0, min(100, int(v)))
		self._applyPitch()

	def _applyPitch(self):
		frac = self._pitch / 100.0
		hz = int(round(_MIN_PITCH + frac * (_MAX_PITCH - _MIN_PITCH)))
		self._dll.sharpvox_set_pitch(self._handle, hz)

	def _get_volume(self):
		return self._volume

	def _set_volume(self, v):
		self._volume = max(0, min(100, int(v)))
		self._applyVolume()

	def _applyVolume(self):
		self._dll.sharpvox_set_volume(self._handle, self._volume / 100.0)
