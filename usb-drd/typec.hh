/*
 * typec.hh — board-specific USB Type-C port control
 *
 * The USB stack only needs three things from the Type-C port:
 *   - Device mode: Rd presented on CC so the far-end host supplies VBUS
 *   - Host mode: 5V driven onto VBUS (and ideally Rp presented on CC)
 *   - VBUS presence, if the board can sense it
 *
 * One implementation is compiled per board (see BOARD in the Makefile):
 *   typec_ev1.cc      — EV1 kit: TCPP03-M20 on I2C1 gates VBUS; its
 *                       dead-battery Rd covers device mode
 *   typec_devboard.cc — custom board: CC1/CC2 wired directly to UCPD1,
 *                       VBUS switch on PB4 (adaptor board), sense on PH3
 */

#pragma once

namespace TypeC
{

// One-time hardware setup. Call once at startup, before entering a mode.
int init();

// Present Rp on CC (if the board supports it) and drive 5V onto VBUS.
int enter_host_mode();
int exit_host_mode();

// Present Rd on CC so the far-end host supplies VBUS.
int enter_device_mode();
int exit_device_mode();

// True if 5V is present on the jack's VBUS pin (best effort per board).
bool vbus_present();

} // namespace TypeC
