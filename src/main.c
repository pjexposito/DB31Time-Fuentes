// Gráficos de los iconos adaptados de http://icons.primail.ch/index.php?page=iconset_weather
// Código del tiempo adaptado de https://github.com/Niknam/futura-weather-sdk2.0.
// Lo anteriormente mencionado ya no se usa en esta version
// Código del reloj basado en https://github.com/orviwan/91-Dub-v2.0
// Con código de https://ninedof.wordpress.com/2014/05/24/pebble-sdk-2-0-tutorial-9-app-configuration/


// UUID para versión en español: "11d1527e-985b-4844-bc10-34ede0ee9caf"
// UUID para versión en inglés:  "a8336799-b197-4e3d-8afd-3290317c65b2"



#include "pebble.h"
  
#define KEY_IDIOMA 0
#define KEY_VIBE 1
#define KEY_DATEFORMAT 2
#define KEY_SEGUNDOS 3
#define KEY_HOURLYVIBE 4  
  
static Window *window;
static Layer *window_layer;
static uint8_t batteryPercent;
static TextLayer *text_layer_hora, *text_layer_segundos, *text_layer_fecha1, *text_layer_fecha2, *text_layer_letras, *text_layer_ano, *text_layer_bateria;


int IDIOMA;  
// IDIOMA = 1, texto en español
// IDIOMA = 0, text in english  
  
bool DATEFORMAT;
// DATEFORMAT = 1, Formato europeo (DD/MM/AAAA)
// DATEFORMAT = 0, Formato americano (MM/DD/AAAA)
  

// Vibra al perder la conexión BT  
int BluetoothVibe;

// Vibrar en el cambio de hora
static int HourlyVibe;

// Si es 1 se muestra el segundero. Si es 0, desaparece.
static int SEGUNDOS;

static bool appStarted = false;

static GBitmap *background_image;
static BitmapLayer *background_layer;

static GBitmap *meter_bar_image;
static BitmapLayer *meter_bar_layer;

static GBitmap *bluetooth_image;
static BitmapLayer *bluetooth_layer;

static GBitmap *porcentaje_image;
static BitmapLayer *porcentaje_layer;

static GBitmap *battery_image;
static BitmapLayer *battery_image_layer;
static BitmapLayer *battery_layer;

static GBitmap *time_format_image;
static GBitmap *time_format_image24;

static BitmapLayer *time_format_layer;




static void carga_preferencias(void)
  { 
    // Carga las preferencias
    IDIOMA = persist_exists(KEY_IDIOMA) ? persist_read_int(KEY_IDIOMA) : 0;
    DATEFORMAT = persist_exists(KEY_DATEFORMAT) ? persist_read_bool(KEY_DATEFORMAT) : 0;
    BluetoothVibe = persist_exists(KEY_VIBE) ? persist_read_int(KEY_VIBE) : 1;
    SEGUNDOS = persist_exists(KEY_SEGUNDOS) ? persist_read_int(KEY_SEGUNDOS) : 1;
    HourlyVibe = persist_exists(KEY_HOURLYVIBE) ? persist_read_int(KEY_HOURLYVIBE) : 0;
  }

static void handle_tick(struct tm *tick_time, TimeUnits units_changed);


