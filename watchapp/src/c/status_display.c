#include "status_display.h"

#define IDLE_PROMPT "Let's take a note.\n(Press SELECT)"
#define REVERT_TO_IDLE_DELAY_MS 3000

static TextLayer	*s_text_layer;
static AppTimer		*s_revert_to_idle_timer;

/*
 * @brief AppTimer callback: puts the idle prompt back on screen. Fires a few
 *			seconds after status_display_show_result() was called, unless
 *			something cancelled it first via status_display_cancel_pending_revert().
 * @param data Unused (required by the AppTimer callback signature).
 */
static void	revert_to_idle_callback(void *data)
{
	s_revert_to_idle_timer = NULL;
	status_display_show_idle();
}

void	status_display_init(Layer *parent_layer, GRect frame)
{
	s_text_layer = text_layer_create(frame);
	text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
	text_layer_set_font(s_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text(s_text_layer, IDLE_PROMPT);
	layer_add_child(parent_layer, text_layer_get_layer(s_text_layer));
}

void	status_display_deinit(void)
{
	status_display_cancel_pending_revert();
	text_layer_destroy(s_text_layer);
}

void	status_display_show_sending(void)
{
	text_layer_set_text(s_text_layer, "Sending...");
}

void	status_display_show_idle(void)
{
	status_display_cancel_pending_revert();
	text_layer_set_text(s_text_layer, IDLE_PROMPT);
}

void	status_display_show_result(const char *text)
{
	text_layer_set_text(s_text_layer, text);
	status_display_cancel_pending_revert();
	s_revert_to_idle_timer = app_timer_register(REVERT_TO_IDLE_DELAY_MS,
			revert_to_idle_callback, NULL);
}

void	status_display_cancel_pending_revert(void)
{
	if (s_revert_to_idle_timer)
	{
		app_timer_cancel(s_revert_to_idle_timer);
		s_revert_to_idle_timer = NULL;
	}
}
