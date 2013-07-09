
#include "common/config-manager.h"
#include "common/debug-channels.h"
#include "common/md5.h"
#include "common/events.h"
#include "common/system.h"
#include "common/translation.h"

#include "engines/util.h"

#include "gui/message.h"
#include "gui/gui-manager.h"

#include "graphics/cursorman.h"

#include "scumm/akos.h"
#include "scumm/charset.h"
#include "scumm/costume.h"
#include "scumm/debugger.h"
#include "scumm/dialogs.h"
#include "scumm/file.h"
#include "scumm/file_nes.h"
#include "scumm/imuse/imuse.h"
#include "scumm/imuse_digi/dimuse.h"
#include "scumm/smush/smush_mixer.h"
#include "scumm/smush/smush_player.h"
#include "scumm/player_towns.h"
#include "scumm/insane/insane.h"
#include "scumm/he/animation_he.h"
#include "scumm/he/intern_he.h"
#include "scumm/he/logic_he.h"
#include "scumm/he/sound_he.h"
#include "scumm/object.h"
#include "scumm/player_nes.h"
#include "scumm/player_sid.h"
#include "scumm/player_pce.h"
#include "scumm/player_apple2.h"
#include "scumm/player_v1.h"
#include "scumm/player_v2.h"
#include "scumm/player_v2cms.h"
#include "scumm/player_v2a.h"
#include "scumm/player_v3a.h"
#include "scumm/player_v3m.h"
#include "scumm/player_v4a.h"
#include "scumm/player_v5m.h"
#include "scumm/resource.h"
#include "scumm/he/resource_he.h"
#include "scumm/scumm_v0.h"
#include "scumm/scumm_v8.h"
#include "scumm/sound.h"
#include "scumm/imuse/sysex.h"
#include "scumm/he/sprite_he.h"
#include "scumm/he/cup_player_he.h"
#include "scumm/util.h"
#include "scumm/verbs.h"
#include "scumm/imuse/pcspk.h"
#include "scumm/imuse/mac_m68k.h"
#include "base/plugins.h"
#include "backends/audiocd/audiocd.h"
#include "audio/musicplugin.h"  /* for music manager */

#include "audio/mixer.h"

#if defined(_WIN32)
#include "backends/platform/sdl/win32/win32.h"
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

typedef void* (*MIDIHOOK) (const char *args);
MIDIHOOK darkgl_MIDIHOOK = 0;
Scumm::IMuse *darkgl_imuse = 0;
byte *darkgl_soundbanks[8] = {0,0,0,0,0,0,0,0};

extern "C" DLLEXPORT void darkgl_scumm_init() {
	g_system = new OSystem_Win32();
	assert(g_system);

	// Pre initialize the backend
	((OSystem_Win32 *)g_system)->init();

	g_system->initBackend();
	PluginManager::instance().init();
   	PluginManager::instance().loadAllPlugins(); // load plugins for cached plugin manager
	MusicManager::instance();

	MidiDriver *nativeMidiDriver = MidiDriver::createMidi(MidiDriver::detectDevice(MDT_MIDI));
	MidiDriver *adlibMidiDriver = MidiDriver::createMidi(MidiDriver::detectDevice(MDT_ADLIB));

	darkgl_imuse = Scumm::IMuse::create(g_system, nativeMidiDriver, adlibMidiDriver);
	darkgl_imuse->property(Scumm::IMuse::PROP_NATIVE_MT32, false);
	darkgl_imuse->setMusicVolume(255);
}

extern "C" DLLEXPORT void darkgl_scumm_set_midi_hook(MIDIHOOK hook) {
	darkgl_MIDIHOOK = hook;
}

extern "C" DLLEXPORT void darkgl_scumm_midi_start(const void *sound) {
	darkgl_soundbanks[0] = (byte*)sound;
	darkgl_imuse->startSound(0);
}

extern "C" DLLEXPORT void darkgl_scumm_midi_stop() {
	darkgl_imuse->stopAllSounds();
}