static void in_recv_handler(DictionaryIterator *iterator, void *context)
  {
  //Recibe los datos de configuración
  Tuple *key_idioma_tuple = dict_find(iterator, KEY_IDIOMA);
  Tuple *key_vibe_tuple = dict_find(iterator, KEY_VIBE);
  Tuple *key_dateformat_tuple = dict_find(iterator, KEY_DATEFORMAT);
  Tuple *key_segundos_tuple = dict_find(iterator, KEY_SEGUNDOS);
  Tuple *key_hourlyvibe_tuple = dict_find(iterator, KEY_HOURLYVIBE);

  if(strcmp(key_idioma_tuple->value->cstring, "spanish") == 0)
    persist_write_int(KEY_IDIOMA, 1);
  else if(strcmp(key_idioma_tuple->value->cstring, "english") == 0)
    persist_write_int(KEY_IDIOMA, 0);
 
  if(strcmp(key_dateformat_tuple->value->cstring, "DDMM") == 0)
      persist_write_bool(KEY_DATEFORMAT, 1);
  else if(strcmp(key_dateformat_tuple->value->cstring, "MMDD") == 0)
    persist_write_bool(KEY_DATEFORMAT, 0);  
    
  if(strcmp(key_vibe_tuple->value->cstring, "on") == 0)
      persist_write_int(KEY_VIBE, 1);
  else if(strcmp(key_vibe_tuple->value->cstring, "off") == 0)
      persist_write_int(KEY_VIBE, 0); 
    
  if(strcmp(key_segundos_tuple->value->cstring, "on") == 0)
      persist_write_int(KEY_SEGUNDOS, 1);
  else if(strcmp(key_segundos_tuple->value->cstring, "off") == 0)
      persist_write_int(KEY_SEGUNDOS, 0);     
    
  if(strcmp(key_hourlyvibe_tuple->value->cstring, "on") == 0)
     persist_write_int(KEY_HOURLYVIBE, 1);
  else if(strcmp(key_hourlyvibe_tuple->value->cstring, "off") == 0)
     persist_write_int(KEY_HOURLYVIBE, 0);
  
  // Vuelve a dibujar el reloj tras cerrar las preferencias
  carga_preferencias();
  
  if(SEGUNDOS)
  {
    tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
    // OBSOLETO layer_set_hidden(bitmap_layer_get_layer(seg_digits_layers[0]), false);
    // OBSOLETO layer_set_hidden(bitmap_layer_get_layer(seg_digits_layers[1]), false);    
  }
  else
  {
    // OBSOLETO layer_set_hidden(bitmap_layer_get_layer(seg_digits_layers[0]), true);
    // OBSOLETO layer_set_hidden(bitmap_layer_get_layer(seg_digits_layers[1]), true);    
    tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
  }
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);  
  handle_tick(tick_time, YEAR_UNIT + MONTH_UNIT + DAY_UNIT + HOUR_UNIT + MINUTE_UNIT + SECOND_UNIT);

}


void change_battery_icon(bool charging) {
  gbitmap_destroy(battery_image);
  if(charging) {
    battery_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_CHARGE);
  }
  else {
    battery_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY);
  }  
  bitmap_layer_set_bitmap(battery_image_layer, battery_image);
  layer_mark_dirty(bitmap_layer_get_layer(battery_image_layer));
}



static void update_battery(BatteryChargeState charge_state) {
  batteryPercent = charge_state.charge_percent;
  if(batteryPercent==100) 
  {
	  // Aqui la batería está al 100%
    return;
  }
  layer_set_hidden(bitmap_layer_get_layer(battery_layer), charge_state.is_charging);
  change_battery_icon(charge_state.is_charging);  

  static char s_time_text[] = "00";
  snprintf(s_time_text, sizeof(s_time_text), "%i", charge_state.charge_percent);
  text_layer_set_text(text_layer_bateria, s_time_text); 
  // OBSOLETO set_container_image(&battery_percent_image[0], battery_percent_layers[0], TINY_IMAGE_RESOURCE_IDS[charge_state.charge_percent/10], GPoint(13, 41));
  // OBSOLETO set_container_image(&battery_percent_image[1], battery_percent_layers[1], TINY_IMAGE_RESOURCE_IDS[charge_state.charge_percent%10], GPoint(20, 41));
  // OBSOLETO set_container_image(&battery_percent_image[2], battery_percent_layers[2], TINY_IMAGE_RESOURCE_IDS[10], GPoint(27, 41));
}

static void toggle_bluetooth_icon(bool connected) {
  if(appStarted && !connected && BluetoothVibe) {
    static uint32_t const segments[] = { 200, 100, 100, 100, 500 };
    VibePattern pat = {
      .durations = segments,
      .num_segments = ARRAY_LENGTH(segments),
      };
    vibes_enqueue_custom_pattern(pat);
    }
  layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer), !connected);
}

