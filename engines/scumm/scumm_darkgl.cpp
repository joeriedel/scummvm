#define FORBIDDEN_SYMBOL_EXCEPTION_rand
#include "common/debug.h"
#include "common/endian.h"
#include "scumm/imuse/imuse.h"
#include "scumm/imuse/sysex.h"
#include "audio/musicplugin.h"  /* for music manager */
#include "audio/mixer.h"
#include "audio/midiparser.h"

namespace Scumm {
	class IMuseInternal;
}

class DarkForcesiMuse {
public:
	enum Track {
		Stalk,
		Fight
	};

	void Init();
	void Play(const void *stalk, const void *fight);
	void Stop();
	void SetTransition(Track track);

	static void sysexHandler(Scumm::Player *, const byte *, uint16);

	static DarkForcesiMuse s_instance;

private:

	void sysex(Scumm::Player *, const byte *, uint16);

	volatile int m_toStalk[4];
	volatile int m_toFight[4];
	volatile uint m_loopTick;

	volatile Track m_from;
	volatile Track m_to;
	volatile int m_bridge;
	volatile bool m_ignoreHeaders;
	volatile bool m_gotHeaders;
	Scumm::IMuseInternal *m_iimuse;
	MidiDriver *m_driver;

};

DarkForcesiMuse DarkForcesiMuse::s_instance;
#define DARKGL_EXPOSE_IMUSE_PRIVATES
#include "scumm/imuse/imuse_internal.h"

#if defined(_WIN32)
#include "backends/platform/sdl/win32/win32.h"
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

byte *darkgl_soundbanks[8] = {0,0,0,0,0,0,0,0};

extern "C" DLLEXPORT void darkgl_scumm_init() {
	DarkForcesiMuse::s_instance.Init();
}

extern "C" DLLEXPORT void darkgl_scumm_midi_start(const void *stalk, const void  *fight) {
	DarkForcesiMuse::s_instance.Play(stalk, fight);
}

extern "C" DLLEXPORT void darkgl_scumm_midi_stop() {
	DarkForcesiMuse::s_instance.Stop();
}

extern "C" DLLEXPORT void darkgl_set_transition(int type) {
	if (type >= 0 && type <=1 )
		DarkForcesiMuse::s_instance.SetTransition((DarkForcesiMuse::Track)type);
}

void DarkForcesiMuse::Init() {
	g_system = new OSystem_Win32();
	assert(g_system);

	// Pre initialize the backend
	((OSystem_Win32 *)g_system)->init();

	g_system->initBackend();
	PluginManager::instance().init();
   	PluginManager::instance().loadAllPlugins(); // load plugins for cached plugin manager
	MusicManager::instance();

	m_driver = MidiDriver::createMidi(MidiDriver::detectDevice(MDT_MIDI));
	//m_driver->property(MidiDriver::PROP_SCUMM_OPL3, 1);
	//m_driver->property(MidiDriver::PROP_OLD_ADLIB, 1);

	m_iimuse = (Scumm::IMuseInternal *)Scumm::IMuse::create(g_system, m_driver, 0);
	//m_iimuse->property(Scumm::IMuse::PROP_GS, 1);
	m_iimuse->setMusicVolume(255);
	m_iimuse->addSysexHandler(/*IMUSE_SYSEX_ID*/ 0x7D, sysexHandler);
}

void DarkForcesiMuse::SetTransition(DarkForcesiMuse::Track track) {
	m_to = track;
}

void DarkForcesiMuse::Stop() {
	m_iimuse->stopAllSounds();
	g_system->delayMillis(25);
	m_from = m_to = Stalk;
}

void DarkForcesiMuse::Play(const void *stalk, const void *fight) {
	Stop();
	m_bridge = -1;
	m_from = m_to = Stalk;
	m_loopTick = 0;
	darkgl_soundbanks[0] = (byte*)stalk;
	darkgl_soundbanks[1] = (byte*)fight;

	// load transion beat offsets
	for (int i = 0; i < 4; ++i) {
		m_toStalk[0] = 0;
		m_toFight[0] = 0;
	}

	m_ignoreHeaders = false;
	m_gotHeaders = false;
	m_iimuse->setMusicVolume(0);
	m_iimuse->startSound(0);
	for (int i = 0; (i < 99) && !m_gotHeaders; ++i) {
		g_system->delayMillis(25);
	}
	m_iimuse->stopSound(0);
	g_system->delayMillis(25);
	m_gotHeaders = false;
	m_iimuse->startSound(1);
	for (int i = 0; (i < 99) && !m_gotHeaders; ++i) {
		g_system->delayMillis(25);
	}
	m_iimuse->stopSound(1);
	g_system->delayMillis(25);

	m_iimuse->setMusicVolume(255);
	m_ignoreHeaders = true;
	m_iimuse->startSound(0);
}

void DarkForcesiMuse::sysexHandler(Scumm::Player *player, const byte *msg, uint16 len) {
	s_instance.sysex(player, msg, len);
}

