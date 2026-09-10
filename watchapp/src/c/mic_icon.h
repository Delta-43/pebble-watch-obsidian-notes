#pragma once

#include <pebble.h>

/*
 * @brief On touch-capable platforms only, draws a tappable mic icon at the
 *			bottom of the screen that starts dictation, same as pressing
 *			SELECT. Every function and constant in this file only exists when
 *			PBL_TOUCH is defined -- a compile-time *capability* check (does
 *			this specific platform build have a touchscreen), not a check for
 *			a specific platform name, so this automatically applies to any
 *			future Pebble platform with a touchscreen too.
 *
 *			The icon's artwork is the MIC_ICON bitmap resource, rasterized
 *			from ../../../design/mic_icon.svg -- edit that file to change the
 *			design, then regenerate resources/images/mic_icon.png from it.
 */

#ifdef PBL_TOUCH

/* Size (both width and height) of the tappable icon, and how far its bottom
 * edge sits above the bottom of the screen. Exposed here so main.c can leave
 * enough room for it when laying out the status text above it. */
#define MIC_ICON_SIZE 52
#define MIC_ICON_BOTTOM_MARGIN 10

/*
 * @brief Creates the mic icon layer, positioned centered at the bottom of
 *			the screen, and subscribes to touch input. Call once, from the
 *			window's load handler.
 * @param parent_layer The layer to attach the icon to (normally the window's
 *			root layer).
 * @param window_bounds The window's full bounds, used to center the icon
 *			horizontally and position it at the bottom.
 */
void	mic_icon_create(Layer *parent_layer, GRect window_bounds);

/*
 * @brief Destroys the icon layer/bitmap and unsubscribes from touch input.
 *			Call once, from the window's unload handler.
 */
void	mic_icon_destroy(void);

/*
 * @brief Hides the icon if the watch's touchscreen is currently disabled
 *			(hardware present, but turned off, e.g. by the user), so it
 *			doesn't sit on screen as a dead tap target -- SELECT still always
 *			works regardless. Call from the window's appear handler, since
 *			that's when the Pebble docs say touch_service_is_enabled() should
 *			be checked (not once at load).
 */
void	mic_icon_update_visibility(void);

#endif
