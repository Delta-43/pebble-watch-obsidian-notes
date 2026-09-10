#pragma once

#include <pebble.h>

/*
 * @brief Owns the single text layer that shows the app's current state to the
 *			user: the idle prompt ("Press SELECT to dictate a note"), "Sending...",
 *			or a final result ("Saved!", a failure reason, etc). Nothing outside
 *			this file ever touches the text layer directly -- every other module
 *			goes through the functions below instead.
 *
 *			A result shown via status_display_show_result() automatically reverts
 *			back to the idle prompt after a few seconds, so the user doesn't have
 *			to do anything to get back to a "ready to dictate" screen.
 */

/*
 * @brief Creates the text layer and shows the idle prompt immediately. Must be
 *			called once, from the window's load handler, before any other
 *			status_display function is used.
 * @param parent_layer The layer to attach the text layer to (normally the
 *			window's root layer).
 * @param frame Where to position and size the text layer.
 */
void	status_display_init(Layer *parent_layer, GRect frame);

/*
 * @brief Destroys the text layer and cancels any pending revert-to-idle timer.
 *			Call once, from the window's unload handler.
 */
void	status_display_deinit(void);

/*
 * @brief Shows "Sending..." -- used while a note is on its way to the phone
 *			and we're waiting to hear back whether it was saved.
 */
void	status_display_show_sending(void);

/*
 * @brief Puts the idle prompt back on screen immediately (not via the
 *			auto-revert timer), and cancels that timer if one happened to be
 *			pending. Used when dictation is cancelled/aborted before ever
 *			reaching note_transport -- there is no "result" to show, so this
 *			just makes sure the screen doesn't keep showing a stale result
 *			from a previous attempt.
 */
void	status_display_show_idle(void);

/*
 * @brief Shows a final result (success or failure) and starts a timer that
 *			automatically reverts the screen back to the idle prompt a few
 *			seconds later, so the user always ends up back at a "ready to
 *			dictate" screen without having to press anything.
 * @param text The result text to show, e.g. "Saved!" or "Failed to save
 *			note". May contain a newline for a second line.
 */
void	status_display_show_result(const char *text);

/*
 * @brief Cancels the auto-revert-to-idle timer started by
 *			status_display_show_result(), if one is currently pending. Safe to
 *			call even if no timer is pending.
 *
 *			Why this exists: a result screen is considered "idle" the moment
 *			it's shown (see note_transport_is_busy()), so the user can
 *			immediately retry (press SELECT / tap the mic icon again) without
 *			waiting for the revert timer to fire. If they do retry quickly,
 *			whoever starts the new dictation attempt must call this first --
 *			otherwise the old timer could fire in the middle of the new
 *			attempt and reset the screen back to the idle prompt while
 *			something is still in progress.
 */
void	status_display_cancel_pending_revert(void);
