#pragma once

#include <pebble.h>

/*
 * @brief Owns the watch's DictationSession -- the native "listen and
 *			transcribe" flow the OS provides. This is the only file that
 *			touches the Dictation API directly; the SELECT button and the mic
 *			icon (on touch platforms) both just call dictation_handler_start().
 */

/*
 * @brief Creates the dictation session. Call once, during app init, before
 *			dictation_handler_start() is ever used.
 */
void	dictation_handler_init(void);

/*
 * @brief Destroys the dictation session. Call once, during app deinit.
 */
void	dictation_handler_deinit(void);

/*
 * @brief Starts listening for a dictated note, if the app is currently idle.
 *			No-ops silently if a note is already being sent/awaited (see
 *			note_transport_is_busy()) -- callers don't need to check that
 *			themselves first.
 */
void	dictation_handler_start(void);
