#include <pebble.h>

#define PI 3.141592653589793238462643383

static Window *window;
static Layer *display_layer;
static InverterLayer *inverter_layer;

#define WIDTH 144
#define HEIGHT 168
#define OFFSET 12
#define CENTRE { WIDTH / 2, HEIGHT / 2 }
#define TOP_CENTRE { WIDTH / 2, 0 }
#define TOP_LEFT {0, 0}
#define BOTTOM_LEFT {0, HEIGHT}
#define BOTTOM_RIGHT { WIDTH, HEIGHT }
#define TOP_RIGHT { WIDTH, 0}

#define RING_WIDTH 10
#define GAP 5
#define MAX_RADIUS (WIDTH / 2) - GAP

#define KEY_INVERT 0
#define KEY_ORDER 1

# define M_PI		3.14159265358979323846	/* pi */

static GPoint centre = CENTRE;
static GPath *path = NULL;
static GPathInfo path_info = {
    .num_points = 7,
    .points = (GPoint[]) {
        CENTRE,
        TOP_CENTRE,
        TOP_LEFT,
        BOTTOM_LEFT,
        BOTTOM_RIGHT,
        TOP_RIGHT,
        {0, 0}
    }
};

static GBitmap *buffer;

static bool seconds_outside = true;
static bool invert = false;
static bool connected = false;

static void draw_ring(Layer *layer, GContext *ctx, double angle, int radius) {
    GPoint end;

    graphics_context_set_compositing_mode(ctx, GCompOpAssign);

    // Clear
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(0, 0, WIDTH, HEIGHT), 0, GCornerNone);

    // Draw big circle
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_circle(ctx, centre, radius);

    if(angle == 0) {
        angle = TRIG_MAX_ANGLE;
    }

    graphics_context_set_fill_color(ctx, GColorClear);
    if(angle <= TRIG_MAX_ANGLE / 4) {
        path_info.points[4] = (GPoint) BOTTOM_RIGHT;
        path_info.points[5] = (GPoint) TOP_RIGHT;
    } else if(angle <= TRIG_MAX_ANGLE / 2) {
        path_info.points[4] = (GPoint) BOTTOM_RIGHT;
        path_info.points[5] = (GPoint) BOTTOM_RIGHT;
    } else if(angle <= TRIG_MAX_ANGLE * 0.75) {
        path_info.points[4] = (GPoint) BOTTOM_LEFT;
        path_info.points[5] = (GPoint) BOTTOM_LEFT;
    } else {
        path_info.points[4] = (GPoint) TOP_LEFT;
        path_info.points[5] = (GPoint) TOP_LEFT;
    }

    path_info.points[6].x = centre.x + (sin_lookup(angle) * WIDTH / TRIG_MAX_RATIO);
    path_info.points[6].y = centre.y - (cos_lookup(angle) * WIDTH / TRIG_MAX_RATIO);

    path = gpath_create(&path_info);
    gpath_draw_filled(ctx, path);
    gpath_destroy(path);
    
    // Draw small circle
    graphics_context_set_fill_color(ctx, GColorClear);
    graphics_fill_circle(ctx, centre, radius - RING_WIDTH);
}

static void snapshot(GContext *ctx) {
    GBitmap *display_bitmap = graphics_capture_frame_buffer(ctx);
    memcpy(buffer->addr, display_bitmap->addr + (display_bitmap->row_size_bytes * OFFSET), display_bitmap->row_size_bytes * WIDTH);
    graphics_release_frame_buffer(ctx, display_bitmap);
}

