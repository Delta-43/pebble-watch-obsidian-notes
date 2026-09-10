#include "dictation_handler.h"
#include "note_transport.h"
#include "status_display.h"

/* How large a transcription this app will accept. Notes dictated for this
 * app are meant to be short (quick reminders, not essays), so 512 bytes of
 * room is generous, not tight. */
#define NOTE_BUFFER_SIZE 512

static DictationSession	*s_dictation_session;
/* True from the moment dictation_session_start() is called until the OS
 * calls dictation_callback() with a final status. Distinct from
 * note_transport_is_busy(), which only covers the later "waiting for the
 * phone's reply" phase -- without this flag, a second trigger fired while
 * the OS is still listening would re-enter dictation_session_start() on the
 * same session. */
static bool			s_listening;

static void	dictation_callback(DictationSession *session,
	DictationSessionStatus status, char *transcription, void *context);

/*
 * @brief Creates (or, after a prior failure, retries creating) the
 *			DictationSession. Safe to call more than once -- no-ops if a
 *			session already exists.
 */
static void	create_session_if_needed(void)
{
	if (s_dictation_session)
		return ;
	s_dictation_session = dictation_session_create(NOTE_BUFFER_SIZE,
			dictation_callback, NULL);
	if (s_dictation_session)
		dictation_session_enable_confirmation(s_dictation_session, true);
}

/*
 * @brief DictationSession callback: called once the OS finishes (or gives
 *			up on) listening. On success, hands the transcription off to
 *			note_transport to actually send it to the phone. On failure,
 *			shows a short explanation for the failure modes the OS's own
 *			dictation UI doesn't already explain on-screen itself -- anything
 *			else (e.g. the user cancelling) just silently returns to idle,
 *			since the OS already made it clear what happened.
 * @param session The session that finished (unused -- there's only ever one).
 * @param status What happened: success, or one of several failure reasons.
 * @param transcription The dictated text, only valid (and only meaningful)
 *			when status is DictationSessionStatusSuccess.
 * @param context Unused (required by the DictationSessionStatusCallback
 *			signature).
 */
static void	dictation_callback(DictationSession *session,
	DictationSessionStatus status, char *transcription, void *context)
{
	s_listening = false;
	if (status == DictationSessionStatusSuccess)
	{
		note_transport_send_note(transcription);
		return ;
	}
	if (status == DictationSessionStatusFailureConnectivityError)
		status_display_show_result("No phone\nconnection");
	else if (status == DictationSessionStatusFailureDisabled)
		status_display_show_result("Dictation not\navailable");
	else
		status_display_show_idle();
}

void	dictation_handler_init(void)
{
	create_session_if_needed();
}

void	dictation_handler_deinit(void)
{
	dictation_session_destroy(s_dictation_session);
}

void	dictation_handler_start(void)
{
	if (note_transport_is_busy() || s_listening)
		return ;
	/* dictation_session_create() can fail (e.g. phone disconnected at
	 * launch) and leave s_dictation_session NULL -- retry it here so a
	 * phone reconnecting later isn't stuck with a permanently-broken
	 * session for the rest of the app's life. */
	create_session_if_needed();
	if (!s_dictation_session)
	{
		status_display_show_result("Dictation not\navailable");
		return ;
	}
	/* A result screen still on display counts as idle -- the user can
	 * retry immediately without waiting for it to auto-revert. Cancel that
	 * pending revert so it can't fire mid-retry and reset the screen while
	 * this new attempt is still in progress. */
	status_display_cancel_pending_revert();
	s_listening = true;
	dictation_session_start(s_dictation_session);
}