void DarkForcesiMuse::sysex(Scumm::Player *player, const byte *msg, uint16 len) {
	const byte *p = msg;
	byte code = *p++;
/*
from fight 10,20,0,0
handled sysEx 3: from fight 16,32 (32)

UNHANDLED sysEx 3: clear callback (6b)

sysEx 1: 0 10 14 31
from stalk 3,f,21,0
handled sysEx 3: from stalk 3,15,33 (33)

UNHANDLED sysEx 3: clear callback (6b)

sysEx 1: 0 10 14 30
UNHANDLED sysEx 3: from fight 16,32 (32)

UNHANDLED sysEx 3: clear callback (6b)

sysEx 1: 0 10 14 31
UNHANDLED sysEx 3: to B (42)
*/
	switch (code) {
	case 1:
		{
			if (len != 8) {
				debug("Unrecognized sysex 1");
				return;
			}

			byte id = *p++;

			byte buf[128];
			player->decode_sysex_bytes(p, buf, len - 2);

			if (m_ignoreHeaders) {
				if (id == 0) {
					int code = READ_BE_UINT16(buf);
					if (code = 0x1014) {
						bool jump = buf[2] == 0x30;

						if (jump) {
							if (m_loopTick > 0) {
								debug("looping track.");
								player->scan(0, 0, m_loopTick);
							}
						} else {
							m_loopTick = player->_parser->getTick();
						}
					}
				}
			}

			//debug("sysEx 1: (%d) %d %d", (int)id, (int)READ_BE_UINT16(buf), (int)READ_BE_UINT16(buf+2));
			debug("sysEx 1: %x %x %x %x", id, buf[0], buf[1], buf[2]);

//			player->scan(0, 0, 0);

			// Shut down a part. [Bug 1088045, comments]
			//Scumm::Part *part = player->getPart(p[0]);
			//if (part != NULL)
			//	part->uninit();
			/*char buf[64] = {0};
			for (int i=1;i<len;++i) {
				char z[32];
				sprintf(z, " %x", msg[i]);
				strcat(buf, z);
			}
			debug("sysEx 1: ?? %s", buf);*/
		} break;
		break;
	case 3:
		{
			if (player->_scanning)
				return;

			char buf[64];
			strncpy(buf, (const char*)p, len-1);
			buf[len-1] = 0;

			bool handled = false;

			if (m_ignoreHeaders) {

				if (!strcmp(buf, "eot")) {
					// end of track? looping?
					if (m_bridge == -1) {
						handled = true;
						debug("looping track.");
						player->scan(0, 0, m_loopTick);
					}
				}

			} else if (!m_gotHeaders) {
				if (strstr(buf, "from stalk")) {
					handled = true;
					sscanf(buf, "from stalk %d,%d,%d,%d", 
						&m_toFight[0], &m_toFight[1], 
						&m_toFight[2], &m_toFight[3]
					);
					debug("from stalk %x,%x,%x,%x", m_toFight[0], m_toFight[1], m_toFight[2], m_toFight[3]);

					m_gotHeaders = true;
				} else if (strstr(buf, "from fight")) {
					handled = true;
					sscanf(buf, "from fight %d,%d,%d,%d", 
						&m_toStalk[0], &m_toStalk[1], 
						&m_toStalk[2], &m_toStalk[3]
					);
					m_gotHeaders = true;
					debug("from fight %x,%x,%x,%x", m_toStalk[0], m_toStalk[1], m_toStalk[2], m_toStalk[3]);
				}
			}

			if (handled) {
				debug("handled sysEx 3: %s (%x)\n", buf, (int)*(msg + len - 1));
			} else {
				debug("UNHANDLED sysEx 3: %s (%x)\n", buf, (int)*(msg + len - 1));
			}

		} break;

	default:
		debug("unhandled sysEx %d", code);
	}
}