static void display_layer_update(Layer *layer, GContext *ctx) {
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    double angle;

    // Second
    angle = TRIG_MAX_ANGLE * (tick_time->tm_sec / 60.0);
    if(seconds_outside) {
        draw_ring(layer, ctx, angle, MAX_RADIUS);
    } else {
        draw_ring(layer, ctx, angle, MAX_RADIUS - RING_WIDTH * 2 - GAP * 2);
    }
    snapshot(ctx);

    // Minute
    angle = TRIG_MAX_ANGLE * (tick_time->tm_min / 60.0);
    draw_ring(layer, ctx, angle, MAX_RADIUS - RING_WIDTH - GAP);

    // Overlay the seconds
    graphics_context_set_compositing_mode(ctx, GCompOpAnd);
    graphics_draw_bitmap_in_rect(ctx, buffer, GRect(0, OFFSET, WIDTH, WIDTH));

    snapshot(ctx);

    // Hour
    angle = TRIG_MAX_ANGLE * ((tick_time->tm_hour % 12) / 12.0);
    if(seconds_outside) {
        draw_ring(layer, ctx, angle, MAX_RADIUS - RING_WIDTH * 2 - GAP * 2);
    } else {
        draw_ring(layer, ctx, angle, MAX_RADIUS);
    }

    // Overlay the seconds and minutes
    graphics_context_set_compositing_mode(ctx, GCompOpAnd);
    graphics_draw_bitmap_in_rect(ctx, buffer, GRect(0, OFFSET, WIDTH, WIDTH));

    // Draw another circle if we've lost bluetooth
    if(!connected) {
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_circle(ctx, centre, RING_WIDTH);
    }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changes) {
    layer_mark_dirty(display_layer);
}

static void app_message_received(DictionaryIterator *iter, void *context) {
    Tuple *tuple = dict_read_first(iter);
    while(tuple) {
        if(tuple->key == KEY_INVERT) {
            if(tuple->value->uint8 == 0) {
                invert = false;
            } else {
                invert = true;
            }

            persist_write_bool(KEY_INVERT, invert);

            layer_set_hidden((Layer*)inverter_layer, !invert);
        } else if(tuple->key == KEY_ORDER) {
            if(tuple->value->uint8 == 0) {
                seconds_outside = true;
            } else {
                seconds_outside = false;
            }

            persist_write_bool(KEY_ORDER, seconds_outside);
        }
        tuple = dict_read_next(iter);
    }
}

static void connection_handler(bool is_connected) {
    connected = is_connected;
}

static void init(void) {
    window = window_create();

    const bool animated = true;
    window_stack_push(window, animated);

    // Set a white background
    window_set_background_color(window, GColorWhite);

    // Register tick timer service
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

    // Register bluetooth connection service
    connected = bluetooth_connection_service_peek();
    bluetooth_connection_service_subscribe(connection_handler);

    // Get the root layer
    display_layer = window_get_root_layer(window);
    GRect frame = layer_get_frame(display_layer);
    layer_set_update_proc(display_layer, &display_layer_update);
    
    // Create the inverter layer
    inverter_layer = inverter_layer_create(frame);
    layer_add_child(display_layer, (Layer*)inverter_layer);
    
    // Set up the bitmaps
    buffer = gbitmap_create_blank(GSize(WIDTH, HEIGHT));

    // Message receive
    app_message_register_inbox_received(app_message_received);
    
    // Message dropped
    app_message_register_inbox_dropped(NULL);
    
    // Open inbox
    // TODO: Work out the right size
    app_message_open(APP_MESSAGE_INBOX_SIZE_MINIMUM, APP_MESSAGE_OUTBOX_SIZE_MINIMUM);

    if(!persist_exists(KEY_INVERT)) {
        persist_write_bool(KEY_INVERT, false);
    }

    if(!persist_exists(KEY_ORDER)) {
        persist_write_bool(KEY_ORDER, true);
    }

    invert = persist_read_bool(KEY_INVERT);
    seconds_outside = persist_read_bool(KEY_ORDER);

    // Hide the inverter?
    layer_set_hidden((Layer*)inverter_layer, !invert);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Heap used: %d", heap_bytes_used());
}

static void deinit(void) {
    gbitmap_destroy(buffer);

    inverter_layer_destroy(inverter_layer);

    window_destroy(window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
