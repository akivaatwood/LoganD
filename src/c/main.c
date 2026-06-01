#include <pebble.h>

static const int16_t s_emblem_y_offset = 2;
static char s_temperature_text[8] = "--°";

static Window *s_main_window;
static Layer *s_canvas_layer;
static GBitmap *s_center_emblem;

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *temperature_t = dict_find(iterator, 0);
  if (temperature_t) {
    snprintf(s_temperature_text, sizeof(s_temperature_text), "%s", temperature_t->value->cstring);
    if (s_canvas_layer) {
      layer_mark_dirty(s_canvas_layer);
    }
  }
}

static GColor color_bg(void) {
#ifdef PBL_COLOR
  return GColorFromHEX(0xCEDEE7);
#else
  return GColorWhite;
#endif
}

static GColor color_text(void) {
#ifdef PBL_COLOR
  return GColorFromHEX(0x585673);
#else
  return GColorBlack;
#endif
}

static void draw_center_emblem(GContext *ctx, GPoint center) {
  if (!s_center_emblem) {
    return;
  }

  const GRect emblem_bounds = gbitmap_get_bounds(s_center_emblem);
  const GRect dest = GRect(center.x - emblem_bounds.size.w / 2,
                           center.y - emblem_bounds.size.h / 2 + s_emblem_y_offset,
                           emblem_bounds.size.w,
                           emblem_bounds.size.h);

  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, s_center_emblem, dest);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  const GPoint center = grect_center_point(&bounds);

  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);

  char time_buffer[6];
  char date_buffer[16];
  char footer_buffer[32];

  clock_copy_time_string(time_buffer, sizeof(time_buffer));
  strftime(date_buffer, sizeof(date_buffer), "%a %e", tick_time);
  snprintf(footer_buffer, sizeof(footer_buffer), "%s  %s", date_buffer, s_temperature_text);

  graphics_context_set_fill_color(ctx, color_bg());
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_text_color(ctx, color_text());
  graphics_draw_text(ctx,
                     time_buffer,
                     fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
                     GRect(0, 8, bounds.size.w, 50),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter,
                     NULL);

  draw_center_emblem(ctx, center);

  graphics_draw_text(ctx,
                     footer_buffer,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(0, bounds.size.h - 36, bounds.size.w, 28),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter,
                     NULL);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_canvas_layer);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(window_layer);

  window_set_background_color(window, color_bg());

  s_center_emblem = gbitmap_create_with_resource(RESOURCE_ID_CENTER_EMBLEM2);
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);
}

static void main_window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
  gbitmap_destroy(s_center_emblem);
}

static void init(void) {
  app_message_register_inbox_received(inbox_received_callback);
  app_message_open(64, 64);

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
