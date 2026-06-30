#include <pebble.h>

/* Version: 2026-06-12 */

static const int16_t s_emblem_y_offset = 9;
static const size_t EMBLEM_COUNT = 12;
static const int32_t TEMPERATURE_TTL_SECONDS = 2 * 60 * 60;
static const char *const s_emblem_labels[12] = {
  "Champions of Fenris",
  "Bloodmaws",
  "Seawolves",
  "Sons of Morkai",
  "Red Moons",
  "Deathwolves",
  "Stormwolves",
  "Ironwolves",
  "Drakeslayers",
  "Blackmanes",
  "Firehowlers",
  "Grimbloods"
};
static char s_temperature_text[8] = "--°";
static time_t s_temperature_updated_at = 0;
static const uint32_t KEY_TEMPERATURE = 0;
static const uint32_t KEY_AUTO_ROTATE = 2;
static const uint32_t KEY_FIXED_IMAGE_INDEX = 3;
static const uint32_t PERSIST_KEY_TEMPERATURE_TEXT = 1;
static const uint32_t PERSIST_KEY_TEMPERATURE_UPDATED_AT = 5;
static const uint32_t PERSIST_KEY_AUTO_ROTATE = 2;
static const uint32_t PERSIST_KEY_FIXED_IMAGE_INDEX = 3;

static Window *s_main_window;
static Layer *s_canvas_layer;
static BitmapLayer *s_emblem_layer;
static GBitmap *s_center_emblems[12];
static size_t s_current_emblem_index = 0;
static bool s_auto_rotate = true;
static size_t s_fixed_emblem_index = 0;

static void update_active_emblem_index(const struct tm *tick_time);
static void update_emblem_layer_bitmap(void);
static GColor color_bg(void);
static GColor emblem_bg_color(size_t index);
static GColor color_text(void);

static bool is_placeholder_temperature(const char *value) {
  return strcmp(value, "--°") == 0;
}

static bool is_temperature_fresh(void) {
  time_t now;

  if (is_placeholder_temperature(s_temperature_text) || s_temperature_updated_at <= 0) {
    return false;
  }

  now = time(NULL);
  return difftime(now, s_temperature_updated_at) < TEMPERATURE_TTL_SECONDS;
}

static const char *display_temperature_text(void) {
  return is_temperature_fresh() ? s_temperature_text : "--°";
}

static void request_temperature_update(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK || !iter) {
    return;
  }

  dict_write_uint8(iter, MESSAGE_KEY_REQUEST_WEATHER, 1);
  dict_write_end(iter);
  app_message_outbox_send();
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *temperature_t = dict_find(iterator, KEY_TEMPERATURE);
  Tuple *auto_rotate_t = dict_find(iterator, KEY_AUTO_ROTATE);
  Tuple *fixed_image_index_t = dict_find(iterator, KEY_FIXED_IMAGE_INDEX);
  if (temperature_t && temperature_t->type == TUPLE_CSTRING && strlen(temperature_t->value->cstring) > 0) {
    if (is_placeholder_temperature(temperature_t->value->cstring) && is_temperature_fresh()) {
      return;
    }

    snprintf(s_temperature_text, sizeof(s_temperature_text), "%s", temperature_t->value->cstring);
    persist_write_string(PERSIST_KEY_TEMPERATURE_TEXT, s_temperature_text);
    if (is_placeholder_temperature(s_temperature_text)) {
      s_temperature_updated_at = 0;
      persist_write_int(PERSIST_KEY_TEMPERATURE_UPDATED_AT, 0);
    } else {
      s_temperature_updated_at = time(NULL);
      persist_write_int(PERSIST_KEY_TEMPERATURE_UPDATED_AT, (int32_t)s_temperature_updated_at);
    }
    if (s_canvas_layer) {
      layer_mark_dirty(s_canvas_layer);
    }
  }

  if (auto_rotate_t) {
    s_auto_rotate = auto_rotate_t->value->int32 != 0;
    persist_write_bool(PERSIST_KEY_AUTO_ROTATE, s_auto_rotate);
  }

  if (fixed_image_index_t) {
    int32_t candidate_index = fixed_image_index_t->value->int32;
    if (candidate_index >= 0 && candidate_index < (int32_t)EMBLEM_COUNT) {
      s_fixed_emblem_index = (size_t)candidate_index;
      persist_write_int(PERSIST_KEY_FIXED_IMAGE_INDEX, (int32_t)s_fixed_emblem_index);
    }
  }

  if (auto_rotate_t || fixed_image_index_t) {
    time_t now = time(NULL);
    struct tm *tick_time = localtime(&now);
    update_active_emblem_index(tick_time);
    update_emblem_layer_bitmap();
    if (s_canvas_layer) {
      layer_mark_dirty(s_canvas_layer);
    }
  }
}