void bluetooth_connection_callback(bool connected) {
  toggle_bluetooth_icon(connected);
}

void battery_layer_update_callback(Layer *me, GContext* ctx) {        
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(2, 2, ((batteryPercent/100.0)*11.0), 3), 0, GCornerNone);
}


static void update_days(struct tm *tick_time) {
  const char *dias_es[] = {"DOM", "LUN", "MAR", "MIE", "JUE", "VIE", "SAB"};
  const char *dias_en[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  
  if (IDIOMA==1)
  	  text_layer_set_text(text_layer_letras, dias_es[tick_time->tm_wday]); 
  else
  	  text_layer_set_text(text_layer_letras, dias_en[tick_time->tm_wday]); 

  if (DATEFORMAT==1)
    {
  	  static char s_time_text[] = "00";
  	  strftime(s_time_text, sizeof(s_time_text), "%d", tick_time);
  	  text_layer_set_text(text_layer_fecha1, s_time_text); 
    }
  else
    {
  	  static char s_time_text[] = "00";
  	  strftime(s_time_text, sizeof(s_time_text), "%d", tick_time);
  	  text_layer_set_text(text_layer_fecha2, s_time_text);  
    }  
}

static void update_months(struct tm *tick_time) {
  if (DATEFORMAT==1)
  {	  
	  static char s_time_text[] = "00";
	  strftime(s_time_text, sizeof(s_time_text), "%m", tick_time);
	  text_layer_set_text(text_layer_fecha2, s_time_text);
  }
  else
  {
	  static char s_time_text[] = "00";
	  strftime(s_time_text, sizeof(s_time_text), "%m", tick_time);
	  text_layer_set_text(text_layer_fecha1, s_time_text); 
  }  
    
}

static void update_years(struct tm *tick_time) {
  static char s_time_text[] = "00";
  strftime(s_time_text, sizeof(s_time_text), "%y", tick_time);
  text_layer_set_text(text_layer_ano, s_time_text);
}

static void update_hours(struct tm *tick_time) {
  if(appStarted && HourlyVibe) {
    vibes_short_pulse();
  }
  if (!clock_is_24h_style()) {
    if (tick_time->tm_hour >= 12) {
          bitmap_layer_set_bitmap(time_format_layer, time_format_image); 

      layer_set_hidden(bitmap_layer_get_layer(time_format_layer), false);
    } 
    else {
      layer_set_hidden(bitmap_layer_get_layer(time_format_layer), true);
    }
}
}

static void update_minutes(struct tm *tick_time) {
    static char s_time_text[] = "00:00";
    if (clock_is_24h_style()) 
	    strftime(s_time_text, sizeof(s_time_text), "%R:%M", tick_time);
     else 
	     strftime(s_time_text, sizeof(s_time_text), "%I:%M", tick_time);
    text_layer_set_text(text_layer_hora, s_time_text);
}

static void update_seconds(struct tm *tick_time) {
  if (SEGUNDOS==1)
  {
	  static char s_time_text[] = "00";
	  strftime(s_time_text, sizeof(s_time_text), "%S", tick_time);
	  text_layer_set_text(text_layer_segundos, s_time_text);
  }
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  if (units_changed & YEAR_UNIT) update_years(tick_time);
  if (units_changed & MONTH_UNIT) update_months(tick_time);
  if (units_changed & DAY_UNIT) update_days(tick_time);
  if (units_changed & HOUR_UNIT) update_hours(tick_time);
  if (units_changed & MINUTE_UNIT) update_minutes(tick_time);
  if (units_changed & SECOND_UNIT) update_seconds(tick_time);
}


static void init(void) {
  
  carga_preferencias();
  
  window = window_create();
  if (window == NULL) {
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "OOM: couldn't allocate window");
      return;
  }
  window_stack_push(window, true /* Animated */);
  window_layer = window_get_root_layer(window);
  
  GFont fuente_hora = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FUENTE_HORA_64));
  GFont fuente_segundos = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FUENTE_SEGUNDOS_32));
  GFont fuente_fecha = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FUENTE_FECHA_32));
  GFont fuente_letras = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FUENTE_LETRAS_24));
  GFont fuente_bateria = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FUENTE_BATERIA_8));
  

	
  background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  background_layer = bitmap_layer_create(layer_get_frame(window_layer));
  bitmap_layer_set_bitmap(background_layer, background_image);
  layer_add_child(window_layer, bitmap_layer_get_layer(background_layer));

  
  
  
  porcentaje_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TINY_PERCENT);
  #ifdef PBL_PLATFORM_BASALT
    GRect bitmap_bounds0 = gbitmap_get_bounds(porcentaje_image);
  #else
    GRect bitmap_bounds0 = porcentaje_image->bounds;
  #endif
  GRect frame0 = GRect(27, 41, bitmap_bounds0.size.w, bitmap_bounds0.size.h);
  
  porcentaje_layer = bitmap_layer_create(frame0);
  bitmap_layer_set_bitmap(porcentaje_layer, porcentaje_image);
  layer_add_child(window_layer, bitmap_layer_get_layer(porcentaje_layer));   


    meter_bar_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_METER_BAR);
    #ifdef PBL_PLATFORM_BASALT
      GRect bitmap_bounds2 = gbitmap_get_bounds(meter_bar_image);
    #else
      GRect bitmap_bounds2 = meter_bar_image->bounds;
    #endif
    GRect frame2 = GRect(13, 70, bitmap_bounds2.size.w, bitmap_bounds2.size.h);
    meter_bar_layer = bitmap_layer_create(frame2);
    bitmap_layer_set_bitmap(meter_bar_layer, meter_bar_image);
    if (BluetoothVibe == 1)
      layer_add_child(window_layer, bitmap_layer_get_layer(meter_bar_layer)); 

  bluetooth_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH);
  #ifdef PBL_PLATFORM_BASALT
    GRect bitmap_bounds3 = gbitmap_get_bounds(bluetooth_image);
  #else
    GRect bitmap_bounds3 = bluetooth_image->bounds;
  #endif
  GRect frame3 = GRect(29, 70, bitmap_bounds3.size.w, bitmap_bounds3.size.h);
  bluetooth_layer = bitmap_layer_create(frame3);
  bitmap_layer_set_bitmap(bluetooth_layer, bluetooth_image);
  layer_add_child(window_layer, bitmap_layer_get_layer(bluetooth_layer));

  battery_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY);
  
  #ifdef PBL_PLATFORM_BASALT
    GRect bitmap_bounds4 = gbitmap_get_bounds(battery_image);
  #else
    GRect bitmap_bounds4 = battery_image->bounds;
  #endif
  GRect frame4 = GRect(13, 51, bitmap_bounds4.size.w, bitmap_bounds4.size.h);
  battery_layer = bitmap_layer_create(frame4);
  battery_image_layer = bitmap_layer_create(frame4);
  bitmap_layer_set_bitmap(battery_image_layer, battery_image);
  layer_set_update_proc(bitmap_layer_get_layer(battery_layer), battery_layer_update_callback);
  
  layer_add_child(window_layer, bitmap_layer_get_layer(battery_image_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(battery_layer));
  
  GRect frame5 = (GRect) {
    .origin = { .x = 13, .y = 83 },
    .size = {.w = 19, .h = 8}
  };
  time_format_layer = bitmap_layer_create(frame5);
  time_format_image24 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_24_HOUR_MODE);
  time_format_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PM_MODE);

  if (clock_is_24h_style()) {
    time_format_image24 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_24_HOUR_MODE);
    bitmap_layer_set_bitmap(time_format_layer, time_format_image24); 
  }
  layer_add_child(window_layer, bitmap_layer_get_layer(time_format_layer));
  
  
 
  // EMPIEZAN LOS CARACTERES
  
  text_layer_hora = text_layer_create((GRect) { .origin = { 7, 69 }, .size = { 100, 70 } });
  text_layer_set_font(text_layer_hora, fuente_hora);
  text_layer_set_background_color(text_layer_hora, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(text_layer_hora));
  
  text_layer_segundos = text_layer_create((GRect) { .origin = { 107, 106 }, .size = { 40, 40 } });
  text_layer_set_font(text_layer_segundos, fuente_segundos);
  text_layer_set_background_color(text_layer_segundos, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(text_layer_segundos));
  
  text_layer_fecha1 = text_layer_create((GRect) { .origin = { 72, 62}, .size = { 40, 40 } });
  text_layer_set_font(text_layer_fecha1, fuente_fecha);
  text_layer_set_background_color(text_layer_fecha1, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(text_layer_fecha1));
  
  text_layer_fecha2 = text_layer_create((GRect) { .origin = { 110, 62}, .size = { 40, 40 } });
  text_layer_set_font(text_layer_fecha2, fuente_fecha);
  text_layer_set_background_color(text_layer_fecha2, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(text_layer_fecha2));
    
  
  text_layer_letras = text_layer_create((GRect) { .origin = { 41, 33}, .size = { 70, 40 } });
  text_layer_set_font(text_layer_letras, fuente_letras);
  text_layer_set_background_color(text_layer_letras, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(text_layer_letras));
  
  text_layer_ano = text_layer_create((GRect) { .origin = { 102, 33}, .size = { 70, 40 } });
  text_layer_set_font(text_layer_ano, fuente_letras);
  text_layer_set_background_color(text_layer_ano, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(text_layer_ano));
  
  text_layer_bateria = text_layer_create((GRect) { .origin = { 14, 41}, .size = { 30, 30 } });
  text_layer_set_font(text_layer_bateria, fuente_bateria);
  text_layer_set_background_color(text_layer_bateria, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(text_layer_bateria));
  
  
  // AQUI ACABAN LOS CARACTERES
 
    
  toggle_bluetooth_icon(bluetooth_connection_service_peek());
  update_battery(battery_state_service_peek());

  appStarted = true;
  
  // Avoids a blank screen on watch start.
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);  
  handle_tick(tick_time, YEAR_UNIT + MONTH_UNIT + DAY_UNIT + HOUR_UNIT + MINUTE_UNIT + SECOND_UNIT);

  tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
  
  
  if(SEGUNDOS)
    tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
  else
    tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);

  bluetooth_connection_service_subscribe(bluetooth_connection_callback);
  battery_state_service_subscribe(&update_battery);
  app_message_register_inbox_received((AppMessageInboxReceived) in_recv_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

}


