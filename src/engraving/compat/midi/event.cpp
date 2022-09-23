/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "event.h"

#include "libmscore/note.h"
#include "libmscore/sig.h"

namespace mu::engraving {
//---------------------------------------------------------
//   Event::Event
//---------------------------------------------------------

Event::Event()
{
    _type            = 0;
    _ontime          = -1;
    _noquantOntime   = 0;
    _noquantDuration = 0;
    _channel         = 0;
    _a               = 0;
    _b               = 0;
    _duration        = 0;
    _tpc             = 0;
    _voice           = 0;
    _edata           = 0;
    _len             = 0;
    _metaType        = 0;
    _note            = 0;
    _tuning          = 0.0;
}

Event::Event(int t)
{
    _type            = t;
    _ontime          = -1;
    _noquantOntime   = 0;
    _noquantDuration = 0;
    _channel         = 0;
    _a               = 0;
    _b               = 0;
    _duration        = 0;
    _tpc             = 0;
    _voice           = 0;
    _edata           = 0;
    _len             = 0;
    _metaType        = 0;
    _note            = 0;
    _tuning          = 0.0;
}

Event::Event(const Event& e)
    : PlayEvent(e)
{
    _type       = e._type;
    _ontime     = e._ontime;
    _noquantOntime   = e._noquantOntime;
    _noquantDuration = e._noquantDuration;
    _channel    = e._channel;
    _a          = e._a;
    _b          = e._b;
    _duration   = e._duration;
    _tpc        = e._tpc;
    _voice      = e._voice;
    _notes      = e._notes;
    if (e._edata) {
        _edata = new unsigned char[e._len + 1];      // don’t forget trailing zero
        memcpy(_edata, e._edata, e._len + 1);
    } else {
        _edata = 0;
    }
    _len        = e._len;
    _metaType   = e._metaType;
    _note       = e._note;
    _tuning     = e._tuning;
}

Event::~Event()
{
    delete[] _edata;
}

//---------------------------------------------------------
//   NPlayEvent::NPlayEvent (beatType2metronomeEvent)
//---------------------------------------------------------

NPlayEvent::NPlayEvent(BeatType beatType)
{
    setType(ME_TICK2);
    setVelo(127);
    switch (beatType) {
    case BeatType::DOWNBEAT:
        setType(ME_TICK1);
        break;
    case BeatType::SIMPLE_STRESSED:
    case BeatType::COMPOUND_STRESSED:
        // use defaults
        break;
    case BeatType::SIMPLE_UNSTRESSED:
    case BeatType::COMPOUND_UNSTRESSED:
        setVelo(80);
        break;
    case BeatType::COMPOUND_SUBBEAT:
        setVelo(25);
        break;
    case BeatType::SUBBEAT:
        setVelo(15);
        break;
    }
}

//---------------------------------------------------------
//   isMuted
//---------------------------------------------------------

bool NPlayEvent::isMuted() const
{
    //! TODO

//    const Note* n = note();
//    if (n) {
//        MasterScore* cs = n->masterScore();
//        Staff* staff = n->staff();
//        Instrument* instr = staff->part()->instrument(n->tick());
//        const Channel* a = instr->playbackChannel(n->subchannel(), cs);
//        return a->mute() || a->soloMute() || !staff->playbackVoice(n->voice());
//    }

//    const Harmony* h = harmony();
//    if (h) {
//        const Channel* hCh = h->part()->harmonyChannel();
//        if (hCh) { //if there is a harmony channel
//            const Channel* pCh = h->masterScore()->playbackChannel(hCh);
//            return pCh->mute() || pCh->soloMute();
//        }
//    }

    return false;
}

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void Event::dump() const
{
    printf("event ");
    switch (_type) {
    case ME_NOTEON:     printf("noteon    ");
        break;
    case ME_CONTROLLER: printf("controller");
        break;
    case ME_PROGRAM:    printf("program   ");
        break;
    default:            printf("0x%02x    ", _type);
        break;
    }
    printf(" 0x%02x 0x%02x\n", _a, _b);
}

bool Event::operator==(const Event&) const
{
    return false;             // TODO
}

//---------------------------------------------------------
// insert
//---------------------------------------------------------

void EventList::insert(const Event& e)
{
    int ontime = e.ontime();
    if (!empty() && back().ontime() > ontime) {
        for (auto i = begin(); i != end(); ++i) {
            if (i->ontime() > ontime) {
                std::vector<Event>::insert(i, e);
                return;
            }
        }
    }
    push_back(e);
}

//---------------------------------------------------------
//   class EventMap::fixupMIDI
//---------------------------------------------------------

void EventMap::fixupMIDI()
{
    /* track info for each of the 128 possible MIDI notes */
    struct channelInfo {
        /* which event the first ME_NOTEON came from */
        NPlayEvent* event[128];
        /* how often is the note on right now? */
        unsigned short nowPlaying[128];
    };

    /* track info for each channel (on the heap, 0-initialised) */
    struct channelInfo* info = (struct channelInfo*)calloc(_highestChannel + 1, sizeof(struct channelInfo));

    auto it = begin();
    while (it != end()) {
        NPlayEvent& event = it->second;
        /* ME_NOTEOFF is never emitted, no need to check for it */
        if (event.type() == ME_NOTEON && !event.isMuted()) {
            unsigned short np = info[event.channel()].nowPlaying[event.pitch()];
            if (event.velo() == 0) {
                /* already off (should not happen) or still playing? */
                if (np == 0 || --np > 0) {
                    event.setDiscard(1);
                } else {
                    /* hoist NOTEOFF to same track as NOTEON */
                    event.setOriginatingStaff(info[event.channel()].event[event.pitch()]->getOriginatingStaff());
                }
            } else {
                if (++np > 1) {
                    /* restrike, possibly on different track */
                    event.setDiscard(info[event.channel()].event[event.pitch()]->getOriginatingStaff() + 1);
                }
                info[event.channel()].event[event.pitch()] = &event;
            }
            info[event.channel()].nowPlaying[event.pitch()] = np;
        }

        ++it;
    }

    free((void*)info);
}
}
