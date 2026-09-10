#include "mic_icon.h"

#ifdef PBL_TOUCH

#include "dictation_handler.h"

static Layer	*s_icon_layer;
static GBitmap	*s_icon_bitmap;

/*
 * @brief Layer update proc: draws the mic bitmap at its native size (it's
 *			already rendered at MIC_ICON_SIZE, so no scaling happens here),
 *			respecting the bitmap's own transparency (GCompOpSet) rather than
 *			drawing a solid rectangle over whatever is behind it.
 * @param layer The icon layer being drawn.
 * @param ctx The graphics context to draw into.
 */
static void	icon_update_proc(Layer *layer, GContext *ctx)
{
	graphics_context_set_compositing_mode(ctx, GCompOpSet);
	graphics_draw_bitmap_in_rect(ctx, s_icon_bitmap, layer_get_bounds(layer));
}

/*
 * @brief TouchService handler: on the initial touch-down of a tap, checks
 *			whether it landed inside the icon's own frame and, if so, starts
 *			dictation -- exactly like pressing SELECT. Ignores every other
 *			touch event type (position updates, liftoff) since only where a
 *			touch *starts* matters for a simple tappable icon.
 * @param event The touch event: its type and (x, y) position.
 * @param context Unused (required by the TouchServiceHandler signature).
 */
static void	touch_handler(const TouchEvent *event, void *context)
{
	GRect	icon_frame;
	GPoint	tap_point;

	if (event->type != TouchEvent_Touchdown)
		return ;
	icon_frame = layer_get_frame(s_icon_layer);
	tap_point = GPoint(event->x, event->y);
	if (grect_contains_point(&icon_frame, &tap_point))
		dictation_handler_start();
}

void	mic_icon_create(Layer *parent_layer, GRect window_bounds)
{
	GRect	icon_frame;

	icon_frame = GRect(
			(window_bounds.size.w - MIC_ICON_SIZE) / 2,
			window_bounds.size.h - MIC_ICON_SIZE - MIC_ICON_BOTTOM_MARGIN,
			MIC_ICON_SIZE,
			MIC_ICON_SIZE);
	s_icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_MIC_ICON);
	s_icon_layer = layer_create(icon_frame);
	layer_set_update_proc(s_icon_layer, icon_update_proc);
	layer_add_child(parent_layer, s_icon_layer);
	touch_service_subscribe(touch_handler, NULL);
}

void	mic_icon_destroy(void)
{
	touch_service_unsubscribe();
	layer_destroy(s_icon_layer);
	gbitmap_destroy(s_icon_bitmap);
}

void	mic_icon_update_visibility(void)
{
	layer_set_hidden(s_icon_layer, !touch_service_is_enabled());
}

#endif