static void deinit(void) {

  tick_timer_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();

  layer_remove_from_parent(bitmap_layer_get_layer(background_layer));
  bitmap_layer_destroy(background_layer);
  gbitmap_destroy(background_image);
  background_image = NULL;
  
  layer_remove_from_parent(bitmap_layer_get_layer(meter_bar_layer));
  bitmap_layer_destroy(meter_bar_layer);
  gbitmap_destroy(meter_bar_image);
  background_image = NULL;
	
  layer_remove_from_parent(bitmap_layer_get_layer(bluetooth_layer));
  bitmap_layer_destroy(bluetooth_layer);
  gbitmap_destroy(bluetooth_image);
  bluetooth_image = NULL;
	
  layer_remove_from_parent(bitmap_layer_get_layer(battery_layer));
  bitmap_layer_destroy(battery_layer);
  gbitmap_destroy(battery_image);
  battery_image = NULL;
	
  layer_remove_from_parent(bitmap_layer_get_layer(battery_image_layer));
  bitmap_layer_destroy(battery_image_layer);

  layer_remove_from_parent(bitmap_layer_get_layer(time_format_layer));
  bitmap_layer_destroy(time_format_layer);
  gbitmap_destroy(time_format_image);
  time_format_image = NULL;
	
  text_layer_destroy(text_layer_hora);
  text_layer_destroy(text_layer_segundos);
  text_layer_destroy(text_layer_fecha1);
  text_layer_destroy(text_layer_letras);
  text_layer_destroy(text_layer_ano);
  text_layer_destroy(text_layer_fecha2);
  text_layer_destroy(text_layer_bateria);

	
  layer_remove_from_parent(window_layer);
  layer_destroy(window_layer);
	
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}