static GColor color_bg(void) {
#ifdef PBL_COLOR
  return emblem_bg_color(s_current_emblem_index);
#else
  return GColorWhite;
#endif
}

static GColor emblem_bg_color(size_t index) {
  switch (index) {
    case 0: return GColorFromRGB(0, 0, 0);
    case 1: return GColorFromRGB(255, 255, 255);
    case 2: return GColorFromRGB(42, 104, 141);
    case 3: return GColorFromRGB(255, 255, 255);
    case 4: return GColorFromRGB(1, 1, 1);
    case 5: return GColorFromRGB(236, 32, 42);
    case 6: return GColorFromRGB(43, 115, 162);
    case 7: return GColorFromRGB(0, 0, 0);
    case 8: return GColorFromRGB(42, 104, 142);
    case 9: return GColorFromRGB(235, 187, 7);
    case 10: return GColorFromRGB(216, 28, 1);
    case 11: return GColorFromRGB(0, 0, 2);
  }

  return GColorFromRGB(0, 0, 0);
}

static GColor color_text(void) {
#ifdef PBL_COLOR
  switch (s_current_emblem_index) {
    case 0:
    case 4:
    case 5:
    case 7:
    case 10:
    case 11:
      return GColorWhite;
    default:
      return GColorBlack;
  }
#else
  return GColorBlack;
#endif
}

static GColor color_battery(int charge_percent) {
#ifdef PBL_COLOR
  int red;
  int green;

  if (charge_percent <= 10) {
    return GColorFromRGB(255, 0, 0);
  }

  if (charge_percent >= 100) {
    return GColorFromRGB(0, 255, 0);
  }

  if (charge_percent <= 50) {
    green = ((charge_percent - 10) * 255) / 40;
    return GColorFromRGB(255, green, 0);
  }

  red = 255 - (((charge_percent - 50) * 255) / 50);
  return GColorFromRGB(red, 255, 0);
#else
  return GColorBlack;
#endif
}

static void draw_battery_badge(GContext *ctx, const char *text, GRect rect, GColor fill_color) {
  GColor text_color = color_text();

  graphics_context_set_fill_color(ctx, fill_color);
  graphics_fill_rect(ctx, rect, 6, GCornersAll);

  graphics_context_set_text_color(ctx, text_color);
  graphics_draw_text(ctx,
                     text,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     rect,
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter,
                     NULL);
}

static void update_active_emblem_index(const struct tm *tick_time) {
  if (s_auto_rotate) {
    s_current_emblem_index = (size_t)((tick_time->tm_min / 5) % EMBLEM_COUNT);
  } else {
    s_current_emblem_index = s_fixed_emblem_index;
  }
}

static GBitmap *active_emblem(void) {
  return s_center_emblems[s_current_emblem_index];
}

static void update_emblem_layer_bitmap(void) {
  if (s_emblem_layer) {
    bitmap_layer_set_bitmap(s_emblem_layer, active_emblem());
  }
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  const BatteryChargeState battery_state = battery_state_service_peek();
  GBitmap *current_emblem = active_emblem();

  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);

  char time_buffer[6];
  char date_buffer[16];
  char battery_buffer[8];
  GRect battery_rect = GRect(8, 2, 50, 22);
  GRect date_rect = GRect(0, 1, bounds.size.w, 24);
  GRect temperature_rect = GRect(bounds.size.w - 58, 2, 50, 22);
  GRect label_rect = GRect(0, 136, bounds.size.w, 26);

  clock_copy_time_string(time_buffer, sizeof(time_buffer));
  strftime(date_buffer, sizeof(date_buffer), "%a %e", tick_time);
  snprintf(battery_buffer, sizeof(battery_buffer), "%d%%", battery_state.charge_percent);

  if (current_emblem) {
    GRect emblem_bounds = gbitmap_get_bounds(current_emblem);
    label_rect.origin.y = ((bounds.size.h / 2) - (emblem_bounds.size.h / 2)) +
                          s_emblem_y_offset + emblem_bounds.size.h + 4;
  }

  graphics_context_set_fill_color(ctx, color_bg());
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  draw_battery_badge(ctx, battery_buffer, battery_rect, color_battery(battery_state.charge_percent));

  graphics_context_set_text_color(ctx, color_text());
  graphics_draw_text(ctx,
                     date_buffer,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     date_rect,
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter,
                     NULL);

  graphics_draw_text(ctx,
                     display_temperature_text(),
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     temperature_rect,
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight,
                     NULL);

  graphics_context_set_text_color(ctx, color_text());
  graphics_draw_text(ctx,
                     time_buffer,
                     fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
                     GRect(0, 18, bounds.size.w, 50),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter,
                     NULL);

  graphics_draw_text(ctx,
                     s_emblem_labels[s_current_emblem_index],
                     fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     label_rect,
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter,
                     NULL);

}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_active_emblem_index(tick_time);
  update_emblem_layer_bitmap();

  if (tick_time->tm_min % 5 == 0) {
    request_temperature_update();
  }
  layer_mark_dirty(s_canvas_layer);
}

