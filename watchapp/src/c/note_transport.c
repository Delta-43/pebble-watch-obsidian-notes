#include "note_transport.h"
#include "status_display.h"

#define RESULT_TIMEOUT_MS 15000
/* Tuple->length includes the null terminator, so a real (non-empty) string
 * sent from the phone always has a length greater than 1. */
#define MIN_NONEMPTY_TUPLE_LENGTH 1

static bool			s_waiting_for_reply;
static AppTimer		*s_result_timeout_timer;

/*
 * @brief Clears the "waiting for a reply" state shared by every path that
 *			ends it: a real reply, a send failure, or a timeout.
 * @param cancel_timer Whether to actually cancel s_result_timeout_timer.
 *			Pass true from any path *other* than the timer's own callback
 *			(a real reply or a send failure, where the timer is still
 *			pending and must be stopped). Pass false from
 *			result_timeout_callback() itself -- by the time that callback
 *			runs, the timer has already fired and the SDK has invalidated its
 *			handle, so calling app_timer_cancel() on it there would operate
 *			on a stale AppTimer pointer.
 */
static void	clear_waiting_state(bool cancel_timer)
{
	if (cancel_timer && s_result_timeout_timer)
		app_timer_cancel(s_result_timeout_timer);
	s_result_timeout_timer = NULL;
	s_waiting_for_reply = false;
}

/*
 * @brief Stops waiting for a reply: cancels the timeout timer (if one is
 *			pending) and marks the transport as no longer busy. Called from
 *			every path that ends the "waiting for the phone" period other
 *			than the timeout itself (see result_timeout_callback()), whether
 *			that's a real reply or a send failure.
 */
static void	stop_waiting_for_reply(void)
{
	clear_waiting_state(true);
}

/*
 * @brief AppTimer callback: fires if the phone never replies within
 *			RESULT_TIMEOUT_MS of us sending a note. Shows a timeout message so
 *			the user isn't left staring at "Sending..." forever.
 * @param data Unused (required by the AppTimer callback signature).
 */
static void	result_timeout_callback(void *data)
{
	clear_waiting_state(false);
	status_display_show_result("Timed out\nwaiting for phone");
}

/*
 * @brief AppMessage inbox handler: called whenever the phone sends the watch
 *			any message. Ignores anything that arrives while we're not
 *			actually waiting for a reply (e.g. a stray/duplicate message), and
 *			ignores anything that isn't a resultStatus reply specifically.
 * @param iterator The received message's key/value pairs.
 * @param context Unused (required by the AppMessage callback signature).
 */
static void	inbox_received_handler(DictionaryIterator *iterator, void *context)
{
	Tuple	*status_tuple;
	Tuple	*message_tuple;
	static char	failure_buffer[64];

	if (!s_waiting_for_reply)
		return ;
	status_tuple = dict_find(iterator, MESSAGE_KEY_resultStatus);
	if (!status_tuple)
		return ;
	stop_waiting_for_reply();
	if (status_tuple->value->int32 != 0)
	{
		status_display_show_result("Saved!");
		return ;
	}
	message_tuple = dict_find(iterator, MESSAGE_KEY_resultMessage);
	if (message_tuple && message_tuple->length > MIN_NONEMPTY_TUPLE_LENGTH)
	{
		snprintf(failure_buffer, sizeof(failure_buffer), "Failed:\n%s",
				message_tuple->value->cstring);
		status_display_show_result(failure_buffer);
	}
	else
		status_display_show_result("Failed to save note");
}

/*
 * @brief AppMessage outbox-failed handler: called if the watch itself
 *			couldn't get the message to the phone at all (as opposed to the
 *			phone receiving it but the webhook call failing). Most commonly
 *			means the watch and phone aren't currently connected.
 * @param iterator Unused -- we don't need details about the failed message.
 * @param reason Unused -- every failure reason gets the same user-facing
 *			message here; see docs/TROUBLESHOOTING.md if you ever need to
 *			distinguish them.
 * @param context Unused (required by the AppMessage callback signature).
 */
static void	outbox_failed_handler(DictionaryIterator *iterator,
	AppMessageResult reason, void *context)
{
	if (!s_waiting_for_reply)
		return ;
	stop_waiting_for_reply();
	status_display_show_result("No phone\nconnection");
}

void	note_transport_init(void)
{
	AppMessageResult	result;

	app_message_register_inbox_received(inbox_received_handler);
	app_message_register_outbox_failed(outbox_failed_handler);
	result = app_message_open(app_message_inbox_size_maximum(),
			app_message_outbox_size_maximum());
	if (result != APP_MSG_OK)
		APP_LOG(APP_LOG_LEVEL_ERROR,
			"app_message_open() failed: %d -- notes will not send", result);
}

void	note_transport_deinit(void)
{
	stop_waiting_for_reply();
}

bool	note_transport_is_busy(void)
{
	return (s_waiting_for_reply);
}

void	note_transport_send_note(char *transcription)
{
	DictionaryIterator	*out_iter;
	AppMessageResult	result;
	DictionaryResult	dict_result;
	const char			*failure_message;

	failure_message = NULL;
	result = app_message_outbox_begin(&out_iter);
	if (result != APP_MSG_OK)
		failure_message = "Failed to save note";
	else
	{
		/* Checked (rather than ignored) because the outbox buffer size is
		 * negotiated at app_message_open() time and isn't guaranteed to fit
		 * a full NOTE_BUFFER_SIZE-length transcription plus dictionary
		 * overhead -- without this check, a note too long for the buffer
		 * would otherwise fail silently at app_message_outbox_send() with
		 * the same generic message as a connectivity failure. */
		dict_result = dict_write_cstring(out_iter, MESSAGE_KEY_noteText,
				transcription);
		if (dict_result != DICT_OK)
			failure_message = "Note too long\nto send";
		else
		{
			result = app_message_outbox_send();
			if (result != APP_MSG_OK)
				failure_message = "Failed to save note";
		}
	}
	if (failure_message)
	{
		status_display_show_result(failure_message);
		return ;
	}
	s_waiting_for_reply = true;
	status_display_show_sending();
	s_result_timeout_timer = app_timer_register(RESULT_TIMEOUT_MS,
			result_timeout_callback, NULL);
}
