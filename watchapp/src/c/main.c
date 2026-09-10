#include <pebble.h>
#include "status_display.h"
#include "note_transport.h"
#include "dictation_handler.h"
#include "mic_icon.h"

/*
 * @brief App entry point. Creates the single window and wires the other
 *			modules together -- this file doesn't contain any dictation,
 *			AppMessage, or drawing logic itself, it just owns the window's
 *			lifecycle and hands each part of the screen to the module
 *			responsible for it:
 *				- status_display: the text that says what's going on
 *				- mic_icon: the tap-to-record icon (touch platforms only)
 *				- dictation_handler: starting a dictation session
 *				- note_transport: sending the result to the phone
 *			See ../../../PLAN.md for the full end-to-end design, and
 *			../../../CLAUDE.md / ../../../TODO.md for what's been built and
 *			verified so far.
 */

static Window	*s_window;

/*
 * @brief SELECT button handler: starts dictation. All the "is the app busy
 *			right now" logic lives in dictation_handler_start() itself, so
 *			this handler doesn't need to check anything first.
 * @param recognizer Unused (required by the ClickHandler signature).
 * @param context Unused (required by the ClickHandler signature).
 */
static void	select_click_handler(ClickRecognizerRef recognizer, void *context)
{
	dictation_handler_start();
}

/*
 * @brief Registers the button(s) this app responds to. Pebble calls this
 *			once per window to set up click handling.
 * @param context Unused (required by the ClickConfigProvider signature).
 */
static void	click_config_provider(void *context)
{
	window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

#ifdef PBL_TOUCH
/*
 * @brief Window appear handler (touch platforms only): re-checks whether
 *			touch is currently enabled every time this window becomes
 *			visible, so the mic icon's visibility always reflects the
 *			watch's current touch setting.
 * @param window Unused (there's only ever one window in this app).
 */
static void	window_appear(Window *window)
{
	mic_icon_update_visibility();
}
#endif

/*
 * @brief Window load handler: builds the screen. Leaves room at the bottom
 *			of the status text for the mic icon on touch platforms, so the
 *			two never overlap.
 * @param window The window being loaded.
 */
static void	window_load(Window *window)
{
	Layer	*window_layer;
	GRect	bounds;
	GRect	status_frame;
	int16_t	bottom_inset;

	window_layer = window_get_root_layer(window);
	bounds = layer_get_bounds(window_layer);
#ifdef PBL_TOUCH
	bottom_inset = MIC_ICON_SIZE + MIC_ICON_BOTTOM_MARGIN;
#else
	bottom_inset = 0;
#endif
	status_frame = GRect(0, 60, bounds.size.w, bounds.size.h - 60 - bottom_inset);
	status_display_init(window_layer, status_frame);
#ifdef PBL_TOUCH
	mic_icon_create(window_layer, bounds);
#endif
}

/*
 * @brief Window unload handler: tears down everything window_load() created,
 *			in the same order status_display/mic_icon expect (each module's
 *			own deinit function handles its own cleanup).
 * @param window The window being unloaded.
 */
static void	window_unload(Window *window)
{
	status_display_deinit();
#ifdef PBL_TOUCH
	mic_icon_destroy();
#endif
}

/*
 * @brief Creates the window and initializes every other module. Order
 *			matters a little: the window is pushed (and so window_load()
 *			runs, creating the mic icon and subscribing to touch) before
 *			dictation_handler/note_transport are initialized, but none of
 *			those can actually fire until the user presses SELECT or taps the
 *			icon, so the exact ordering here isn't safety-critical -- it just
 *			mirrors the natural "screen first, then the things that act on
 *			it" reading order.
 */
static void	init(void)
{
	s_window = window_create();
	window_set_click_config_provider(s_window, click_config_provider);
	window_set_window_handlers(s_window, (WindowHandlers){
		.load = window_load,
		.unload = window_unload,
#ifdef PBL_TOUCH
		.appear = window_appear,
#endif
	});
	window_stack_push(s_window, true);
	dictation_handler_init();
	note_transport_init();
}

/*
 * @brief Tears down every module in the reverse of how init() set them up.
 */
static void	deinit(void)
{
	note_transport_deinit();
	dictation_handler_deinit();
	window_destroy(s_window);
}

int	main(void)
{
	init();
	app_event_loop();
	deinit();
}