static GBitmap *load_emblem_resource(size_t index) {
  switch (index) {
    case 0: return gbitmap_create_with_resource(RESOURCE_ID_CENTER_EMBLEM0);
    case 1: return gbitmap_create_with_resource(RESOURCE_ID_CENTER_EMBLEM1);
    case 2: return gbitmap_create_with_resource(RESOURCE_ID_CENTER_EMBLEM2);
    case 3: return gbitmap_create_with_resource(RESOURCE_ID_CENTER_EMBLEM3);
    case 4: return gbitmap_create_with_resource(RESOURCE_ID_CENTER_EMBLEM4);
    case 5: return gbitmap_create_with_resource(RESOURCE_ID_CENTER_EMBLEM5);
    case 6: return gbitmap_create_with_resource(RESOURCE_ID_CENTER_EMBLEM6);
    case 7: return gbitmap_create_with_resource(RESOURCE_ID_CENTER_EMBLEM7);
    case 8: return gbitmap_create_with_resource(RESOURCE_ID_CENTER_EMBLEM8);
    case 9: return gbitmap_create_with_resource(RESOURCE_ID_CENTER_EMBLEM9);
    case 10: return gbitmap_create_with_resource(RESOURCE_ID_CENTER_EMBLEM10);
    case 11: return gbitmap_create_with_resource(RESOURCE_ID_CENTER_EMBLEM11);
  }

  return NULL;
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(window_layer);
  const GPoint center = grect_center_point(&bounds);
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);
  size_t i;

  window_set_background_color(window, color_bg());

  for (i = 0; i < EMBLEM_COUNT; i += 1) {
    s_center_emblems[i] = load_emblem_resource(i);
  }

  update_active_emblem_index(tick_time);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  {
    GBitmap *current_emblem = active_emblem();
    GRect emblem_bounds = gbitmap_get_bounds(current_emblem);
    GRect emblem_frame = GRect(center.x - emblem_bounds.size.w / 2,
                               center.y - emblem_bounds.size.h / 2 + s_emblem_y_offset,
                               emblem_bounds.size.w,
                               emblem_bounds.size.h);

    s_emblem_layer = bitmap_layer_create(emblem_frame);
    bitmap_layer_set_bitmap(s_emblem_layer, current_emblem);
    bitmap_layer_set_compositing_mode(s_emblem_layer, GCompOpSet);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_emblem_layer));
  }
}

static void main_window_unload(Window *window) {
  size_t i;

  bitmap_layer_destroy(s_emblem_layer);
  layer_destroy(s_canvas_layer);
  for (i = 0; i < EMBLEM_COUNT; i += 1) {
    gbitmap_destroy(s_center_emblems[i]);
  }
}

static void init(void) {
  if (persist_exists(PERSIST_KEY_TEMPERATURE_TEXT)) {
    persist_read_string(PERSIST_KEY_TEMPERATURE_TEXT, s_temperature_text, sizeof(s_temperature_text));
  }
  if (persist_exists(PERSIST_KEY_TEMPERATURE_UPDATED_AT)) {
    s_temperature_updated_at = (time_t)persist_read_int(PERSIST_KEY_TEMPERATURE_UPDATED_AT);
  } else if (!is_placeholder_temperature(s_temperature_text)) {
    s_temperature_updated_at = time(NULL);
    persist_write_int(PERSIST_KEY_TEMPERATURE_UPDATED_AT, (int32_t)s_temperature_updated_at);
  }
  if (persist_exists(PERSIST_KEY_AUTO_ROTATE)) {
    s_auto_rotate = persist_read_bool(PERSIST_KEY_AUTO_ROTATE);
  }
  if (persist_exists(PERSIST_KEY_FIXED_IMAGE_INDEX)) {
    int persisted_index = persist_read_int(PERSIST_KEY_FIXED_IMAGE_INDEX);
    if (persisted_index >= 0 && persisted_index < (int)EMBLEM_COUNT) {
      s_fixed_emblem_index = (size_t)persisted_index;
    }
  }

  app_message_register_inbox_received(inbox_received_callback);
  app_message_open(128, 128);

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  request_temperature_update();
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
