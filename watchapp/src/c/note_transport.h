#pragma once

#include <pebble.h>

/*
 * @brief Owns the watch's side of the "send a dictated note to the phone,
 *			wait for it to confirm the note was saved" conversation. This is the
 *			only place that talks AppMessage -- everything else just calls
 *			note_transport_send_note() and note_transport_is_busy().
 *
 *			The phone (PebbleKit JS, see ../pkjs/index.js) relays the note text
 *			to an n8n webhook and sends a reply back with the result. See
 *			../../../PLAN.md for the full end-to-end design.
 */

/*
 * @brief Registers the AppMessage handlers this module needs. Call once,
 *			during app init, before note_transport_send_note() is ever used.
 */
void	note_transport_init(void);

/*
 * @brief Cancels any pending timeout timer. Call once, during app deinit.
 */
void	note_transport_deinit(void);

/*
 * @brief Reports whether a note is currently being sent (i.e. we're waiting
 *			to hear back from the phone). Callers use this to avoid starting a
 *			second dictation while the first one is still in flight.
 * @return true if a send is in progress, false if it's safe to start a new
 *			one.
 */
bool	note_transport_is_busy(void);

/*
 * @brief Sends dictated text to the phone over AppMessage and starts waiting
 *			for its reply. Shows "Sending..." immediately (see
 *			status_display_show_sending()), and eventually shows a result (see
 *			status_display_show_result()) once the phone replies, the send
 *			fails outright, or a reply never arrives within the timeout.
 * @param transcription The dictated text to send. Only needs to stay valid
 *			for the duration of this call -- it's copied into the outgoing
 *			AppMessage immediately.
 */
void	note_transport_send_note(char *transcription);