//	darkgl_imuse->stopAllSounds();
//	darkgl_transition = 0;
//	darkgl_currentsound = 0;
//	darkgl_soundbanks[0] = (byte*)stalk;
//	darkgl_soundbanks[1] = (byte*)fight;
//	
//	// load transition points.
//
//	for (int i = 0; i < 2; ++i) {
//		for (int k = 0; k < 4; ++k) {
//			darkgl_transitions[i][k] = -1;
//		}
//	}
//
//	darkgl_gotfrom = 0;
//	darkgl_disablesysex = 1;
//	darkgl_imuse->setMusicVolume(0);
//
//	darkgl_imuse->startSound(0);
//	for (int i = 0; (i < 100) && !darkgl_gotfrom; ++i) {
//		g_system->delayMillis(25);
//	}
//	darkgl_imuse->stopSound(0);
//	g_system->delayMillis(25);
//
//	darkgl_gotfrom = 0;
//	darkgl_imuse->startSound(1);
//	for (int i = 0; (i < 100) && !darkgl_gotfrom; ++i) {
//		g_system->delayMillis(25);
//	}
//	darkgl_imuse->stopSound(1);
//	g_system->delayMillis(25);
//	darkgl_disablesysex = 0;
//	darkgl_imuse->setMusicVolume(255);
//
//	for (int i = 0; i < 2; ++i) {
//		if (darkgl_transitions[i][0] < 0) { // to A?
//			darkgl_transitions[i][0] = 0;
//		}
//		if (darkgl_transitions[i][1] < 0) { // to B?
//			darkgl_transitions[i][1] = darkgl_transitions[i][0];
//		}
//		if (darkgl_transitions[i][2] < 0) { // to C? -> B
//			darkgl_transitions[i][2] = darkgl_transitions[i][1];
//		}
//		if (darkgl_transitions[i][3] < 0) { // to D? -> C
//			darkgl_transitions[i][3] = darkgl_transitions[i][2];
//		}
//	}
//
//	darkgl_imuse->startSound(0);
//	darkgl_suppress_startnew = 0;
//}
//
//extern "C" DLLEXPORT void darkgl_scumm_midi_stop() {
//	darkgl_imuse->stopAllSounds();
//	g_system->delayMillis(25);
//}
//
//extern "C" DLLEXPORT void darkgl_set_transition(int type) {
//	darkgl_transition = type;
//}
//
//void sysexHandler_darkgl(Scumm::Player *player, const byte *msg, uint16 len) {
//	const byte *p = msg;
//	byte code = *p++;
//
//	switch (code) {
//	case 1:
//		{
//			// Shut down a part. [Bug 1088045, comments]
//			Scumm::Part *part = player->getPart(p[0]);
//			if (part != NULL)
//				part->uninit();
//		} break;
//		break;
//	case 3:
//		{
//			if (player->_scanning)
//				return;
//
//			char buf[64];
//			strncpy(buf, (const char*)p, len-1);
//			buf[len-1] = 0;
//			debug("sysEx 3: %s", buf);
//
//			if (darkgl_disablesysex) {
//				if (strstr(buf, "from stalk")) {
//					sscanf(buf, "from stalk %d,%d,%d,%d",
//						&darkgl_transitions[1][0], &darkgl_transitions[1][1],
//						&darkgl_transitions[1][2], &darkgl_transitions[1][3]);
//					darkgl_gotfrom = 1;
//				} else if(strstr(buf, "from fight")) {
//					sscanf(buf, "from fight %d,%d,%d,%d",
//						&darkgl_transitions[0][0], &darkgl_transitions[0][1],
//						&darkgl_transitions[0][2], &darkgl_transitions[0][3]);
//					darkgl_gotfrom = 1;
//				}
//			} else {
//				if (strstr(buf, "start new")) {
//					if (darkgl_suppress_startnew) {
//						darkgl_suppress_startnew = 0;
//					} else {
//						darkgl_suppress_startnew = 1;
//
//						darkgl_imuse->stopAllSounds();
//						darkgl_imuse->startSound(darkgl_currentsound);
//						Scumm::Player *newPlayer = darkgl_imuse->findActivePlayer(darkgl_currentsound);
//
//						int track = nextTrack(darkgl_next_track);
//						int note = 0;
//						if (track > 0) {
//							note = darkgl_transitions[darkgl_currentsound][rand()%4];
//						}
//
//						newPlayer->scan(track, note, 0);
//						debug("music change to %d track %d note %d", darkgl_currentsound, track, note);
//					}
//				} else  if (darkgl_transition != darkgl_currentsound) {
//					int target = darkgl_transition;
//					int note = -1;
//					int track = -1;
//					if (strstr(buf, "to A")) {
//						track = 0;
//						note = darkgl_transitions[target][0];
//					} else if (strstr(buf, "to B")) {
//						track = 1;
//						note = darkgl_transitions[target][1];
//					} else if (strstr(buf, "to C")) {
//						track = 2;
//						note = darkgl_transitions[target][2];
//					} else if (strstr(buf, "to D")) {
//						track = 3;
//						note = darkgl_transitions[target][3];
//					} else if (strstr(buf, "trans stalk") && target == 0) {
//						track = nextTrack(darkgl_next_track);
//						sscanf(buf, "trans stalk %d", &note);
//					} else if (strstr(buf, "trans fight") && target == 1) {
//						track = nextTrack(darkgl_next_track);
//						sscanf(buf, "trans stalk %d", &note);
//					}
//
//					if (note != -1) {
//						darkgl_next_track = track;
//						darkgl_currentsound = target;
//						darkgl_imuse->stopAllSounds();
//						darkgl_imuse->startSound(target);
//						Scumm::Player *newPlayer = darkgl_imuse->findActivePlayer(target);
//						newPlayer->scan(track, note, 0);
//						debug("music change to %d track %d note %d", target, track, note);
//					}
//				}
//			}
//
//		} break;
//
//	default:
//		debug("unhandled sysEx %d", code);
//	}
//
//	//Scumm::sysexHandler_Scumm(player, msg, len);
//}
