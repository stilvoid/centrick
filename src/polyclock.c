#include <pebble.h>

#define PI 3.141592653589793238462643383

static Window *window;
static Layer *background_layer;
static Layer *display_layer;

static GPath *path = NULL;

static GPathInfo path_info = {
    .num_points = 3,
    .points = (GPoint[]) {
        {72, 84},
        {72, 84},
        {72, 84}
    }
};

static GPoint centre = {144 / 2, 168 / 2};
static const int radius = 144 / 2 - 20;
static const int mark_radius = 144 / 2 - 10;
static const int mark_size = 10;

static double angle;

static void background_layer_update(Layer *layer, GContext *ctx) {
    GPoint tick_in;
    GPoint tick_out;

    graphics_context_set_stroke_color(ctx, GColorBlack);

    // Draw some marks
    for(int i=0; i<12; i++) {
        angle = TRIG_MAX_ANGLE * (i / 12.0);

        tick_in.x = centre.x + (sin_lookup(angle) * (mark_radius) / TRIG_MAX_RATIO);
        tick_in.y = centre.y - (cos_lookup(angle) * (mark_radius) / TRIG_MAX_RATIO);

        tick_out.x = centre.x + (sin_lookup(angle) * (mark_radius - mark_size) / TRIG_MAX_RATIO);
        tick_out.y = centre.y - (cos_lookup(angle) * (mark_radius - mark_size) / TRIG_MAX_RATIO);
    
        graphics_draw_line(ctx, tick_in, tick_out);
    }
}

static void display_layer_update(Layer *layer, GContext *ctx) {
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);

    // Second
    angle = TRIG_MAX_ANGLE * (tick_time->tm_sec / 60.0);
    path_info.points[0].x = centre.x + (sin_lookup(angle) * radius / TRIG_MAX_RATIO);
    path_info.points[0].y = centre.y - (cos_lookup(angle) * radius / TRIG_MAX_RATIO);
    
    // Minute
    angle = TRIG_MAX_ANGLE * (tick_time->tm_min / 60.0);
    path_info.points[1].x = centre.x + (sin_lookup(angle) * radius / TRIG_MAX_RATIO);
    path_info.points[1].y = centre.y - (cos_lookup(angle) * radius / TRIG_MAX_RATIO);

    // Hour
    angle = TRIG_MAX_ANGLE * ((tick_time->tm_hour % 12) / 12.0);
    path_info.points[2].x = centre.x + (sin_lookup(angle) * radius / TRIG_MAX_RATIO);
    path_info.points[2].y = centre.y - (cos_lookup(angle) * radius / TRIG_MAX_RATIO);

    // Do some drawing
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_fill_color(ctx, GColorBlack);

    // Draw the poly
    path = gpath_create(&path_info);
    gpath_draw_filled(ctx, path);
    gpath_destroy(path);

    // Draw some dots
    graphics_fill_circle(ctx, path_info.points[0], 1);
    graphics_fill_circle(ctx, path_info.points[1], 2);
    graphics_fill_circle(ctx, path_info.points[2], 3);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changes) {
    layer_mark_dirty(display_layer);
}

static void init(void) {
    window = window_create();

    const bool animated = true;
    window_stack_push(window, animated);

    // Set a black background
    window_set_background_color(window, GColorWhite);

    // Register tick timer service
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

    // Get the root layer
    Layer *root_layer = window_get_root_layer(window);
    GRect frame = layer_get_frame(root_layer);
    
    // Create a layer for the marks
    background_layer = layer_create(frame);
    layer_set_update_proc(background_layer, &background_layer_update);
    layer_add_child(root_layer, background_layer);

    // Create a layer for the poly
    display_layer = layer_create(frame);
    layer_set_update_proc(display_layer, &display_layer_update);
    layer_add_child(background_layer, display_layer);
}

static void deinit(void) {
    layer_destroy(display_layer);
    layer_destroy(background_layer);
    window_destroy(window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
