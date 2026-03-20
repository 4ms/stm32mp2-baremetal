#pragma once
#include <cstdint>

/* MIDI status byte to human-readable name */
static const char *midi_status_name(uint8_t status)
{
	switch (status & 0xF0) {
		case 0x80:
			return "NoteOff";
		case 0x90:
			return "NoteOn ";
		case 0xA0:
			return "AfterT ";
		case 0xB0:
			return "CC     ";
		case 0xC0:
			return "PrgChg ";
		case 0xD0:
			return "ChanPr ";
		case 0xE0:
			return "PBend  ";
		case 0xF0:
			return "System ";
		default:
			return "???    ";
	}
}
