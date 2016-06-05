#include "pebble.h"
#include "stddef.h"
#include "string.h"
  
#define GColorWhiteInit { GColorWhiteARGB8 }
#define GColorBlackInit { GColorBlackARGB8 }
#define GColorDarkCandyAppleRedInit { GColorDarkCandyAppleRedARGB8 }

// global window variables
// ANYTHING THAT IS CALLED BY PEBBLE API HAS TO BE NOT STATIC
// PEBBLE BLUETOOTH API FLAGS ARE BOOL FLAGS
// ALL OTHER FLAGS ARE DECLARED AS UINT8_T; FALSE = 100; TRUE = 111
// PEBBLE OUTAGES - IN THIS ORDER OR PRECEDENCE
// BLUETOOTH - CHECK PHONE - CHECK RIG/CGM
// IF CHECKING FOR A SPECIFIC OUTAGE, YOU HAVE TO CHECK FOR ALL HIGHER CONDITIONS IN OUTAGE HIERARCHY
  
Window *window_cgm = NULL;
Layer *window_layer_cgm = NULL;
Layer *rectangle_layers = NULL;

TextLayer *bg_layer = NULL;
TextLayer *cgmtime_layer = NULL;
TextLayer *message_layer = NULL;    // BG DELTA & MESSAGE LAYER
TextLayer *rig_battlevel_layer = NULL;
TextLayer *watch_battlevel_layer = NULL;
TextLayer *t1dname_layer = NULL;
TextLayer *time_watch_digi_layer = NULL;
TextLayer *time_watch_classic_layer = NULL;
TextLayer *date_app_layer = NULL;
TextLayer *happymsg_layer = NULL;
TextLayer *raw_calc_layer = NULL;
TextLayer *raw_unfilt_layer = NULL;
TextLayer *noise_layer = NULL;
TextLayer *calcraw_last1_digi_layer = NULL;
TextLayer *calcraw_last2_digi_layer = NULL;
TextLayer *calcraw_last3_digi_layer = NULL;
TextLayer *calcraw_last1_classic_layer = NULL;
TextLayer *calcraw_last2_classic_layer = NULL;
TextLayer *calcraw_last3_classic_layer = NULL;

BitmapLayer *bg_image_layer = NULL;
BitmapLayer *icon_layer = NULL;
BitmapLayer *cgmicon_layer = NULL;

GBitmap *bg_image_bitmap = NULL;
GBitmap *icon_bitmap = NULL;
GBitmap *cgmicon_bitmap = NULL;
GBitmap *specialvalue_bitmap = NULL;

PropertyAnimation *happymsg_animation = NULL;

static char time_watch_text[] = "00:00";
static char date_app_text[] = "Aug 26 Sat";

// variables for AppSync
AppSync sync_cgm;
uint8_t AppSyncErrAlert = 100;

// CGM message is 135 bytes
// Dictionary calculation 1 + 11x7 (11 tuplets, header) + 89 bytes (data) = 167 bytes; pad with 33 bytes
// See dict_calc_buffer_size()
static uint8_t sync_buffer_cgm[200];

// variables for timers and time
AppTimer *timer_cgm = NULL;
AppTimer *BT_timer = NULL;
time_t cgm_time_now = 0;
time_t app_time_now = 0;
int timeformat = 0;

// global variable for bluetooth connection
bool bluetooth_connected_cgm = true;

// global variables for sync tuple functions
char current_icon[2] = {0};
char last_bg[6] = {0};
char last_battlevel[4] = {0};

uint32_t current_cgm_time = 0;
uint32_t stored_cgm_time = 0;
uint32_t current_cgm_timeago = 0;
uint8_t init_loading_cgm_timeago = 111;
char cgm_label_buffer[6] = {0};

// global variable for single state machine
// sometimes we have to know we have cleared an outage, but the outage flags
// have not been cleared in their single states; this is for that condition
uint8_t ClearedOutage = 100;
uint8_t ClearedBTOutage = 100;

uint32_t current_app_time = 0;
char current_bg_delta[10] = {0};
char last_calc_raw[6] = {0};
char last_raw_unfilt[6] = {0};
uint8_t current_noise_value = 0;
char last_calc_raw1[6] = {0};
char last_calc_raw2[6] = {0};
char last_calc_raw3[6] = {0};
int current_bg = 0;
int current_uraw = 0;
int current_calc_raw = 0;
int current_calc_raw1 = 0;
int current_calc_raw2 = 0;
int current_calc_raw3 = 0;
uint8_t currentBG_isMMOL = 100;
int converted_bgDelta = 0;
char current_values[60] = {0};
uint8_t HaveCalcRaw = 100;
uint8_t HaveURaw = 100;
char current_t1dname[25] ={0};
int batterydisplay_value = 0;
int feeling_value = 0;
int color_value = 0;
int mainfont_value = 0;
GColor top_color = GColorWhiteInit;
GColor bottom_color = GColorWhiteInit;
GColor topleft_color = GColorWhiteInit;
GColor bottomleft_color = GColorWhiteInit;
GColor topright_color = GColorWhiteInit;
GColor bottomright_color = GColorWhiteInit;
GColor text_default_color = GColorBlackInit;
GColor text_warning_color = GColorDarkCandyAppleRedInit;
GColor text_urgent_color = GColorDarkCandyAppleRedInit;
static uint8_t DoubleDownAlert = 100;

// global BG snooze timer
static uint8_t lastAlertTime = 0;

// global special value alert
static uint8_t specvalue_alert = 100;

// global overwrite variables for vibrating when hit a specific BG if already in a snooze
static uint8_t specvalue_overwrite = 100;
static uint8_t hypolow_overwrite = 100;
static uint8_t biglow_overwrite = 100;
static uint8_t midlow_overwrite = 100;
static uint8_t low_overwrite = 100;
static uint8_t high_overwrite = 100;
static uint8_t midhigh_overwrite = 100;
static uint8_t bighigh_overwrite = 100;

// global retries counters for timeout problems
static uint8_t appsyncandmsg_retries_counter = 0;
static uint8_t dataoffline_retries_counter = 0;

// global variables for vibrating in special conditions
static uint8_t BluetoothAlert = 100;
static uint8_t BT_timer_pop = 100;
static uint8_t DataOfflineAlert = 100;
static uint8_t CGMOffAlert = 100;
static uint8_t PhoneOffAlert = 100;

// global constants for time durations; seconds
static const uint8_t MINUTEAGO = 60;
static const uint32_t TWOYEARSAGO = 2*365*(24*60*60);
static const uint16_t MS_IN_A_SECOND = 1000;

// Constants for string buffers
// If add month to date, buffer size needs to increase to 12; also need to reformat date_app_text init string
static const uint8_t TIME_TEXTBUFF_SIZE = 6;
static const uint8_t LABEL_BUFFER_SIZE = 6;
static const uint8_t BATTLEVEL_FORMAT_SIZE = 12;

// global Pebble constant for null value
static const int pebbleNullValue1 = 537024512;

// ** START OF CONSTANTS THAT CAN BE CHANGED; DO NOT CHANGE IF YOU DO NOT KNOW WHAT YOU ARE DOING **
// ** FOR MMOL, ALL VALUES ARE STORED AS INTEGER; LAST DIGIT IS USED AS DECIMAL **
// ** BE EXTRA CAREFUL OF CHANGING SPECIAL VALUES OR TIMERS; DO NOT CHANGE WITHOUT EXPERT HELP **

// FOR BG RANGES
// DO NOT SET ANY BG RANGES EQUAL TO ANOTHER; LOW CAN NOT EQUAL MIDLOW
// LOW BG RANGES MUST BE IN ASCENDING ORDER; SPECVALUE < HYPOLOW < BIGLOW < MIDLOW < LOW
// HIGH BG RANGES MUST BE IN ASCENDING ORDER; HIGH < MIDHIGH < BIGHIGH
// DO NOT ADJUST SPECVALUE UNLESS YOU HAVE A VERY GOOD REASON
// DO NOT USE NEGATIVE NUMBERS OR DECIMAL POINTS OR ANYTHING OTHER THAN A NUMBER

// BG Ranges, MG/DL
uint16_t SPECVALUE_BG_MGDL = 20;
uint16_t SHOWLOW_BG_MGDL = 40;
uint16_t HYPOLOW_BG_MGDL = 55;
uint16_t BIGLOW_BG_MGDL = 60;
uint16_t MIDLOW_BG_MGDL = 70;
uint16_t LOW_BG_MGDL = 80;

uint16_t HIGH_BG_MGDL = 180;
uint16_t MIDHIGH_BG_MGDL = 240;
uint16_t BIGHIGH_BG_MGDL = 300;
uint16_t SHOWHIGH_BG_MGDL = 400;

// BG Ranges, MMOL
// VALUES ARE IN INT, NOT FLOATING POINT, LAST DIGIT IS DECIMAL
// FOR EXAMPLE : SPECVALUE IS 1.1, BIGHIGH IS 16.6
// ALWAYS USE ONE AND ONLY ONE DECIMAL POINT FOR LAST DIGIT
// GOOD : 5.0, 12.2 // BAD : 7 , 14.44
uint16_t SPECVALUE_BG_MMOL = 11;
uint16_t SHOWLOW_BG_MMOL = 23;
uint16_t HYPOLOW_BG_MMOL = 30;
uint16_t BIGLOW_BG_MMOL = 33;
uint16_t MIDLOW_BG_MMOL = 39;
uint16_t LOW_BG_MMOL = 44;

uint16_t HIGH_BG_MMOL = 100;
uint16_t MIDHIGH_BG_MMOL = 133;
uint16_t BIGHIGH_BG_MMOL = 166;
uint16_t SHOWHIGH_BG_MMOL = 222;

// BG Snooze Times, in Minutes; controls when vibrate again
// RANGE 0-240
uint8_t SPECVALUE_SNZ_MIN = 30;
uint8_t HYPOLOW_SNZ_MIN = 5;
uint8_t BIGLOW_SNZ_MIN = 5;
uint8_t MIDLOW_SNZ_MIN = 10;
uint8_t LOW_SNZ_MIN = 15;
uint8_t HIGH_SNZ_MIN = 30;
uint8_t MIDHIGH_SNZ_MIN = 30;
uint8_t BIGHIGH_SNZ_MIN = 30;
  
// Vibration Levels; 0 = NONE; 1 = LOW; 2 = MEDIUM; 3 = HIGH
// IF YOU DO NOT WANT A SPECIFIC VIBRATION, SET TO 0
uint8_t SPECVALUE_VIBE = 2;
uint8_t HYPOLOWBG_VIBE = 3;
uint8_t BIGLOWBG_VIBE = 3;
uint8_t LOWBG_VIBE = 3;
uint8_t HIGHBG_VIBE = 2;
uint8_t BIGHIGHBG_VIBE = 2;
uint8_t DOUBLEDOWN_VIBE = 3;
uint8_t APPSYNC_ERR_VIBE = 1;
uint8_t BTOUT_VIBE = 1;
uint8_t CGMOUT_VIBE = 1;
uint8_t PHONEOUT_VIBE = 1;
uint8_t LOWBATTERY_VIBE = 1;
uint8_t DATAOFFLINE_VIBE = 1;

// Icon Cross Out & Vibrate Once Wait Times, in Minutes
// RANGE 0-240
// IF YOU WANT TO WAIT LONGER TO GET CONDITION, INCREASE NUMBER
static const uint8_t CGMOUT_WAIT_MIN = 15;
static const uint8_t CGMOUT_INIT_WAIT_MIN = 7;
static const uint8_t PHONEOUT_WAIT_MIN = 5;

// Control Vibrations
// SPECIAL FLAG TO HARD CODE VIBRATIONS OFF; If you want no vibrations, SET TO 111 (true)
// Use for Sleep Face or anyone else for a custom load
uint8_t HardCodeNoVibrations = 100;

// Control Animations
// SPECIAL FLAG TO HARD CODE ANIMATIONS OFF; If you want no animations, SET TO 111 (true)
// This is for people who want old ones too
// Use for a custom load
uint8_t HardCodeNoAnimations = 100;

// Control Raw data
// If you want to turn off vibrations for calculated raw, set to 111 (true)
uint8_t TurnOffVibrationsCalcRaw = 100;
// If you want to see calculated raw, set to 111 (true)
uint8_t TurnOnCalcRaw = 111;
// If you want to see unfiltered raw, set to 111 (true)
uint8_t TurnOnUnfilteredRaw = 111;

// ** END OF CONSTANTS THAT CAN BE CHANGED; DO NOT CHANGE IF YOU DO NOT KNOW WHAT YOU ARE DOING **

// Control Vibrations for Config File
// IF YOU WANT NO VIBRATIONS, SET TO 111 (true)
uint8_t TurnOffAllVibrations = 100;
// IF YOU WANT LESS INTENSE VIBRATIONS, SET TO 111 (true)
uint8_t TurnOffStrongVibrations = 100;

// Bluetooth Timer Wait Time, in Seconds
// RANGE 0-240
// THIS IS ONLY FOR BAD BLUETOOTH CONNECTIONS
// TRY EXTENDING THIS TIME TO SEE IF IT WILL HELP SMOOTH CONNECTION
// CGM DATA RECEIVED EVERY 60 SECONDS, GOING BEYOND THAT MAY RESULT IN MISSED DATA
static const uint8_t BT_ALERT_WAIT_SECS = 45;

// Message Timer & Animate Wait Times, in Seconds
static const uint8_t WATCH_MSGSEND_SECS = 60;
static const uint8_t LOADING_MSGSEND_SECS = 10;
static const uint8_t HAPPYMSG_ANIMATE_SECS = 10;

// App Sync / Message retries, for timeout / busy problems
// Change to see if there is a temp or long term problem
// This is approximately number of seconds, so if set to 50, timeout is at 50 seconds
// However, this can vary widely - can be up to 6 seconds .... for 50, timeout can be up to 3 minutes
// ON NEW SDKS - THIS CAN BE MINUTES - SHOULD NOT BE LONGER THAN 14 NOW.
static const uint8_t APPSYNCANDMSG_RETRIES_MAX = 9;

// HTML Request retries, for timeout / busy problems
// Change to see if there is a temp or long term problem
// This is number of minutes, so if set to 11 timeout is at 11 minutes
static const uint8_t DATAOFFLINE_RETRIES_MAX = 14;

enum CgmKey {
	CGM_ICON_KEY = 0x0,		// TUPLE_CSTRING, MAX 2 BYTES (10)
	CGM_BG_KEY = 0x1,		// TUPLE_CSTRING, MAX 4 BYTES (253 OR 22.2)
	CGM_TCGM_KEY = 0x2,		// TUPLE_INT, 4 BYTES (CGM TIME)
	CGM_TAPP_KEY = 0x3,		// TUPLE_INT, 4 BYTES (APP / PHONE TIME)
	CGM_DLTA_KEY = 0x4,		// TUPLE_CSTRING, MAX 5 BYTES (BG DELTA, -100 or -10.0)
	CGM_UBAT_KEY = 0x5,		// TUPLE_CSTRING, MAX 3 BYTES (UPLOADER BATTERY, 100)
	CGM_NAME_KEY = 0x6,		// TUPLE_CSTRING, MAX 12 BYTES (ChristineRox)
	CGM_VALS_KEY = 0x7,   // TUPLE_CSTRING, MAX 25 BYTES (0,000,000,000,000,0,0,0,0,0,0,0,0,0,0,0,0,0)
	CGM_CLRW_KEY = 0x8,   // TUPLE_CSTRING, MAX 4 BYTES (253 OR 22.2)
	CGM_RWUF_KEY = 0x9,   // TUPLE_CSTRING, MAX 4 BYTES (253 OR 22.2)
	CGM_NOIZ_KEY = 0xA    // TUPLE_INT, 4 BYTES (1-4)
}; 
// TOTAL MESSAGE DATA 4x6+2+5+3+12+43 = 89 BYTES
// TOTAL KEY HEADER DATA (STRINGS) 4x11+2 = 46 BYTES
// TOTAL MESSAGE 135 BYTES

// only special value index we have to declare global
const uint8_t LOGO_SPECVALUE_ICON_INDX = 5;

// ARRAY OF TIMEAGO ICONS
static const uint8_t TIMEAGO_ICONS[] = {
	RESOURCE_ID_IMAGE_RCVRNONE,   //0
	RESOURCE_ID_IMAGE_RCVRON,     //1
	RESOURCE_ID_IMAGE_RCVROFF     //2
};

// INDEX FOR ARRAY OF TIMEAGO ICONS
static const uint8_t RCVRNONE_ICON_INDX = 0;
static const uint8_t RCVRON_ICON_INDX = 1;
static const uint8_t RCVROFF_ICON_INDX = 2;

// START OF ROUTINES

// GLOBAL UTILITY ROUTINES

static char *translate_app_error(AppMessageResult result) {
  switch (result) {
	case APP_MSG_OK: return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY: return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
    case APP_MSG_INVALID_STATE: return "APP_MSG_INVALID_STATE";
    default: return "APP UNKNOWN ERROR";
  }
}

static char *translate_dict_error(DictionaryResult result) {
  switch (result) {
    case DICT_OK: return "DICT_OK";
    case DICT_NOT_ENOUGH_STORAGE: return "DICT_NOT_ENOUGH_STORAGE";
    case DICT_INVALID_ARGS: return "DICT_INVALID_ARGS";
    case DICT_INTERNAL_INCONSISTENCY: return "DICT_INTERNAL_INCONSISTENCY";
    case DICT_MALLOC_FAILED: return "DICT_MALLOC_FAILED";
    default: return "DICT UNKNOWN ERROR";
  }
}

int myBGAtoi(char *str) {

	// VARIABLES
    int res = 0; // Initialize result
	
	// CODE START
 
    // initialize currentBG_isMMOL flag
	currentBG_isMMOL = 100;
	
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "myBGAtoi, START currentBG is MMOL: %i", currentBG_isMMOL );
	
    // Iterate through all characters of input string and update result
    for (int i = 0; str[i] != '\0'; ++i) {
      
	  //APP_LOG(APP_LOG_LEVEL_DEBUG, "myBGAtoi, STRING IN: %s", &str[i] );
	  
        if (str[i] == ('.')) {
        currentBG_isMMOL = 111;
      }
      else if ( (str[i] >= ('0')) && (str[i] <= ('9')) ) {
        res = res*10 + str[i] - '0';
      }
      
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "myBGAtoi, FOR RESULT OUT: %i", res );
	  //APP_LOG(APP_LOG_LEVEL_DEBUG, "myBGAtoi, currentBG is MMOL: %i", currentBG_isMMOL );	  
    }
 //APP_LOG(APP_LOG_LEVEL_DEBUG, "myBGAtoi, FINAL RESULT OUT: %i", res );
    return res;
} // end myBGAtoi



static void null_and_cancel_timer (AppTimer **timer_to_null, bool cancel_timer) {
  
  if (*timer_to_null != NULL) {
    if (cancel_timer) { app_timer_cancel(*timer_to_null); }
    *timer_to_null = NULL;
  }
  
} // null_and_cancel_timer

static void destroy_null_GBitmap(GBitmap **GBmp_image) {
	//APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL GBITMAP: ENTER CODE");
  
  if ( (*GBmp_image != NULL) && (*GBmp_image != (void *)NULL) &&
       ((int)(*GBmp_image) != pebbleNullValue1) ) {
    //APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL GBITMAP: POINTER EXISTS, DESTROY BITMAP IMAGE");
      gbitmap_destroy(*GBmp_image);
      if (*GBmp_image != NULL) {
        //APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL GBITMAP: POINTER EXISTS, SET POINTER TO NULL");
        *GBmp_image = NULL;
      }
	}
  
   //APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL GBITMAP: EXIT CODE");
} // end destroy_null_GBitmap

static void destroy_null_BitmapLayer(BitmapLayer **bmp_layer) {
	//APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL BITMAP: ENTER CODE");
	
	if ( (*bmp_layer != NULL) && (*bmp_layer != (void *)NULL) &&
       ((int)(*bmp_layer) != pebbleNullValue1) ) {
    //APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL BITMAP: POINTER EXISTS, DESTROY BITMAP LAYER");
      bitmap_layer_destroy(*bmp_layer);
      if (*bmp_layer != NULL) {
        //APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL BITMAP: POINTER EXISTS, SET POINTER TO NULL");
        *bmp_layer = NULL;
      }
	}

  //APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL BITMAP: EXIT CODE");
} // end destroy_null_BitmapLayer

static void destroy_null_TextLayer(TextLayer **txt_layer) {
	//APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL TEXT LAYER: ENTER CODE");
	
	if ( (*txt_layer != NULL) && (*txt_layer != (void *)NULL) &&
       ((int)(*txt_layer) != pebbleNullValue1) ) {
    //APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL TEXT LAYER: POINTER EXISTS, DESTROY TEXT LAYER");
      text_layer_destroy(*txt_layer);
      if (*txt_layer != NULL) {
        //APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL TEXT LAYER: POINTER EXISTS, SET POINTER TO NULL");
        *txt_layer = NULL;
      }
	}
//APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL TEXT LAYER: EXIT CODE");
} // end destroy_null_TextLayer

static void destroy_null_Layer(Layer **layer) {
	//APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL LAYER: ENTER CODE");
	
	if ( (*layer != NULL) && (*layer != (void *)NULL) &&
       ((int)(*layer) != pebbleNullValue1) ) {
    //APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL LAYER: POINTER EXISTS, DESTROY TEXT LAYER");
      layer_destroy(*layer);
      if (*layer != NULL) {
        //APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL LAYER: POINTER EXISTS, SET POINTER TO NULL");
        *layer = NULL;
      }
	}
//APP_LOG(APP_LOG_LEVEL_INFO, "DESTROY NULL LAYER: EXIT CODE");
} // end destroy_null_Layer

static void create_update_bitmap(GBitmap **bmp_image, BitmapLayer *bmp_layer, const int resource_id) {
	//APP_LOG(APP_LOG_LEVEL_INFO, " CREATE UPDATE BITMAP: ENTER CODE");
  
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "resource_id: %d", resource_id);
  
	// if Gbitmap pointer exists, destroy and set to NULL
	destroy_null_GBitmap(bmp_image);
  
	// create bitmap and pointer
  //APP_LOG(APP_LOG_LEVEL_INFO, " CREATE UPDATE BITMAP: CREATE BITMAP");
	*bmp_image = gbitmap_create_with_resource(resource_id);
  
	if (*bmp_image == NULL) {
      // couldn't create bitmap, return so don't crash
    //APP_LOG(APP_LOG_LEVEL_INFO, " CREATE UPDATE BITMAP: COULDNT CREATE BITMAP, RETURN");
      return;
	}
	else {
      // set bitmap
    //APP_LOG(APP_LOG_LEVEL_INFO, " CREATE UPDATE BITMAP: SET BITMAP");
      bitmap_layer_set_bitmap(bmp_layer, *bmp_image);
	}
	//APP_LOG(APP_LOG_LEVEL_INFO, " CREATE UPDATE BITMAP: EXIT CODE");
} // end create_update_bitmap

void set_message_layer (char *msg_string, char *msg_buffer, bool use_msg_buffer, GColor msg_color) {
  
  text_layer_set_text_color(message_layer, msg_color);
  
  if (use_msg_buffer) { text_layer_set_text(message_layer, (char *)msg_buffer); }
  else { text_layer_set_text(message_layer, (char *)msg_string); }
  
} // end set_message_layer

void clear_cgm_timeago () {
  
  // erase cgm timeago time
  text_layer_set_text(cgmtime_layer, "");
  init_loading_cgm_timeago = 111;
    
	// erase cgm icon
  create_update_bitmap(&cgmicon_bitmap,cgmicon_layer,TIMEAGO_ICONS[RCVRNONE_ICON_INDX]);
  
} // end clear_cgm_timeago

// HANDLERS

static void alert_handler_cgm(uint8_t alertValue) {
	//APP_LOG(APP_LOG_LEVEL_INFO, "ALERT HANDLER");
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "ALERT CODE: %d", alertValue);
	
	// CONSTANTS
	// constants for vibrations patterns; has to be uint32_t, measured in ms, maximum duration 10000ms
	// Vibe pattern: ON, OFF, ON, OFF; ON for 500ms, OFF for 100ms, ON for 100ms; 
	
	// CURRENT PATTERNS
	const uint32_t highalert_fast[] = { 300,100,50,100,300,100,50,100,300,100,50,100,300,100,50,100,300,100,50,100,300,100,50,100,300,100,50,100,300,100,50,100,300 };
	const uint32_t medalert_long[] = { 500,100,100,100,500,100,100,100,500,100,100,100,500,100,100,100,500 };
	const uint32_t lowalert_beebuzz[] = { 75,50,50,50,75,50,50,50,75,50,50,50,75,50,50,50,75,50,50,50,75,50,50,50,75 };
	
	// PATTERN DURATION
	const uint8_t HIGHALERT_FAST_STRONG = 33;
	const uint8_t HIGHALERT_FAST_SHORT = (33/2);
	const uint8_t MEDALERT_LONG_STRONG = 17;
	const uint8_t MEDALERT_LONG_SHORT = (17/2);
	const uint8_t LOWALERT_BEEBUZZ_STRONG = 25;
	const uint8_t LOWALERT_BEEBUZZ_SHORT = (25/2);
  
	// CODE START
	
	if ( (TurnOffAllVibrations == 111) || (HardCodeNoVibrations == 111) ) {
      //turn off all vibrations is set, return out here
      return;
	}
	
	switch (alertValue) {

	case 0:
      //No alert
      //Normal (new data, in range, trend okay)
      break;
    
	case 1:;
      //Low
      //APP_LOG(APP_LOG_LEVEL_INFO, "ALERT HANDLER: LOW ALERT");
      VibePattern low_alert_pat = {
			.durations = lowalert_beebuzz,
			.num_segments = LOWALERT_BEEBUZZ_STRONG,
      };
	  if (TurnOffStrongVibrations == 111) { low_alert_pat.num_segments = LOWALERT_BEEBUZZ_SHORT; };
      vibes_enqueue_custom_pattern(low_alert_pat);
      break;

	case 2:;
      // Medium Alert
      //APP_LOG(APP_LOG_LEVEL_INFO, "ALERT HANDLER: MEDIUM ALERT");
      VibePattern med_alert_pat = {
			.durations = medalert_long,
			.num_segments = MEDALERT_LONG_STRONG,
      };
	  if (TurnOffStrongVibrations == 111) { med_alert_pat.num_segments = MEDALERT_LONG_SHORT; };
      vibes_enqueue_custom_pattern(med_alert_pat);
      break;

	case 3:;
      // High Alert
      //APP_LOG(APP_LOG_LEVEL_INFO, "ALERT HANDLER: HIGH ALERT");
      VibePattern high_alert_pat = {
			.durations = highalert_fast,
			.num_segments = HIGHALERT_FAST_STRONG,
      };
	  if (TurnOffStrongVibrations == 111) { high_alert_pat.num_segments = HIGHALERT_FAST_SHORT; };
      vibes_enqueue_custom_pattern(high_alert_pat);
      break;
  
	} // switch alertValue
	
} // end alert_handler_cgm

void BT_timer_callback(void *data);

void handle_bluetooth_cgm(bool bt_connected) {
  //APP_LOG(APP_LOG_LEVEL_INFO, "HANDLE BT: ENTER CODE");
  
  if (bt_connected == false) {
  // bluetooth is out  
	
	  // Check BluetoothAlert for extended Bluetooth outage; if so, do nothing
	  if (BluetoothAlert == 111) {
      //Already vibrated and set message; out
	    return;
	  }
	
	  // Check to see if the BT_timer needs to be set; if BT_timer is not null we're still waiting
	  if (BT_timer == NULL) {
	    // check to see if timer has popped
	    if (BT_timer_pop == 100) {
	      //set timer
	      BT_timer = app_timer_register((BT_ALERT_WAIT_SECS*MS_IN_A_SECOND), BT_timer_callback, NULL);
		    // have set timer; next time we come through we will see that the timer has popped
		    return;
	    }
	  }
	  else {
	    // BT_timer is not null and we're still waiting (why? #WEARENOTWAITING)
	    return;
    }
	
	  // timer has popped
	  // Vibrate; BluetoothAlert takes over until Bluetooth connection comes back on
	  //APP_LOG(APP_LOG_LEVEL_INFO, "BT HANDLER: TIMER POP, NO BLUETOOTH, VIBRATE");
    alert_handler_cgm(BTOUT_VIBE);
    BluetoothAlert = 111;
	
	  // Reset timer pop
	  BT_timer_pop = 100;
	
	  //APP_LOG(APP_LOG_LEVEL_INFO, "NO BLUETOOTH");
    //set_message_layer("NO BLUETOOTH\0", "", false, text_urgent_color);
    set_message_layer("NO BLUETOOTH\0", "", false, text_urgent_color);
    
    // clear cgm timeago icon and set init flag
    clear_cgm_timeago();
	
  }
    
  else {
	  // Bluetooth is on, reset BluetoothAlert
    //APP_LOG(APP_LOG_LEVEL_INFO, "HANDLE BT: BLUETOOTH ON");
    if (BluetoothAlert == 111) { 
      ClearedBTOutage = 111; 
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "BT HANDLER, SET CLEARED OUTAGE: %i ", ClearedOutage);
    } 
    BluetoothAlert = 100;
    //ClearedBTOutage = 111; 
    if (BT_timer == NULL) {
      // no timer is set, so need to reset timer pop
      BT_timer_pop = 100;
    }
  }
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "BluetoothAlert: %i", BluetoothAlert);
} // end handle_bluetooth_cgm

void BT_timer_callback(void *data) {
  //APP_LOG(APP_LOG_LEVEL_INFO, "BT TIMER CALLBACK: ENTER CODE");
	
	// reset timer pop and timer
	BT_timer_pop = 111;
  null_and_cancel_timer(&BT_timer, false);
	
	// check bluetooth and call handler
  //handle_bluetooth_cgm(connection_service_peek_pebble_app_connection());
  handle_bluetooth_cgm(bluetooth_connection_service_peek());
	
} // end BT_timer_callback

void handle_watch_battery_cgm(BatteryChargeState watch_charge_state) {

  static char watch_battery_text[8] = {0};
  
  text_layer_set_text_color(watch_battlevel_layer, text_default_color);
  
  if (batterydisplay_value == 1) {
  // want number display
    
    if (watch_charge_state.charge_percent == 100) {
      text_layer_set_text(watch_battlevel_layer, "Pbl GO!\0");
    } 
    else if (watch_charge_state.is_charging) {
      text_layer_set_text(watch_battlevel_layer, "Pbl CH!\0");
      #ifdef PBL_COLOR
      text_layer_set_text_color(watch_battlevel_layer, text_urgent_color); 
      #endif
    } 
    else if ((watch_charge_state.charge_percent > 0) && (watch_charge_state.charge_percent <= 100)) {
      snprintf(watch_battery_text, BATTLEVEL_FORMAT_SIZE, "Pbl %d", watch_charge_state.charge_percent);
      text_layer_set_text(watch_battlevel_layer, watch_battery_text);
      if (watch_charge_state.charge_percent <= 20) {
        #ifdef PBL_COLOR
        text_layer_set_text_color(watch_battlevel_layer, text_urgent_color); 
        #endif
      }
    }
    else {
      text_layer_set_text(watch_battlevel_layer, "Pbl \0");
      #ifdef PBL_COLOR
      text_layer_set_text_color(watch_battlevel_layer, text_urgent_color); 
      #endif
      return;
    }
  }
  
  else {
  // want emoji display
    
  // initialize status to thumbs down emoji
  text_layer_set_text(watch_battlevel_layer, " Pbl \U0001F44E\0");
  
  // two hearts emoji, happy to be charing
  if (watch_charge_state.is_charging) {
    #ifdef PBL_COLOR
    text_layer_set_text_color(watch_battlevel_layer, text_urgent_color); 
    #endif
    text_layer_set_text(watch_battlevel_layer, " Pbl  \U0001F495");
  }
  else if (watch_charge_state.charge_percent == 100) {
    // thumbs up emoji
    text_layer_set_text(watch_battlevel_layer, " Pbl  \U0001F44D\0");
  }
  else if ( (watch_charge_state.charge_percent > 60) && (watch_charge_state.charge_percent < 100) ) {
    // happy face emoji
    text_layer_set_text(watch_battlevel_layer, " Pbl  \U0001F603\0");
  }
  else if ( (watch_charge_state.charge_percent > 40) && (watch_charge_state.charge_percent <= 60) ) {
    // straight line face emoji
    text_layer_set_text(watch_battlevel_layer, " Pbl  \U0001F610\0");
  }
    // sad or tired face emoji
  else if ( (watch_charge_state.charge_percent > 20) && (watch_charge_state.charge_percent <= 40) ) {
    text_layer_set_text(watch_battlevel_layer, " Pbl  \U0001F625\0");
  }  
   // very tired emoji
  else if ( (watch_charge_state.charge_percent > 10) && (watch_charge_state.charge_percent <= 20) ) {
    #ifdef PBL_COLOR
    text_layer_set_text_color(watch_battlevel_layer, text_urgent_color); 
    #endif
    text_layer_set_text(watch_battlevel_layer, " Pbl  \U0001F629\0");
  }  
  else if ( (watch_charge_state.charge_percent > 0) && (watch_charge_state.charge_percent <= 10) ) { 
    // thumbs down emoji
    #ifdef PBL_COLOR
    text_layer_set_text_color(watch_battlevel_layer, text_urgent_color); 
    #endif
    text_layer_set_text(watch_battlevel_layer, " Pbl \U0001F44E\0");
  }
  }
} // end handle_watch_battery_cgm

// format current time from watch
void handle_minute_tick_cgm(struct tm* tick_time_cgm, TimeUnits units_changed_cgm) {
  
  // VARIABLES
  size_t tick_return_cgm = 0;
  
  // CODE START
    
  if (units_changed_cgm & MINUTE_UNIT) {
    //APP_LOG(APP_LOG_LEVEL_INFO, "TICK TIME MINUTE CODE");
    if (timeformat == 0) {
      tick_return_cgm = strftime(time_watch_text, TIME_TEXTBUFF_SIZE, "%l:%M", tick_time_cgm);
    } 
    else {
      tick_return_cgm = strftime(time_watch_text, TIME_TEXTBUFF_SIZE, "%H:%M", tick_time_cgm);
    }
    
	  if (tick_return_cgm != 0) {
      if (mainfont_value == 1) { text_layer_set_text(time_watch_classic_layer, time_watch_text); }
      else { text_layer_set_text(time_watch_digi_layer, time_watch_text); }
    }
	
	  //APP_LOG(APP_LOG_LEVEL_DEBUG, "lastAlertTime IN:  %i", lastAlertTime);
	  // increment BG snooze
    ++lastAlertTime;
	  //APP_LOG(APP_LOG_LEVEL_DEBUG, "lastAlertTime OUT:  %i", lastAlertTime);
	
    // check bluetooth for debugging only
    //bluetooth_connected_cgm = connection_service_peek_pebble_app_connection();
    //bluetooth_connected_cgm = bluetooth_connection_service_peek();
    //if (!bluetooth_connected_cgm) {
      // Bluetooth is out; set BT message
		  //APP_LOG(APP_LOG_LEVEL_INFO, "TICK TIME MINUTE CODE: NO BT, SET NO BT MESSAGE");
        //set_message_layer("NO BLUETOOTH\0", "", false, text_urgent_color);
    //}// if !bluetooth connected
    //APP_LOG(APP_LOG_LEVEL_INFO, "TICK TIME MINUTE CODE: BT TIMER: %s ", (char *)BT_timer);
    
    // check bluetooth
    
    //handle_bluetooth_cgm(connection_service_peek_pebble_app_connection());
    handle_bluetooth_cgm(bluetooth_connection_service_peek());
    
    // check watch battery
    handle_watch_battery_cgm(battery_state_service_peek());
    
    } 
  
} // end handle_minute_tick_cgm

static void draw_date_from_app() {
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "DRAW DATE FROM APP: ENTER CODE");
  
  // CONSTANTS
  const uint8_t DATE_TEXTBUFF_SIZE = 11;
  
  // VARIABLES
  time_t d_app = time(NULL);
  struct tm *current_d_app = localtime(&d_app);
  size_t draw_return = 0;

  // CODE START
  
  // format current date from app
  if (strcmp(time_watch_text, "00:00") == 0) {
      //APP_LOG(APP_LOG_LEVEL_INFO, "TimeFormat: %d", timeformat);
      if (timeformat == 0) {
      //if (timeformat == 0) {
        draw_return = strftime(time_watch_text, TIME_TEXTBUFF_SIZE, "%l:%M", current_d_app);
      } else {
        draw_return = strftime(time_watch_text, TIME_TEXTBUFF_SIZE, "%H:%M", current_d_app);
      }
  
    //APP_LOG(APP_LOG_LEVEL_INFO, "DRAW DATE FROM APP: FORMATTED TIME");
    
	  if (draw_return != 0) {
      if (mainfont_value == 1) {
        text_layer_set_text(time_watch_classic_layer, time_watch_text); 
        //APP_LOG(APP_LOG_LEVEL_INFO, "DRAW DATE FROM APP: LOADED CLASSIC TIME");
      }
      else { 
        text_layer_set_text(time_watch_digi_layer, time_watch_text); 
        //APP_LOG(APP_LOG_LEVEL_INFO, "DRAW DATE FROM APP: LOADED DIGITAL TIME");
      }
      }
	  }
  
  draw_return = strftime(date_app_text, DATE_TEXTBUFF_SIZE, "%a %b %e", current_d_app);
  //draw_return = strftime(date_app_text, DATE_TEXTBUFF_SIZE, " ", current_d_app);
  if (draw_return != 0) {
    text_layer_set_text(date_app_layer, date_app_text);
  }

  //APP_LOG(APP_LOG_LEVEL_INFO, "DRAW DATE FROM APP: EXIT CODE");
  
} // end draw_date_from_app

void sync_error_callback_cgm(DictionaryResult appsync_dict_error, AppMessageResult appsync_error, void *context) {

  // VARIABLES
  DictionaryIterator *iter = NULL;
  AppMessageResult appsync_err_openerr = APP_MSG_OK;
  AppMessageResult appsync_err_senderr = APP_MSG_OK;

  bool bluetooth_connected_syncerror = false;
  
  // CODE START
  
  //bluetooth_connected_syncerror = connection_service_peek_pebble_app_connection();
  bluetooth_connected_syncerror = bluetooth_connection_service_peek();
  if (!bluetooth_connected_syncerror) {
    // bluetooth is out, BT message already set; return out
    return;
  }
  
  // increment app sync retries counter
  appsyncandmsg_retries_counter++;
  
  // if hit max counter, skip resend and flag user
  if (appsyncandmsg_retries_counter < APPSYNCANDMSG_RETRIES_MAX) {
  
    // APPSYNC ERROR debug logs
    //APP_LOG(APP_LOG_LEVEL_INFO, "APP SYNC ERROR");
    APP_LOG(APP_LOG_LEVEL_DEBUG, "APPSYNC ERR, MSG: %i RES: %s DICT: %i RES: %s RETRIES: %i", 
            appsync_error, translate_app_error(appsync_error), appsync_dict_error, translate_dict_error(appsync_dict_error), appsyncandmsg_retries_counter);
  
    // try to resend the message; open app message outbox
    appsync_err_openerr = app_message_outbox_begin(&iter); 
    if (appsync_err_openerr == APP_MSG_OK) {
      // could open app message outbox; send message
      appsync_err_senderr = app_message_outbox_send();
      if (appsync_err_senderr == APP_MSG_OK) {
        // everything OK, reset AppSyncErrAlert so no vibrate
        if (AppSyncErrAlert == 111) { 
          ClearedOutage = 111; 
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "APPSYNC ERR, SET CLEARED OUTAGE: %i ", ClearedOutage);
        } 
        AppSyncErrAlert = 100;
        // sent message OK; return
	      return;
      } // if appsync_err_senderr
    } // if appsync_err_openerr
  } // if appsyncandmsg_retries_counter
    
  // flag resend error
  if (appsyncandmsg_retries_counter <= APPSYNCANDMSG_RETRIES_MAX) {
    //APP_LOG(APP_LOG_LEVEL_INFO, "APP SYNC RESEND ERROR");
    APP_LOG(APP_LOG_LEVEL_DEBUG, "APPSYNC RESEND ERR, OPEN: %i RES: %s SEND: %i RES: %s RETRIES: %i", 
            appsync_err_openerr, translate_app_error(appsync_err_openerr), appsync_err_senderr, translate_app_error(appsync_err_senderr), appsyncandmsg_retries_counter);
    return;
  } 
 
  // check bluetooth again
  //bluetooth_connected_syncerror = connection_service_peek_pebble_app_connection();
  bluetooth_connected_syncerror = bluetooth_connection_service_peek();  
  if (bluetooth_connected_syncerror == false) {
    // bluetooth is out, BT message already set; return out
    return;
  }
  
  // set message to Reload / Restart
  set_message_layer("RELOAD/RESTART\0", "", false, text_urgent_color);
  
  // reset appsync retries counter
  appsyncandmsg_retries_counter = 0;
  
  // clear cgm timeago icon and set init flag
  clear_cgm_timeago();

  // check if need to vibrate
  if (AppSyncErrAlert == 100) {
    //APP_LOG(APP_LOG_LEVEL_INFO, "APPSYNC ERROR: VIBRATE");
    alert_handler_cgm(APPSYNC_ERR_VIBE);
    AppSyncErrAlert = 111;
  } 
  
} // end sync_error_callback_cgm

// LOAD ROUTINES

//strtok
char *strtok(s, delim)
	register char *s;
	register const char *delim;
{
	register char *spanp;
	register int c, sc;
	char *tok;
	static char *last;


	if (s == NULL && (s = last) == NULL)
		return (NULL);

	/*
	 * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
	 */
cont:
	c = *s++;
	for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
		if (c == sc)
			goto cont;
	}

	if (c == 0) {		/* no non-delimiter characters */
		last = NULL;
		return (NULL);
	}
	tok = s - 1;

	/*
	 * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
	 * Note that delim must have one NUL; we stop if we see that, too.
	 */
	for (;;) {
		c = *s++;
		spanp = (char *)delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				last = s;
				return (tok);
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
} // end strtok

static void load_values(){
  //APP_LOG(APP_LOG_LEVEL_DEBUG,"Loaded Values: %s", current_values);

  int num_a_items = 0;
  char *o;
  int mgormm = 0;
  int vibes = 0;
  int rawvibrate = 0;
  int vibeon = 0;
  int rawon = 0;
  int urawon = 0;
  int animateon = 0;
  
  if (current_values == NULL) {
    return;
  } else {
    o = strtok(current_values,",");
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "mg or mm: %s", o);      
    mgormm = atoi(o);

    while(o != NULL) {
      num_a_items++;
      switch (num_a_items) {
        case 2:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "lowbg: %s", o);
          if (mgormm == 0){
            LOW_BG_MGDL = atoi(o);
            if (LOW_BG_MGDL < 60) {
            	MIDLOW_BG_MGDL = 55;
            	BIGLOW_BG_MGDL = 50;
            	HYPOLOW_BG_MGDL = 45;
            } else if (LOW_BG_MGDL < 70) {
            	MIDLOW_BG_MGDL = 60;
            	BIGLOW_BG_MGDL = 55;
            	HYPOLOW_BG_MGDL = 50;
            }
          } else {
            LOW_BG_MMOL = atoi(o);
            if (LOW_BG_MMOL < 33) {
            	MIDLOW_BG_MMOL = 31;
            	BIGLOW_BG_MMOL = 28;
            	HYPOLOW_BG_MMOL = 25;
            } else if (LOW_BG_MMOL < 39) {
            	MIDLOW_BG_MMOL = 33;
            	BIGLOW_BG_MMOL = 31;
            	HYPOLOW_BG_MMOL = 28;
            }
          }
          break;
        case 3:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "highbg: %s", o);
          if (mgormm == 0){
            HIGH_BG_MGDL = atoi(o);
            if (HIGH_BG_MGDL > 239) {
              MIDHIGH_BG_MGDL = 300;
              BIGHIGH_BG_MGDL = 350;
            }
          } else {
            HIGH_BG_MMOL = atoi(o);
              if (HIGH_BG_MMOL > 132) {
               MIDHIGH_BG_MMOL = 166;
               BIGHIGH_BG_MMOL =  200;
              }
          }
          break;
        case 4:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "lowsnooze: %s", o);
          LOW_SNZ_MIN = atoi(o);
          break;
        case 5:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "highsnooze: %s", o);      
          HIGH_SNZ_MIN = atoi(o);
          break;
        case 6:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "lowvibe: %s", o);
          LOWBG_VIBE = atoi(o);
          break;
        case 7:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "highvibe: %s", o);
          HIGHBG_VIBE = atoi(o);
          break;
        case 8:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "vibepattern: %s", o);
          vibes = atoi(o);        
          if (vibes == 0){
            TurnOffAllVibrations = 111;
            TurnOffStrongVibrations = 111;
          } else if (vibes == 1){
            TurnOffAllVibrations = 100;
            TurnOffStrongVibrations = 111; 
          } else if (vibes == 2){
            TurnOffAllVibrations = 100;
            TurnOffStrongVibrations = 100;
          }
          break;
        case 9:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "timeformat: %s", o);
          timeformat = atoi(o);
          break;
        case 10:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "rawvibrate: %s", o);
          rawvibrate = atoi(o);
          if (rawvibrate == 0) { TurnOffVibrationsCalcRaw = 111; }
          else { TurnOffVibrationsCalcRaw = 100; }
          break;
        case 11:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "vibeon: %s", o);
          vibeon = atoi(o);
          if (vibeon == 0) { 
            // turn vibrator off; emoji will be set to sleep so have visual indication
            HardCodeNoVibrations = 111; 
          }
          else { 
            // turn vibrator on 
            HardCodeNoVibrations = 100;
          }
          break;
        case 12:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "rawon: %s", o);
          rawon = atoi(o);
          if (rawon == 0) { 
            TurnOnCalcRaw = 100; 
            TurnOnUnfilteredRaw = 100;
          } 
          else { 
            TurnOnCalcRaw = 111;
            TurnOnUnfilteredRaw = 111;
          }
          break;
        case 13:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "urawon: %s", o);
          urawon = atoi(o);
          if (urawon == 0) { TurnOnUnfilteredRaw = 100; }
          else { TurnOnUnfilteredRaw = 111; }
          break;
        case 14:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "animateon: %s", o);
          animateon = atoi(o);
          if (animateon == 0) { HardCodeNoAnimations = 111; }
          else { HardCodeNoAnimations = 100; }
          break; 
        case 15:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "mainfont_value: %s", o);
          mainfont_value = atoi(o);
          break;
        case 16:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "batterydisplay_value: %s", o);
          batterydisplay_value = atoi(o);
          break;
        case 17:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "feeling_value: %s", o);
          feeling_value = atoi(o);
          break;
        case 18:
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "color_value: %s", o);
          color_value = atoi(o);
          break;
        
      }
      o = strtok(NULL,",");
    }
  }
   
} //End load_values
static void load_icon() {
	//APP_LOG(APP_LOG_LEVEL_INFO, "LOAD ICON ARROW FUNCTION START");
	
	// CONSTANTS
	
	// ICON ASSIGNMENTS OF ARROW DIRECTIONS
	const char NO_ARROW[] = "0";
	const char DOUBLEUP_ARROW[] = "1";
	const char SINGLEUP_ARROW[] = "2";
	const char UP45_ARROW[] = "3";
	const char FLAT_ARROW[] = "4";
	const char DOWN45_ARROW[] = "5";
	const char SINGLEDOWN_ARROW[] = "6";
	const char DOUBLEDOWN_ARROW[] = "7";
	const char NOTCOMPUTE_ICON[] = "8";
	const char OUTOFRANGE_ICON[] = "9";
	
  
	// ARRAY OF ARROW ICON IMAGES
	const uint8_t ARROW_ICONS[] = {
	  RESOURCE_ID_IMAGE_SPECVALUE_NONE,  //0
	  RESOURCE_ID_IMAGE_UPUP,            //1
	  RESOURCE_ID_IMAGE_UP,              //2
	  RESOURCE_ID_IMAGE_UP45,            //3
	  RESOURCE_ID_IMAGE_FLAT,            //4
	  RESOURCE_ID_IMAGE_DOWN45,          //5
	  RESOURCE_ID_IMAGE_DOWN,            //6
	  RESOURCE_ID_IMAGE_DOWNDOWN,        //7
	  RESOURCE_ID_IMAGE_LOGO             //8
	};
    
	// INDEX FOR ARRAY OF ARROW ICON IMAGES
	const uint8_t NONE_ARROW_ICON_INDX = 0;
	const uint8_t UPUP_ICON_INDX = 1;
	const uint8_t UP_ICON_INDX = 2;
	const uint8_t UP45_ICON_INDX = 3;
	const uint8_t FLAT_ICON_INDX = 4;
	const uint8_t DOWN45_ICON_INDX = 5;
	const uint8_t DOWN_ICON_INDX = 6;
	const uint8_t DOWNDOWN_ICON_INDX = 7;
	const uint8_t LOGO_ARROW_ICON_INDX = 8;
  
  // VARIABLES
  //static uint8_t DoubleDownAlert = 100;
  
  // CODE START
	
  // For testing only
  //strncpy(current_icon, DOUBLEDOWN_ARROW, 3);
  
  // Got an icon value, check data offline condition, clear vibrate flag if needed
  if ( (strcmp(current_bg_delta, "OFF") < 0) || (strcmp(current_bg_delta, "OFF") > 0) ) {
    DataOfflineAlert = 100;
    dataoffline_retries_counter = 0;
  }
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD ARROW ICON, BEFORE CHECK SPEC VALUE BITMAP");
	// check if special value set
	if (specvalue_alert == 100) {
    
    // no special value, set arrow
    // check for arrow direction, set proper arrow icon
	  //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD ICON, CURRENT ICON: %s", current_icon);
    
    if (strcmp(current_icon, DOUBLEDOWN_ARROW) == 0) {
	    if (DoubleDownAlert == 100) {
	      //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD ICON, ICON ARROW: DOUBLE DOWN");
	      alert_handler_cgm(DOUBLEDOWN_VIBE);
	      DoubleDownAlert = 111;
	    }
	    create_update_bitmap(&icon_bitmap,icon_layer,ARROW_ICONS[DOWNDOWN_ICON_INDX]);
      return;
    } 
  
    // not in a DoubleDown, so clear alert
    DoubleDownAlert = 100;
        
    if ( (strcmp(current_icon, NO_ARROW) == 0) || (strcmp(current_icon, NOTCOMPUTE_ICON) == 0) || (strcmp(current_icon, OUTOFRANGE_ICON) == 0) ) {
	    create_update_bitmap(&icon_bitmap,icon_layer,ARROW_ICONS[NONE_ARROW_ICON_INDX]);
    } 
    else if (strcmp(current_icon, DOUBLEUP_ARROW) == 0) {
	    create_update_bitmap(&icon_bitmap,icon_layer,ARROW_ICONS[UPUP_ICON_INDX]);
    }
    else if (strcmp(current_icon, SINGLEUP_ARROW) == 0) {
	    create_update_bitmap(&icon_bitmap,icon_layer,ARROW_ICONS[UP_ICON_INDX]);
    }
    else if (strcmp(current_icon, UP45_ARROW) == 0) {
      create_update_bitmap(&icon_bitmap,icon_layer,ARROW_ICONS[UP45_ICON_INDX]);
    }
    else if (strcmp(current_icon, FLAT_ARROW) == 0) {
      create_update_bitmap(&icon_bitmap,icon_layer,ARROW_ICONS[FLAT_ICON_INDX]);
    }
    else if (strcmp(current_icon, DOWN45_ARROW) == 0) {
	    create_update_bitmap(&icon_bitmap,icon_layer,ARROW_ICONS[DOWN45_ICON_INDX]);
    }
    else if (strcmp(current_icon, SINGLEDOWN_ARROW) == 0) {
      create_update_bitmap(&icon_bitmap,icon_layer,ARROW_ICONS[DOWN_ICON_INDX]);
    }
    else {
	    // check for special cases and set icon accordingly
		  // check bluetooth
      //bluetooth_connected_cgm = connection_service_peek_pebble_app_connection();
      bluetooth_connected_cgm = bluetooth_connection_service_peek();
        
	    // check to see if we are in the loading screen  
	    if (!bluetooth_connected_cgm) {
	      // Bluetooth is out; in the loading screen so set logo
	      create_update_bitmap(&icon_bitmap,icon_layer,ARROW_ICONS[LOGO_ARROW_ICON_INDX]);
	    }
	    else {
		    // unexpected, set blank icon
	      create_update_bitmap(&icon_bitmap,icon_layer,ARROW_ICONS[NONE_ARROW_ICON_INDX]);
	    }
	  }
	} // if specvalue_alert == 100
	else { // this is just for log when need it
	  //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD ICON, SPEC VALUE ALERT IS TRUE, DONE");
	} // else specvalue_alert == 111
	
} // end load_icon

static void set_bg_image_layer (uint8_t bg_image_index, bool use_club_images) { 
  
  // ARRAY OF BG IMAGE ICONS
  const uint8_t BG_IMAGE_ICONS[] = {
	  RESOURCE_ID_IMAGE_HI,   //0
	  RESOURCE_ID_IMAGE_LO     //1
  };
  
  // ARRAY OF ICONS FOR PERFECT BG
  const uint8_t PERFECTBG_ICONS[] = {
	  RESOURCE_ID_IMAGE_CLUB100,         //0
	  RESOURCE_ID_IMAGE_CLUB55           //1
  };
  
  if (use_club_images) {
    create_update_bitmap(&bg_image_bitmap,bg_image_layer,PERFECTBG_ICONS[bg_image_index]);
  }
  else {
    create_update_bitmap(&bg_image_bitmap,bg_image_layer,BG_IMAGE_ICONS[bg_image_index]);
  }
  
  layer_set_hidden(text_layer_get_layer(bg_layer), true);
  layer_set_hidden(bitmap_layer_get_layer(bg_image_layer), false);
  
} // end set_bg_image_layer

// forward declarations for animation code
static void load_t1dname();

// ANIMATION CODE

void destroy_happymsg_animation(PropertyAnimation **happymsg_animation) {
  
  // VARIABLES
  bool anim_got_elapsed = false;
  int32_t *animation_elapsed_time = 0;
  
  if (*happymsg_animation == NULL) {
    return;
  }

  if (animation_is_scheduled((Animation*) *happymsg_animation)) {
    // only unschedule if the animation is over; once over it will unschedule itself
    anim_got_elapsed = animation_get_elapsed((Animation*) *happymsg_animation, animation_elapsed_time);
    if ( (anim_got_elapsed) && 
        (!(*animation_elapsed_time > 0) && (*animation_elapsed_time < HAPPYMSG_ANIMATE_SECS*MS_IN_A_SECOND)) ) {
      // animation exists and is over, unschedule
      animation_unschedule((Animation*) *happymsg_animation);
    }
  }

  //Only SDK 2.0 requires a call to property_animation_destroy
  if (happymsg_animation != NULL) {
    #ifdef PBL_PLATFORM_APLITE
    property_animation_destroy(*happymsg_animation);
    #endif
  }
  
  *happymsg_animation = NULL;
} // end destroy_happymsg_animation

// happymsg ANIMATION
void happymsg_animation_started(Animation *animation, void *data) {

  // CONSTANTS
  
  // INDEX FOR ARRAY OF PERFECT BG ICONS
  static const uint8_t CLUB100_ICON_INDX = 0;
  static const uint8_t CLUB55_ICON_INDX = 1;
  
	//APP_LOG(APP_LOG_LEVEL_INFO, "HAPPY MSG ANIMATE, ANIMATION STARTED ROUTINE, CLEAR OUT BOTTOM LAYER");
  
	// clear out bottom layer to scroll message
  text_layer_set_text(t1dname_layer, "\0");
  text_layer_set_text(date_app_layer, "\0");
  
  // set perfect bg club image
  if ( ((currentBG_isMMOL == 100) && (current_bg == 100)) || ((currentBG_isMMOL == 111) && (current_bg == 55)) ) {

    // set appropriate bg club image
    if ((currentBG_isMMOL == 111) && (current_bg == 55)) { 
      set_bg_image_layer(CLUB55_ICON_INDX, true);
    }
    else if ((currentBG_isMMOL == 100) && (current_bg == 100)) {
      set_bg_image_layer(CLUB100_ICON_INDX, true);
    }
  }
  
} // end happymsg_animation_started

void happymsg_animation_stopped(Animation *animation, bool finished, void *data) {

	//APP_LOG(APP_LOG_LEVEL_INFO, "HAPPY MSG ANIMATE, ANIMATION STOPPED ROUTINE");

   // reset bg if needed
  if ( ((currentBG_isMMOL == 100) && (current_bg == 100)) || ((currentBG_isMMOL == 111) && (current_bg == 55)) ) {
    layer_set_hidden(text_layer_get_layer(bg_layer), false);
    layer_set_hidden(bitmap_layer_get_layer(bg_image_layer), true);
  }
  
	// set BG delta / message layer
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "HAPPY MSG ANIMATE, ANIMATION STOPPED, SET TO BG DELTA);
  load_t1dname();
  draw_date_from_app();
  
  // Cancel timers and null pointers if needed
  destroy_happymsg_animation(&happymsg_animation);
  
} // end happymsg_animation_stopped

void animate_happymsg(char *happymsg_to_display) {

  // CONSTANTS
  const uint8_t HAPPYMSG_BUFFER_SIZE = 30;
  
	// VARIABLES 
  Layer *animate_happymsg_layer = NULL;
  
	// for animation
	GRect from_happymsg_rect = GRect(0,0,0,0);
	GRect to_happymsg_rect = GRect(0,0,0,0);

  static char animate_happymsg_buffer[30] = {0};
  
	// CODE START
    
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "ANIMATE HAPPY MSG, STRING PASSED: %s", happymsg_to_display);
    strncpy(animate_happymsg_buffer, happymsg_to_display, HAPPYMSG_BUFFER_SIZE);
	  text_layer_set_text(happymsg_layer, animate_happymsg_buffer);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "ANIMATE HAPPY MSG, MSG IN BUFFER: %s", animate_happymsg_buffer);
    animate_happymsg_layer = text_layer_get_layer(happymsg_layer);
	  from_happymsg_rect = GRect(144, 119, 144, 55);
	  to_happymsg_rect = GRect(-144, 119, 144, 55); 
	  destroy_happymsg_animation(&happymsg_animation);
	  //APP_LOG(APP_LOG_LEVEL_INFO, "ANIMATE HAPPY MSG, CREATE FRAME");
	  happymsg_animation = property_animation_create_layer_frame(animate_happymsg_layer, &from_happymsg_rect, &to_happymsg_rect);
	  //APP_LOG(APP_LOG_LEVEL_INFO, "ANIMATE HAPPY MSG, SET DURATION AND CURVE");
	  animation_set_duration((Animation*) happymsg_animation, HAPPYMSG_ANIMATE_SECS*MS_IN_A_SECOND);
	  animation_set_curve((Animation*) happymsg_animation, AnimationCurveLinear);
  
	  //APP_LOG(APP_LOG_LEVEL_INFO, "ANIMATE HAPPY MSG, SET HANDLERS");
	  animation_set_handlers((Animation*) happymsg_animation, (AnimationHandlers) {
	    .started = (AnimationStartedHandler) happymsg_animation_started,
	    .stopped = (AnimationStoppedHandler) happymsg_animation_stopped,
	  }, NULL /* callback data */);
      
	  //APP_LOG(APP_LOG_LEVEL_INFO, "ANIMATE HAPPY MSG, SCHEDULE");
	  animation_schedule((Animation*) happymsg_animation);   
  
} //end animate_happymsg

static void set_happymsg_animation (int mgdl_value, int mmol_value, bool using_mmol, char *animate_happymsg_buffer) {
  
  if ( ((currentBG_isMMOL == 100) && (current_bg == mgdl_value)) || 
       ((currentBG_isMMOL == 111) && (current_bg == mmol_value) && (using_mmol)) ) {
		      // HAPPY MSG, ANIMATE BG      
		      //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, ANIMATE PERFECT BG");
		      animate_happymsg(animate_happymsg_buffer);
        } // animate happy msg layer
  
} // end set_happymsg_animation

void bg_vibrator (uint16_t BG_BOTTOM_INDX, uint16_t BG_TOP_INDX, uint8_t BG_SNOOZE, uint8_t *bg_overwrite, uint8_t BG_VIBE) {

      // VARIABLES
  
      uint16_t conv_vibrator_bg = 180;

      conv_vibrator_bg = current_bg;
  
      // adjust high bg for comparison, if needed
      if ( ((currentBG_isMMOL == 111) && (current_bg >= HIGH_BG_MGDL))
        || ((currentBG_isMMOL == 100) && (current_bg >= HIGH_BG_MMOL)) ) {
        conv_vibrator_bg = current_bg + 1;
      }
  
      // check BG and vibrate if needed
      //APP_LOG(APP_LOG_LEVEL_INFO, "BG VIBRATOR, CHECK TO SEE IF WE NEED TO VIBRATE");
      if ( ( ((conv_vibrator_bg > BG_BOTTOM_INDX) && (conv_vibrator_bg <= BG_TOP_INDX))
          && ((lastAlertTime == 0) || (lastAlertTime > BG_SNOOZE)) )
		    || ( ((conv_vibrator_bg > BG_BOTTOM_INDX) && (conv_vibrator_bg <= BG_TOP_INDX)) && (*bg_overwrite == 100) ) ) {
      
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "lastAlertTime SNOOZE VALUE IN: %i", lastAlertTime);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "bg_overwrite IN: %i", *bg_overwrite);
     
        // send alert and handle a bouncing connection
        if ((lastAlertTime == 0) || (*bg_overwrite == 100)) { 
        //APP_LOG(APP_LOG_LEVEL_INFO, "BG VIBRATOR: VIBRATE");
          alert_handler_cgm(BG_VIBE);        
          // don't know where we are coming from, so reset last alert time no matter what
          // set to 1 to prevent bouncing connection
          lastAlertTime = 1;
         if (*bg_overwrite == 100) { *bg_overwrite = 111; }
        }
      
        // if hit snooze, reset snooze counter; will alert next time around
        if (lastAlertTime > BG_SNOOZE) { 
          lastAlertTime = 0;
          specvalue_overwrite = 100;
          hypolow_overwrite = 100;
          biglow_overwrite = 100;
          midlow_overwrite = 100;
          low_overwrite = 100;
          midhigh_overwrite = 100;
          bighigh_overwrite = 100;
          //APP_LOG(APP_LOG_LEVEL_INFO, "BG VIBRATOR, OVERWRITE RESET");
        }
      
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "BG VIBRATOR, lastAlertTime SNOOZE VALUE OUT: %i", lastAlertTime);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "BG VIBRATOR, bg_overwrite OUT: %i", *bg_overwrite);
      } 

} // end bg_vibrator	  

void set_special_value (uint8_t specvalue_icon_index, char *bg_layer_string, uint8_t specvalue_alert_setting) {
  
  // ARRAY OF SPECIAL VALUE ICONS
static const uint8_t SPECIAL_VALUE_ICONS[] = {
	RESOURCE_ID_IMAGE_SPECVALUE_NONE,   //0
	RESOURCE_ID_IMAGE_BROKEN_ANTENNA,   //1
  #ifdef PBL_PLATFORM_APLITE
  RESOURCE_ID_IMAGE_BLACK_DROP,       //2
  #else
  RESOURCE_ID_IMAGE_RED_DROP,         //2
  #endif
	RESOURCE_ID_IMAGE_HOURGLASS,        //3
	RESOURCE_ID_IMAGE_QUESTION_MARKS,   //4
	RESOURCE_ID_IMAGE_LOGO              //5
};
  
  text_layer_set_text(bg_layer, (char *)bg_layer_string);
	create_update_bitmap(&specialvalue_bitmap,icon_layer, SPECIAL_VALUE_ICONS[specvalue_icon_index]);
	specvalue_alert = specvalue_alert_setting;
  
} // end set_special_value

static void set_calculated_raw_layer (TextLayer **calcraw_classic_layer, TextLayer **calcraw_digi_layer, 
                                      char *check_last_calc_raw, int check_currect_calc_raw) {
  
  // set default text color
      if (mainfont_value == 1) { 
        text_layer_set_text_color(*calcraw_classic_layer, text_default_color); 
      }
      else { 
        text_layer_set_text_color(*calcraw_digi_layer, text_default_color); 
      }
      
      // set bg field accordingly for calculated raw layer
      if (mainfont_value == 1) { 
        text_layer_set_text(*calcraw_classic_layer, check_last_calc_raw); 
      }
      else { 
        if (check_currect_calc_raw == 0) { 
          text_layer_set_text(*calcraw_digi_layer, " ");
        }
        else {
          text_layer_set_text(*calcraw_digi_layer, check_last_calc_raw);
        }
      }
  
} // set_calculated_raw_layer

static void load_bg() {
  //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, FUNCTION START");
	
	// CONSTANTS
  const uint8_t BG_BUFFER_SIZE = 6;
  
	// ARRAY OF BG CONSTANTS; MGDL
	uint16_t BG_MGDL[] = {
	  SPECVALUE_BG_MGDL,	//0
	  SHOWLOW_BG_MGDL,		//1
	  HYPOLOW_BG_MGDL,		//2
	  BIGLOW_BG_MGDL,		//3
	  MIDLOW_BG_MGDL,		//4
	  LOW_BG_MGDL,			//5
	  HIGH_BG_MGDL,			//6
	  MIDHIGH_BG_MGDL,		//7
	  BIGHIGH_BG_MGDL,		//8
	  SHOWHIGH_BG_MGDL		//9
	};
	
	// ARRAY OF BG CONSTANTS; MMOL
	uint16_t BG_MMOL[] = {
	  SPECVALUE_BG_MMOL,	//0
	  SHOWLOW_BG_MMOL,		//1
	  HYPOLOW_BG_MMOL,		//2
	  BIGLOW_BG_MMOL,		//3
	  MIDLOW_BG_MMOL,		//4
	  LOW_BG_MMOL,			//5
	  HIGH_BG_MMOL,			//6
	  MIDHIGH_BG_MMOL,		//7
	  BIGHIGH_BG_MMOL,		//8
	  SHOWHIGH_BG_MMOL		//9
	};
    
	// INDEX FOR ARRAYS OF BG CONSTANTS
	const uint8_t SPECVALUE_BG_INDX = 0;
	const uint8_t SHOWLOW_BG_INDX = 1;
	const uint8_t HYPOLOW_BG_INDX = 2;
	const uint8_t BIGLOW_BG_INDX = 3;
	const uint8_t MIDLOW_BG_INDX = 4;
	const uint8_t LOW_BG_INDX = 5;
	const uint8_t HIGH_BG_INDX = 6;
	const uint8_t MIDHIGH_BG_INDX = 7;
	const uint8_t BIGHIGH_BG_INDX = 8;
	const uint8_t SHOWHIGH_BG_INDX = 9;
	
	// MG/DL SPECIAL VALUE CONSTANTS ACTUAL VALUES
	// mg/dL = mmol / .0555 OR mg/dL = mmol * 18.0182
	const uint8_t SENSOR_NOT_ACTIVE_VALUE_MGDL = 1;		// show blood drop, ?SN
	const uint8_t MINIMAL_DEVIATION_VALUE_MGDL = 2; 		// show blood drop, ?MD
	const uint8_t NO_ANTENNA_VALUE_MGDL = 3; 			// show broken antenna, ?NA 
	const uint8_t SENSOR_NOT_CALIBRATED_VALUE_MGDL = 5;	// show blood drop, ?NC
	const uint8_t STOP_LIGHT_VALUE_MGDL = 6;				// show blood drop, ?CD
	const uint8_t HOURGLASS_VALUE_MGDL = 9;				// show hourglass, hourglass
	const uint8_t QUESTION_MARKS_VALUE_MGDL = 10;		// show ???, ???
	const uint8_t BAD_RF_VALUE_MGDL = 12;				// show broken antenna, ?RF

	// MMOL SPECIAL VALUE CONSTANTS ACTUAL VALUES
	// mmol = mg/dL / 18.0182 OR mmol = mg/dL * .0555
	const uint8_t SENSOR_NOT_ACTIVE_VALUE_MMOL = 1;		// show blood drop, ?SN (.06 -> .1)
	const uint8_t MINIMAL_DEVIATION_VALUE_MMOL = 1;		// show blood drop, ?MD (.11 -> .1)
	const uint8_t NO_ANTENNA_VALUE_MMOL = 2;				// show broken antenna, ?NA (.17 -> .2)
	const uint8_t SENSOR_NOT_CALIBRATED_VALUE_MMOL = 3;	// show blood drop, ?NC, (.28 -> .3)
	const uint8_t STOP_LIGHT_VALUE_MMOL = 4;				// show blood drop, ?CD (.33 -> .3, set to .4 here)
	const uint8_t HOURGLASS_VALUE_MMOL = 5;				// show hourglass, hourglass (.50 -> .5)
	const uint8_t QUESTION_MARKS_VALUE_MMOL = 6;			// show ???, ??? (.56 -> .6)
	const uint8_t BAD_RF_VALUE_MMOL = 7;					// show broken antenna, ?RF (.67 -> .7)
	
	// ARRAY OF SPECIAL VALUES CONSTANTS; MGDL
	uint8_t SPECVALUE_MGDL[] = {
	  SENSOR_NOT_ACTIVE_VALUE_MGDL,		//0	
	  MINIMAL_DEVIATION_VALUE_MGDL,		//1
	  NO_ANTENNA_VALUE_MGDL,			//2
	  SENSOR_NOT_CALIBRATED_VALUE_MGDL,	//3
	  STOP_LIGHT_VALUE_MGDL,			//4
	  HOURGLASS_VALUE_MGDL,				//5
	  QUESTION_MARKS_VALUE_MGDL,		//6
	  BAD_RF_VALUE_MGDL					//7
	};
	
	// ARRAY OF SPECIAL VALUES CONSTANTS; MMOL
	uint8_t SPECVALUE_MMOL[] = {
	  SENSOR_NOT_ACTIVE_VALUE_MMOL,		//0	
	  MINIMAL_DEVIATION_VALUE_MMOL,		//1
	  NO_ANTENNA_VALUE_MMOL,			//2
	  SENSOR_NOT_CALIBRATED_VALUE_MMOL,	//3
	  STOP_LIGHT_VALUE_MMOL,			//4
	  HOURGLASS_VALUE_MMOL,				//5
	  QUESTION_MARKS_VALUE_MMOL,		//6
	  BAD_RF_VALUE_MMOL					//7
	};
	
	// INDEX FOR ARRAYS OF SPECIAL VALUES CONSTANTS
	const uint8_t SENSOR_NOT_ACTIVE_VALUE_INDX = 0;
	const uint8_t MINIMAL_DEVIATION_VALUE_INDX = 1;
	const uint8_t NO_ANTENNA_VALUE_INDX = 2;
	const uint8_t SENSOR_NOT_CALIBRATED_VALUE_INDX = 3;
	const uint8_t STOP_LIGHT_VALUE_INDX = 4;
	const uint8_t HOURGLASS_VALUE_INDX = 5;
	const uint8_t QUESTION_MARKS_VALUE_INDX = 6;
	const uint8_t BAD_RF_VALUE_INDX = 7;
 
  // INDEX FOR ARRAY OF SPECIAL VALUE ICONS
  const uint8_t NONE_SPECVALUE_ICON_INDX = 0;
  const uint8_t BROKEN_ANTENNA_ICON_INDX = 1;
  const uint8_t BLOOD_DROP_ICON_INDX = 2;
  const uint8_t HOURGLASS_ICON_INDX = 3;
  const uint8_t QUESTION_MARKS_ICON_INDX = 4;
  // LOGO value 5 is declared global
  //const uint8_t LOGO_SPECVALUE_ICON_INDX = 5;

  // INDEX FOR ARRAY OF BG IMAGE ICONS
  const uint8_t HI_ICON_INDX = 0;
  const uint8_t LO_ICON_INDX = 1;
  
	// VARIABLES 
  
	// pointers to be used to MGDL or MMOL values for parsing
	uint16_t *bg_ptr = NULL;
	uint8_t *specvalue_ptr = NULL;

  // happy message; max message 24 characters
  // DO NOT GO OVER 24 CHARACTERS, INCLUDING SPACES OR YOU WILL CRASH
  // YOU HAVE BEEN WARNED
	char happymsg_buffer66[26] = "BEAM MY BG UP SCOTTY!\0";
  char happymsg_buffer87[26] = "YOU'VE BEEN RICKROLLED\0";
  char happymsg_buffer100[26] = "WHO YA GONNA CALL?!?\0";
  char happymsg_buffer110[26] = "YOU ARE MY SUPERHERO!\0";
  char happymsg_buffer142[26] = "THE ANSWER TO*LIFE?\0";
  char happymsg_buffer248[26] = "BEE-DO! BEE-DO! BEE-DO!\0";
  char happymsg_buffer303[26] = "JUST KEEP SWIMMING\0";
    
	// CODE START
  text_layer_set_text_color(bg_layer, text_default_color);
  layer_set_hidden(text_layer_get_layer(bg_layer), false);
  layer_set_hidden(bitmap_layer_get_layer(bg_image_layer), true);
  
	// if special value set, erase anything in the icon field
	if (specvalue_alert == 111) {
    set_special_value(NONE_SPECVALUE_ICON_INDX, "\0", 111);
	}
	
	// set special value alert to zero no matter what
	specvalue_alert = 100;
  
    // see if we're doing MGDL or MMOL; get currentBG_isMMOL value in myBGAtoi
	  // convert BG value from string to int
	  
    // FOR TESTING ONLY
    //strncpy(last_bg, "5", BG_BUFFER_SIZE); 
    //strncpy(last_calc_raw1, "22.2", BG_BUFFER_SIZE);
    //strncpy(last_calc_raw2, "22.2", BG_BUFFER_SIZE); 
    //strncpy(last_calc_raw3, "22.2", BG_BUFFER_SIZE); 
    //strncpy(last_raw_unfilt, "266", BG_BUFFER_SIZE); 
  
	  // FOR TESTING ONLY, ANIMATIONS AND TRANSITIONS
    //if (current_bg == 87) { strncpy(last_bg, "89", BG_BUFFER_SIZE); }
    //else if (current_bg == 89) { strncpy(last_bg, "88", BG_BUFFER_SIZE); }
    //else { strncpy(last_bg, "87", BG_BUFFER_SIZE); }
	
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD BG, BGATOI IN, CURRENT_BG: %d LAST_BG: %s ", current_bg, last_bg);
    current_bg = myBGAtoi(last_bg);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD BG, BG ATOI OUT, CURRENT_BG: %d LAST_BG: %s ", current_bg, last_bg);
    
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "LAST BG: %s", last_bg);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "CURRENT BG: %i", current_bg);
    
  if (currentBG_isMMOL == 100) {
	  bg_ptr = BG_MGDL;
	  specvalue_ptr = SPECVALUE_MGDL;
	}
	else {
	  bg_ptr = BG_MMOL;
	  specvalue_ptr = SPECVALUE_MMOL;
  }
	
    // BG parse, check snooze, and set text 
      
    // check for init code or error code
   if ((current_bg <= 0) || (last_bg[0] == '-')) {
      lastAlertTime = 0;
      
      // check bluetooth
      //bluetooth_connected_cgm = connection_service_peek_pebble_app_connection();
      bluetooth_connected_cgm = bluetooth_connection_service_peek();
     
      if (!bluetooth_connected_cgm) {
	      // Bluetooth is out; set BT message
		    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, BG INIT: NO BT, SET NO BT MESSAGE");
          set_message_layer("NO BLUETOOTH\0", "", false, text_urgent_color);
      }// if !bluetooth connected
      else {
	      // if init code, we will set it right in message layer
	      //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, UNEXPECTED BG: SET ERR ICON");
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD BG, UNEXP BG, CURRENT_BG: %d LAST_BG: %s ", current_bg, last_bg);
        set_message_layer("BG ERROR\0", "", false, text_urgent_color);
        set_special_value(NONE_SPECVALUE_ICON_INDX, "\0", 111);
      }
      
	} // if current_bg <= 0
	  
    else {
      // valid BG
      
	    // check for special value, if special value, then replace icon and blank BG; else send current BG  
	    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, BEFORE CREATE SPEC VALUE BITMAP");
      
	  if ((current_bg == specvalue_ptr[NO_ANTENNA_VALUE_INDX]) || (current_bg == specvalue_ptr[BAD_RF_VALUE_INDX])) {
	    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, SPECIAL VALUE: SET BROKEN ANTENNA");
      set_special_value(BROKEN_ANTENNA_ICON_INDX, "\0", 111);
	  }
	  else if ((current_bg == specvalue_ptr[SENSOR_NOT_CALIBRATED_VALUE_INDX]) || (current_bg == specvalue_ptr[STOP_LIGHT_VALUE_INDX])
          || (current_bg == specvalue_ptr[SENSOR_NOT_ACTIVE_VALUE_INDX]) || (current_bg == specvalue_ptr[MINIMAL_DEVIATION_VALUE_INDX])) { 
	    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, SPECIAL VALUE: SET BLOOD DROP");
      set_special_value(BLOOD_DROP_ICON_INDX, "\0", 111);
	  }
	  else if (current_bg == specvalue_ptr[HOURGLASS_VALUE_INDX]) {
	    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, SPECIAL VALUE: SET HOUR GLASS");
      set_special_value(HOURGLASS_ICON_INDX, "\0", 111);
	  }
	  else if (current_bg == specvalue_ptr[QUESTION_MARKS_VALUE_INDX]) {
	    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, SPECIAL VALUE: SET QUESTION MARKS, CLEAR TEXT");
      set_special_value(QUESTION_MARKS_ICON_INDX, "\0", 111);
	  }
	  else if (current_bg < bg_ptr[SPECVALUE_BG_INDX]) {
	    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, UNEXPECTED SPECIAL VALUE: SET NO ICON");
      set_special_value(NONE_SPECVALUE_ICON_INDX, "\0", 111);
	  } // end special value checks
		
      //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, AFTER CREATE SPEC VALUE BITMAP");
      
	  if (specvalue_alert == 100) {
	    // we didn't find a special value, so set BG instead
	    // arrow icon already set separately
	    if (current_bg < bg_ptr[SHOWLOW_BG_INDX]) {
		    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG: SET TO LO");
        set_bg_image_layer(LO_ICON_INDX, false);
		}
		else if (current_bg > bg_ptr[SHOWHIGH_BG_INDX]) {
		  //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG: SET TO HI");
      set_bg_image_layer(HI_ICON_INDX, false);
		}
		else {
		  // else update with current BG
		  //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD BG, SET TO BG: %s ", last_bg);
      
      // set BG color
		  if ( ((current_bg <= bg_ptr[LOW_BG_INDX]) && (current_bg > bg_ptr[BIGLOW_BG_INDX])) ||
           ((current_bg >= bg_ptr[HIGH_BG_INDX]) && (current_bg < bg_ptr[BIGHIGH_BG_INDX])) ) {
        #ifdef PBL_COLOR
        text_layer_set_text_color(bg_layer, text_urgent_color); 
        #endif
      }
      else if ( (current_bg <= bg_ptr[BIGLOW_BG_INDX]) ||
                (current_bg >= bg_ptr[BIGHIGH_BG_INDX]) ) {
        #ifdef PBL_COLOR
        text_layer_set_text_color(bg_layer, text_urgent_color); 
        #endif
      }
      
		  text_layer_set_text(bg_layer, last_bg);
 
      // EVERY TIME YOU DO A NEW MESSAGE, YOU HAVE TO ALLOCATE A NEW HAPPY MSG BUFFER AT THE TOP OF LOAD BG FUNCTION
      
      if (HardCodeNoAnimations == 100) {
        set_happymsg_animation(100, 55, true, happymsg_buffer100);
        set_happymsg_animation(66, 36, true, happymsg_buffer66);
        set_happymsg_animation(87, 87, true, happymsg_buffer87);
        set_happymsg_animation(110, 60, true, happymsg_buffer110);
        set_happymsg_animation(142, 42, true, happymsg_buffer142);
        set_happymsg_animation(248, 148, true, happymsg_buffer248);
        set_happymsg_animation(303, 166, true, happymsg_buffer303);
        
      } // HardCodeNoAnimations; end all animation code
	  
      }	  } // end bg checks (if special_value_bitmap)
      
      // set calculated raw color
      if (HaveCalcRaw == 111) {
        current_calc_raw = myBGAtoi(last_calc_raw);
        // set default text color
        text_layer_set_text_color(raw_calc_layer, text_default_color);
        #ifdef PBL_COLOR
        if ( ((current_calc_raw <= bg_ptr[LOW_BG_INDX]) && (current_calc_raw > bg_ptr[BIGLOW_BG_INDX])) ||
             ((current_calc_raw >= bg_ptr[HIGH_BG_INDX]) && (current_calc_raw < bg_ptr[BIGHIGH_BG_INDX])) ) {
          //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, RAW CALC YELLOW ALERT");
          text_layer_set_text_color(raw_calc_layer, text_urgent_color); 
        }
        else if ( (current_calc_raw <= bg_ptr[BIGLOW_BG_INDX]) ||
                  (current_calc_raw >= bg_ptr[BIGHIGH_BG_INDX]) ) {
          //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, RAW CALC RED ALERT");
          text_layer_set_text_color(raw_calc_layer, text_urgent_color); 
        }
        #endif
        text_layer_set_text(raw_calc_layer, last_calc_raw);
      }
      
      // set unfiltered raw color
      if (HaveURaw == 111) {
        current_uraw = myBGAtoi(last_raw_unfilt);
        // set default text color
        text_layer_set_text_color(raw_unfilt_layer, text_default_color);
        #ifdef PBL_COLOR
        if ( ((current_uraw <= bg_ptr[LOW_BG_INDX]) && (current_uraw > bg_ptr[BIGLOW_BG_INDX])) ||
             ((current_uraw >= bg_ptr[HIGH_BG_INDX]) && (current_uraw < bg_ptr[BIGHIGH_BG_INDX])) ) {
          //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, RAW CALC YELLOW ALERT");
          text_layer_set_text_color(raw_unfilt_layer, text_urgent_color); 
        }
        else if ( (current_uraw <= bg_ptr[BIGLOW_BG_INDX]) ||
                  (current_uraw >= bg_ptr[BIGHIGH_BG_INDX]) ) {
          //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, RAW CALC RED ALERT");
          text_layer_set_text_color(raw_unfilt_layer, text_urgent_color); 
        }
        #endif
        text_layer_set_text(raw_unfilt_layer, last_raw_unfilt);
      }
      
      // see if we're going to use the current bg or the calculated raw bg for vibrations
      if ( ((current_bg > 0) && (current_bg < bg_ptr[SPECVALUE_BG_INDX])) && (HaveCalcRaw == 111) ) {
        
        //current_calc_raw = myBGAtoi(last_calc_raw);
        
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD BG, TurnOffVibrationsCalcRaw: %d", TurnOffVibrationsCalcRaw);
         
        if (TurnOffVibrationsCalcRaw == 100) {
          // set current_bg to calculated raw so we can vibrate on that instead
          current_bg = current_calc_raw;
          if (currentBG_isMMOL == 100) {
            bg_ptr = BG_MGDL;
            specvalue_ptr = SPECVALUE_MGDL;
          }
          else {
            bg_ptr = BG_MMOL;
            specvalue_ptr = SPECVALUE_MMOL;
          }
        } // TurnOffVibrationsCalcRaw
        
        // use calculated raw values in BG field; if different cascade down so we have last three values
        if (current_calc_raw != current_calc_raw1) {
            strncpy(last_calc_raw3, last_calc_raw2, BG_BUFFER_SIZE);
            strncpy(last_calc_raw2, last_calc_raw1, BG_BUFFER_SIZE);
            strncpy(last_calc_raw1, last_calc_raw, BG_BUFFER_SIZE);
            current_calc_raw3 = current_calc_raw2;
            current_calc_raw2 = current_calc_raw1;
            current_calc_raw1 = current_calc_raw;
        }
      }
      
      else {
        // if not in special values or don't have calculated raw, blank out the fields
        strncpy(last_calc_raw1, " ", BG_BUFFER_SIZE);
        strncpy(last_calc_raw2, " ", BG_BUFFER_SIZE);
        strncpy(last_calc_raw3, " ", BG_BUFFER_SIZE);
      }
      
      // set calculated raw layers, default text and number
      set_calculated_raw_layer(&calcraw_last1_classic_layer, &calcraw_last1_digi_layer, last_calc_raw1, current_calc_raw1);
      set_calculated_raw_layer(&calcraw_last2_classic_layer, &calcraw_last2_digi_layer, last_calc_raw2, current_calc_raw2);
      set_calculated_raw_layer(&calcraw_last3_classic_layer, &calcraw_last3_digi_layer, last_calc_raw3, current_calc_raw3);
      
      #ifdef PBL_COLOR
      
      // set bg field color accordingly for calculated raw layer
      if ( ((current_calc_raw1 <= bg_ptr[LOW_BG_INDX]) && (current_calc_raw1 > bg_ptr[BIGLOW_BG_INDX])) ||
           ((current_calc_raw1 >= bg_ptr[HIGH_BG_INDX]) && (current_calc_raw1 < bg_ptr[BIGHIGH_BG_INDX])) ) {
        //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, CALC RAW 1 YELLOW ALERT");
        if (mainfont_value == 1) { text_layer_set_text_color(calcraw_last1_classic_layer, text_warning_color); }
        else { text_layer_set_text_color(calcraw_last1_digi_layer, text_warning_color); }
      }
      else if ( (current_calc_raw1 <= bg_ptr[BIGLOW_BG_INDX]) ||
                (current_calc_raw1 >= bg_ptr[BIGHIGH_BG_INDX]) ) {
        //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, CALC RAW 1 RED ALERT");
        if (mainfont_value == 1) { text_layer_set_text_color(calcraw_last1_classic_layer, text_urgent_color); }
        else { text_layer_set_text_color(calcraw_last1_digi_layer, text_urgent_color); }
      }
      
      if ( ((current_calc_raw2 <= bg_ptr[LOW_BG_INDX]) && (current_calc_raw2 > bg_ptr[BIGLOW_BG_INDX])) ||
           ((current_calc_raw2 >= bg_ptr[HIGH_BG_INDX]) && (current_calc_raw2 < bg_ptr[BIGHIGH_BG_INDX])) ) {
        //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, CALC RAW 2 YELLOW ALERT");
        if (mainfont_value == 1) { text_layer_set_text_color(calcraw_last2_classic_layer, text_warning_color); }
        else { text_layer_set_text_color(calcraw_last2_digi_layer, text_warning_color); }
      }
      else if ( (current_calc_raw2 <= bg_ptr[BIGLOW_BG_INDX]) ||
                (current_calc_raw2 >= bg_ptr[BIGHIGH_BG_INDX]) ) {
        //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, CALC RAW 2 RED ALERT");
        if (mainfont_value == 1) { text_layer_set_text_color(calcraw_last2_classic_layer, text_urgent_color); }
        else { text_layer_set_text_color(calcraw_last2_digi_layer, text_urgent_color); }
      }
      
      if ( ((current_calc_raw3 <= bg_ptr[LOW_BG_INDX]) && (current_calc_raw3 > bg_ptr[BIGLOW_BG_INDX])) ||
           ((current_calc_raw3 >= bg_ptr[HIGH_BG_INDX]) && (current_calc_raw3 < bg_ptr[BIGHIGH_BG_INDX])) ) {
        //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, CALC RAW 3 YELLOW ALERT");
        if (mainfont_value == 1) { text_layer_set_text_color(calcraw_last3_classic_layer, text_warning_color); }
        else { text_layer_set_text_color(calcraw_last3_digi_layer, text_warning_color); } 
      }
      else if ( (current_calc_raw3 <= bg_ptr[BIGLOW_BG_INDX]) ||
                (current_calc_raw3 >= bg_ptr[BIGHIGH_BG_INDX]) ) {
        //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, CALC RAW 3 RED ALERT");
        if (mainfont_value == 1) { text_layer_set_text_color(calcraw_last3_classic_layer, text_urgent_color); }
        else { text_layer_set_text_color(calcraw_last3_digi_layer, text_urgent_color); }
      }
      #endif
    
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD BG, START VIBRATE, CURRENT_BG: %d LAST_BG: %s ", current_bg, last_bg);
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD BG, START VIBRATE, CURRENT_CALC_RAW: %d LAST_CALC_RAW: %s ", current_calc_raw, last_calc_raw);
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD BG, START VIBRATE, CALC_RAW 2: %d FORMAT CALC RAW 2: %s ", current_calc_raw2, last_calc_raw2);
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD BG, START VIBRATE, CALC_RAW 3: %d FORMAT CALC RAW 3: %s ", current_calc_raw3, last_calc_raw3);
      
      bg_vibrator (0, bg_ptr[SPECVALUE_BG_INDX], SPECVALUE_SNZ_MIN, &specvalue_overwrite, SPECVALUE_VIBE);
      bg_vibrator (bg_ptr[SPECVALUE_BG_INDX], bg_ptr[HYPOLOW_BG_INDX], HYPOLOW_SNZ_MIN, &hypolow_overwrite, HYPOLOWBG_VIBE);
      bg_vibrator (bg_ptr[HYPOLOW_BG_INDX], bg_ptr[BIGLOW_BG_INDX], BIGLOW_SNZ_MIN, &biglow_overwrite, BIGLOWBG_VIBE);
      bg_vibrator (bg_ptr[BIGLOW_BG_INDX], bg_ptr[MIDLOW_BG_INDX], MIDLOW_SNZ_MIN, &midlow_overwrite, LOWBG_VIBE);
      bg_vibrator (bg_ptr[MIDLOW_BG_INDX], bg_ptr[LOW_BG_INDX], LOW_SNZ_MIN, &low_overwrite, LOWBG_VIBE);
      bg_vibrator (bg_ptr[HIGH_BG_INDX], bg_ptr[MIDHIGH_BG_INDX], HIGH_SNZ_MIN, &high_overwrite, HIGHBG_VIBE);
      bg_vibrator (bg_ptr[MIDHIGH_BG_INDX], bg_ptr[BIGHIGH_BG_INDX], MIDHIGH_SNZ_MIN, &midhigh_overwrite, HIGHBG_VIBE);
      bg_vibrator (bg_ptr[BIGHIGH_BG_INDX], 1000, BIGHIGH_SNZ_MIN, &bighigh_overwrite, BIGHIGHBG_VIBE);

      // else "normal" range or init code
      if ( ((current_bg > bg_ptr[LOW_BG_INDX]) && (current_bg < bg_ptr[HIGH_BG_INDX])) 
              || (current_bg <= 0) ) {
      
        // do nothing; just reset snooze counter
        lastAlertTime = 0;
      } // else if "NORMAL RANGE" BG
	  
    } // else if current bg <= 0
      
    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG, FUNCTION OUT");
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD BG, FUNCTION OUT, SNOOZE VALUE: %d", lastAlertTime);
	
} // end load_bg

static void set_cgm_timeago (char *timeago_string, int timeago_diff, bool use_timeago_string, char *timeago_label) {
  
  // CONSTANTS
  const uint8_t TIMEAGO_BUFFER_SIZE = 10;
  
  // VARIABLES
  static char formatted_cgm_timeago[10] = {0};
  
  if (use_timeago_string) { text_layer_set_text(cgmtime_layer, (char *)timeago_string); }
  else { 
    snprintf(formatted_cgm_timeago, TIMEAGO_BUFFER_SIZE, "%i", timeago_diff);
    strncpy(cgm_label_buffer, timeago_label, LABEL_BUFFER_SIZE);
    strcat(formatted_cgm_timeago, cgm_label_buffer);
    text_layer_set_text(cgmtime_layer, formatted_cgm_timeago);
  }
  
} // end set_cgm_timeago

static void load_cgmtime() {
    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD CGMTIME FUNCTION START");
	
    // CONSTANTS
    const uint16_t HOURAGO = 60*(60);
    const uint32_t DAYAGO = 24*(60*60);
    const uint32_t WEEKAGO = 7*(24*60*60);
  
    // VARIABLES
    
    static uint32_t cgm_time_offset = 0;
    int cgm_timeago_diff = 0;

    time_t current_temp_time = time(NULL);
    struct tm *current_local_time = localtime(&current_temp_time);
    size_t draw_cgm_time = 0;
    static char cgm_time_text[] = "00:00";
    
    // CODE START
	
    // initialize color
    text_layer_set_text_color(cgmtime_layer, text_default_color);
       
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, NEW CGM TIME: %lu", current_cgm_time);
    
    if (current_cgm_time == 0) {     
      // Init code or error code; set text layer & icon to empty value 
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, CGM TIME AGO INIT OR ERROR CODE: %s", cgm_label_buffer);
      // clear cgm timeago icon and set init flag
      clear_cgm_timeago();
    }
    else {    
   
      cgm_time_now = time(NULL);
      
      if ((init_loading_cgm_timeago == 111) && (PhoneOffAlert == 100)) {
        //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD CGMTIME, INIT CGM TIMEAGO SHOW LAST TIME");
        current_temp_time = current_cgm_time;
        current_local_time = localtime(&current_temp_time);
        draw_cgm_time = strftime(cgm_time_text, TIME_TEXTBUFF_SIZE, "%l:%M", current_local_time);
        if (draw_cgm_time != 0) {
          text_layer_set_text(cgmtime_layer, cgm_time_text);
        }
      }
      
      // display cgm_timeago as now to 5m always, no matter what the difference is by using an offset
      if (stored_cgm_time == current_cgm_time) {
          // stored time is same as incoming time, so display timeago
          current_cgm_timeago = (abs((abs(cgm_time_now - current_cgm_time)) - cgm_time_offset));
      }
      else {
        // got new cgm time, set loading flags and get offset
        if ((stored_cgm_time != 0) && (BluetoothAlert == 100) && (PhoneOffAlert == 100) && 
          (AppSyncErrAlert == 100)) {
          init_loading_cgm_timeago = 100;
        }
        
        // get offset
        stored_cgm_time = current_cgm_time;
        current_cgm_timeago = 0;
        cgm_time_offset = abs(cgm_time_now - current_cgm_time); 
        
      }

        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, CURRENT CGM TIME: %lu", current_cgm_time);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, STORED CGM TIME: %lu", stored_cgm_time);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, TIME NOW IN CGM: %lu", cgm_time_now);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, CGM TIME OFFSET: %lu", cgm_time_offset);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, CURRENT CGM TIMEAGO: %lu", current_cgm_timeago);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, INIT LOADING BOOL: %d", init_loading_cgm_timeago);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, GM TIME AGO LABEL IN: %s", cgm_label_buffer);
      
      
	    // if not in initial cgm timeago, set rcvr on icon and time label
      if ((init_loading_cgm_timeago == 100) && (BluetoothAlert == 100) && (PhoneOffAlert == 100)) {
        create_update_bitmap(&cgmicon_bitmap,cgmicon_layer,TIMEAGO_ICONS[RCVRON_ICON_INDX]);
      
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, CURRENT CGM TIME: %lu", current_cgm_time);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, STORED CGM TIME: %lu", stored_cgm_time);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, TIME NOW IN CGM: %lu", cgm_time_now);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, CGM TIME OFFSET: %lu", cgm_time_offset);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, CURRENT CGM TIMEAGO: %lu", current_cgm_timeago);
      
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, GM TIME AGO LABEL IN: %s", cgm_label_buffer);
      
        if (current_cgm_timeago < MINUTEAGO) {
          cgm_timeago_diff = 0;
          set_cgm_timeago("now", cgm_timeago_diff, true, "");
          // We've cleared Check Rig, so make sure reset flag is set.
          CGMOffAlert = 100;
        }
        else if (current_cgm_timeago < HOURAGO) {
          cgm_timeago_diff = (current_cgm_timeago / MINUTEAGO);
          set_cgm_timeago("", cgm_timeago_diff, false, "m");
        }
        else if (current_cgm_timeago < DAYAGO) {
          cgm_timeago_diff = (current_cgm_timeago / HOURAGO);
          set_cgm_timeago("", cgm_timeago_diff, false, "h");
        }
        else if (current_cgm_timeago < WEEKAGO) {
          cgm_timeago_diff = (current_cgm_timeago / DAYAGO);
          set_cgm_timeago("", cgm_timeago_diff, false, "d");
        }
        else {
          // clear cgm timeago icon and set init flag
          clear_cgm_timeago();
        } 
      }
      
      // check to see if we need to show receiver off icon
      if ( ((cgm_timeago_diff >= CGMOUT_WAIT_MIN) || ((strcmp(cgm_label_buffer, "") != 0) && (strcmp(cgm_label_buffer, "m") != 0))) 
      || ( ( ((current_cgm_timeago < TWOYEARSAGO) && ((current_cgm_timeago / MINUTEAGO) >= CGMOUT_INIT_WAIT_MIN)) 
          || ((strcmp(cgm_label_buffer, "") != 0) && (strcmp(cgm_label_buffer, "m") != 0)) )
           && (init_loading_cgm_timeago == 111) && (ClearedOutage == 100) && (ClearedBTOutage == 100) ) ) {
	      // set receiver off icon
	      //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, SET RCVR OFF ICON, CGM TIMEAGO DIFF: %d", cgm_timeago_diff);
	      //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, SET RCVR OFF ICON, LABEL: %s", cgm_label_buffer);
        if (init_loading_cgm_timeago == 100) {
          #ifdef PBL_COLOR
          text_layer_set_text_color(cgmtime_layer, text_urgent_color); 
          #endif
	        create_update_bitmap(&cgmicon_bitmap,cgmicon_layer,TIMEAGO_ICONS[RCVROFF_ICON_INDX]);
        }       
	      // Vibrate if we need to
	      if ((BluetoothAlert == 100) && (PhoneOffAlert == 100) && (CGMOffAlert == 100) && 
            (ClearedOutage == 100) && (ClearedBTOutage == 100)) {
	        //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD CGMTIME, CGM TIMEAGO: VIBRATE");
	        alert_handler_cgm(CGMOUT_VIBE);
	        CGMOffAlert = 111;
          set_message_layer("CHECK RIG\0", "", false, text_urgent_color);
	      } // if CGMOffAlert       
      } // if CGM_OUT_MIN     
	    else {
        if ((CGMOffAlert == 111) && (cgm_timeago_diff != 0)) { 
          ClearedOutage = 111;
          //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, SET CLEARED OUTAGE: %i ", ClearedOutage);
        } 
        // CGM is not out, reset CGMOffAlert
        CGMOffAlert = 100;
      } // else CGM_OUT_MIN
      
    } // else init code

    //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD CGMTIME, CGM TIMEAGO LABEL OUT: %s", cgm_label_buffer);
} // end load_cgmtime

static void load_apptime(){
    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD APPTIME, READ APP TIME FUNCTION START");
	
	// VARIABLES
	uint32_t current_app_timeago = 0;
	int app_timeago_diff = 0;
  
	// CODE START
	draw_date_from_app();  
       
      app_time_now = time(NULL);
      
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD APPTIME, TIME NOW: %lu", app_time_now);
      
      current_app_timeago = abs(app_time_now - current_app_time);
      
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD APPTIME, CURRENT APP TIMEAGO: %lu", current_app_timeago);
      
	  app_timeago_diff = (current_app_timeago / MINUTEAGO);
	  if ( (current_app_timeago < TWOYEARSAGO) && (app_timeago_diff >= PHONEOUT_WAIT_MIN) ) {
              
        // clear cgm timeago icon and set init flag
        clear_cgm_timeago();
      
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD APPTIME, SET init_loading_cgm_timeago: %i", init_loading_cgm_timeago);
		
		  //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD APPTIME, CHECK IF HAVE TO VIBRATE");
		  // Vibrate if we need to
		  if ((BluetoothAlert == 100) && (PhoneOffAlert == 100) && 
          (ClearedOutage == 100) && (ClearedBTOutage == 100)) {
		    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD APPTIME, READ APP TIMEAGO: VIBRATE");
		    alert_handler_cgm(PHONEOUT_VIBE);
		    PhoneOffAlert = 111;
        set_message_layer("CHECK PHONE\0", "", false, text_urgent_color);
		  }
	  }
	  else {
	    // reset PhoneOffAlert    
      if (PhoneOffAlert == 111) {
        ClearedOutage = 111;
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD APPTIME, SET CLEARED OUTAGE: %i ", ClearedOutage);
      } 
      PhoneOffAlert = 100;
    }		
  
	//APP_LOG(APP_LOG_LEVEL_INFO, "LOAD APPTIME, FUNCTION OUT");
} // end load_apptime

static void set_bg_delta(char *bg_delta_string, char *bg_delta_buffer, bool use_bg_delta_string, char *bg_delta_label) {
  
  // CONSTANTS
	const uint8_t BGDELTA_LABEL_SIZE = 14;
	const uint8_t BGDELTA_FORMATTED_SIZE = 14;
  
  // VARIABLES
  static char formatted_bg_delta[14] = {0};
  
  char delta_label_buffer[14] = {0};
  
  if (use_bg_delta_string) { strncpy(formatted_bg_delta, (char *)bg_delta_string, BGDELTA_FORMATTED_SIZE); }
  else { strncpy(formatted_bg_delta, (char *)bg_delta_buffer, BGDELTA_FORMATTED_SIZE); }
 
  strncpy(delta_label_buffer, (char *)bg_delta_label, BGDELTA_LABEL_SIZE);
	strcat(formatted_bg_delta, delta_label_buffer);
  set_message_layer("", formatted_bg_delta, true, text_default_color);
  
} // end set_bg_delta

static void load_bg_delta() {
	//APP_LOG(APP_LOG_LEVEL_INFO, "BG DELTA FUNCTION START");
	
	// CODE START
  
  // check bluetooth connection
  //bluetooth_connected_cgm = connection_service_peek_pebble_app_connection();
  bluetooth_connected_cgm = bluetooth_connection_service_peek();
  
	if ((!bluetooth_connected_cgm) || (BluetoothAlert == 111)) {
	  // Bluetooth is out; BT message already set, so return
	  return;
	}
  
	// check for CHECK PHONE condition, if true set message
	if ((PhoneOffAlert == 111) && (ClearedOutage == 100) && (ClearedBTOutage == 100)) {
    set_message_layer("CHECK PHONE\0", "", false, text_urgent_color);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD BG DELTA MSG, init_loading_cgm_timeago: %i", init_loading_cgm_timeago);
    return;	
	}

	// check for CHECK CGM condition, if true set message
    
	if ((CGMOffAlert == 111) && (ClearedOutage == 100) && (ClearedBTOutage == 100) && 
      (current_cgm_timeago != 0) && (stored_cgm_time == current_cgm_time)) {
    set_message_layer("CHECK RIG\0", "", false, text_urgent_color);  
    return;	
	}
  
  // Clear out any remaining CHECK RIG condition
  if ((CGMOffAlert == 111) && (current_cgm_timeago == 0)) {
	  init_loading_cgm_timeago = 100;
    return;
  }
	
	// check for special messages; if no string, set no message
	if (strcmp(current_bg_delta, "") == 0) {
      set_message_layer("\0", "", false, text_default_color);
      return;	
    }
	
  	// check for NO ENDPOINT condition, if true set message
	// put " " (space) in bg field so logo continues to show
	if (strcmp(current_bg_delta, "NOEP") == 0) {
      set_message_layer("NO ENDPOINT\0", "", false, text_urgent_color);
      set_special_value(LOGO_SPECVALUE_ICON_INDX, " \0", 100);
      return;	
	}

  // check for COMPRESSION (compression low) condition, if true set message
	if (strcmp(current_bg_delta, "PRSS") == 0) {
      set_message_layer("COMPRESSION?\0", "", false, text_urgent_color);
      return;	
	}
  
  	// check for DATA OFFLINE condition, if true set message to fix condition	
	if (strcmp(current_bg_delta, "OFF") == 0) {
    if (dataoffline_retries_counter >= DATAOFFLINE_RETRIES_MAX) {
      set_message_layer("ATTN: NO DATA\0", "", false, text_urgent_color);
      text_layer_set_text(bg_layer, " \0");
      if (DataOfflineAlert == 100) {
        //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG DELTA, DATA OFFLINE, VIBRATE");
        alert_handler_cgm(DATAOFFLINE_VIBE);
        DataOfflineAlert = 111;
      } // DataOfflineAlert
      // NOTE: DataOfflineAlert is cleared in load_icon because that means we got a good message again
      // NOTE: dataoffline_retries_counter is cleared in load_icon because that means we got a good message again
    }  
    else {
      dataoffline_retries_counter++; 
	  }  
	  return;	
  } // strcmp "OFF"
  
  	// check if LOADING.., if true set message
  	// put " " (space) in bg field so logo continues to show
    if (strcmp(current_bg_delta, "LOAD") == 0) {
      set_message_layer("LOADING 8.0\0", "", false, text_default_color);
      set_special_value(LOGO_SPECVALUE_ICON_INDX, " \0", 100);
      return;
    }
 
	// check for zero delta here; if get later then we know we have an error instead
    if (strcmp(current_bg_delta, "0") == 0) {
      set_bg_delta ("0", "", true, " mg/dL");
      return;	
    }
  
    if (strcmp(current_bg_delta, "0.0") == 0) {
      set_bg_delta ("0.0", "", true, " mmol");
      return;	
    }
  
	// check to see if we have MG/DL or MMOL
	// get currentBG_isMMOL in myBGAtoi
	converted_bgDelta = myBGAtoi(current_bg_delta);
 
	// Bluetooth is good, Phone is good, CGM connection is good, no special message 
	// set delta BG message
	
	// zero here, means we have an error instead; set error message
    if (converted_bgDelta == 0) {
      //strncpy(formatted_bg_delta, "BG DELTA ERR", BGDELTA_FORMATTED_SIZE);
      set_message_layer("BG DELTA ERROR\0", "", false, text_urgent_color);
      return;
    }
  
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD BG DELTA, DELTA STRING: %s", &current_bg_delta[i]);
    if (currentBG_isMMOL == 100) {
	  // set mg/dL string
	  //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG DELTA: FOUND MG/DL, SET STRING");
	    if (converted_bgDelta >= 100) {
		  // bg delta too big, set zero instead
        set_bg_delta ("0", "", true, " mg/dL");
	    }
	    else {
        set_bg_delta ("", current_bg_delta, false, " mg/dL");
	    }
	}
	else {
	  // set mmol string
	  //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BG DELTA: FOUND MMOL, SET STRING");
	  if (converted_bgDelta >= 55) {
	    // bg delta too big, set zero instead
      set_bg_delta ("0.0", "", true, " mmol");
	  }
	  else {
      set_bg_delta ("", current_bg_delta, false, " mmol");
	  }
	}
	if (DoubleDownAlert == 111) { 
    #ifdef PBL_COLOR
    text_layer_set_text_color(message_layer, text_urgent_color); 
    #endif
  }
	
} // end load_bg_delta

static void load_rig_battlevel() {
  //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BATTLEVEL, FUNCTION START");
	
	// VARIABLES
  static char formatted_battlevel[8] = {0};
  static uint8_t LowBatteryAlert = 100;
  
	uint8_t current_battlevel = 0;

	// CODE START
  text_layer_set_text_color(rig_battlevel_layer, text_default_color);

	//APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD BATTLEVEL, LAST BATTLEVEL: %s", last_battlevel);
  
	if (strcmp(last_battlevel, " ") == 0) {
      // Init code or no battery, can't do battery; set text layer & icon to empty value 
      //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BATTLEVEL, NO BATTERY");
      text_layer_set_text(rig_battlevel_layer, "\0");
      LowBatteryAlert = 100;	
      return;
    }
  
	if (strcmp(last_battlevel, "0") == 0) {
      // Zero battery level; set here, so if we get zero later we know we have an error instead
      //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BATTLEVEL, ZERO BATTERY, SET STRING");
      #ifdef PBL_COLOR
      text_layer_set_text_color(rig_battlevel_layer, text_urgent_color); 
      #endif
      text_layer_set_text(rig_battlevel_layer, " Rig \U0001F44E\0");
      if (LowBatteryAlert == 100) {
		    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BATTLEVEL, ZERO BATTERY, VIBRATE");
		    alert_handler_cgm(LOWBATTERY_VIBE);
		    LowBatteryAlert = 111;
      }	 
      return;
    }
  
	current_battlevel = atoi(last_battlevel);
  
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD BATTLEVEL, CURRENT BATTLEVEL: %i", current_battlevel);
  
	if ((current_battlevel <= 0) || (current_battlevel > 100) || (last_battlevel[0] == '-')) { 
    // got a negative or out of bounds or error battery level
	  //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BATTLEVEL, UNKNOWN, ERROR BATTERY");
    text_layer_set_text(rig_battlevel_layer, "Rig \0");
    #ifdef PBL_COLOR
    text_layer_set_text_color(rig_battlevel_layer, text_urgent_color); 
    #endif
    return;
	}
  
  // display battlevel
  
  if (batterydisplay_value == 1) {
  // want number display
  
    if ( (current_battlevel >= 99) && (current_battlevel <= 100) ) {
      text_layer_set_text(rig_battlevel_layer, "Rig GO!\0");
    } 
    else {
      snprintf(formatted_battlevel, BATTLEVEL_FORMAT_SIZE, "Rig %d", current_battlevel);
      text_layer_set_text(rig_battlevel_layer, formatted_battlevel);
    }
  }
  
  else {
  // want emoji display
    
  // initialize status to thumbs down emoji
  text_layer_set_text(rig_battlevel_layer, " Rig \U0001F44E\0");
    
  // initialized to thumbs down emoji. set others states as necessary.
  if ( (current_battlevel >= 99) && (current_battlevel <= 100) ) {
    // thumbs up emoji
    text_layer_set_text(rig_battlevel_layer, " Rig \U0001F44D\0");
  }
  else if ( (current_battlevel > 60) && (current_battlevel < 99) ) {
    // happy face emoji
    text_layer_set_text(rig_battlevel_layer, " Rig  \U0001F603\0");
  }
  else if ( (current_battlevel > 40) && (current_battlevel <= 60) ) {
    // straight line face emoji
    text_layer_set_text(rig_battlevel_layer, " Rig  \U0001F610\0");
  }
    // sad or tired face emoji
  else if ( (current_battlevel > 20) && (current_battlevel <= 40) ) {
    text_layer_set_text(rig_battlevel_layer, " Rig  \U0001F625\0");
  }  
    // very tired emoji
  else if ( (current_battlevel > 10) && (current_battlevel <= 20) ) {
    text_layer_set_text(rig_battlevel_layer, " Rig  \U0001F629\0");
  }  
  else if ( (current_battlevel > 0) && (current_battlevel <= 10) ) { 
    // thumbs down emoji
    text_layer_set_text(rig_battlevel_layer, " Rig \U0001F44E\0");
  }
  }
    
  // vibrate on battlevel if needed
	if ( (current_battlevel > 10) && (current_battlevel <= 20) ) {
    #ifdef PBL_COLOR
    text_layer_set_text_color(rig_battlevel_layer, text_urgent_color); 
    #endif
    if (LowBatteryAlert == 100) {
	    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BATTLEVEL, LOW BATTERY, 20 OR LESS, VIBRATE");
	    alert_handler_cgm(LOWBATTERY_VIBE);
	    LowBatteryAlert = 111;
    }
	}
  
	if ( (current_battlevel > 5) && (current_battlevel <= 10) ) {
    #ifdef PBL_COLOR
    text_layer_set_text_color(rig_battlevel_layer, text_urgent_color); 
    #endif
    if (LowBatteryAlert == 100) {
	    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BATTLEVEL, LOW BATTERY, 10 OR LESS, VIBRATE");
	    alert_handler_cgm(LOWBATTERY_VIBE);
	    LowBatteryAlert = 111;
    }
  }
  
	if ( (current_battlevel > 0) && (current_battlevel <= 5) ) {
    #ifdef PBL_COLOR
    text_layer_set_text_color(rig_battlevel_layer, text_urgent_color); 
    #endif
    if (LowBatteryAlert == 100) {
	    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BATTLEVEL, LOW BATTERY, 5 OR LESS, VIBRATE");
	    alert_handler_cgm(LOWBATTERY_VIBE);
	    LowBatteryAlert = 111;
    }	  
  }
	
	//APP_LOG(APP_LOG_LEVEL_INFO, "LOAD BATTLEVEL, END FUNCTION");
} // end load_rig_battlevel

static void load_noise() {
    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD NOISE, FUNCTION START");

  	// CONSTANTS
  	#define NO_NOISE 0
  	#define CLEAN_NOISE 1
  	#define LIGHT_NOISE 2
  	#define MEDIUM_NOISE 3
  	#define HEAVY_NOISE 4
  	#define WARMUP_NOISE 5
  	#define OTHER_NOISE 6  
  
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD NOISE, CURRENT NOISE VALUE: %i ", current_noise_value);  
  
    // for testing only
    //current_noise_value = LIGHT_NOISE;
  
    text_layer_set_text_color(noise_layer, text_default_color);
  
    switch (current_noise_value) {
      
    case NO_NOISE:;
      text_layer_set_text(noise_layer, " \0");
      break;
    case CLEAN_NOISE:;
      text_layer_set_text(noise_layer, "Cln\0");
      break;
    case LIGHT_NOISE:;
      text_layer_set_text(noise_layer, "Lgt\0");
      break;
    case MEDIUM_NOISE:;
      text_layer_set_text(noise_layer, "Med\0");
      break;
    case HEAVY_NOISE:;
      #ifdef PBL_COLOR
      text_layer_set_text_color(noise_layer, text_urgent_color); 
      #endif
      text_layer_set_text(noise_layer, "Hvy\0");
      break;
    case WARMUP_NOISE:;
      text_layer_set_text(noise_layer, " \0");
      break;
    case OTHER_NOISE:;
      text_layer_set_text(noise_layer, " \0");
      break;
    default:;
      #ifdef PBL_COLOR
      text_layer_set_text_color(noise_layer, text_urgent_color); 
      #endif
      text_layer_set_text(noise_layer, " \0");
    }
  
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "SYNC TUPLE, NOISE: %s ", formatted_noise);
  
    //text_layer_set_text(noise_layer, formatted_noise);
  
	//APP_LOG(APP_LOG_LEVEL_INFO, "LOAD NOISE, END FUNCTION");
} // end load_noise

static void load_t1dname() {
  
   //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD T1DNAME, FUNCTION START");
 
  // CONSTANTS
  const uint8_t T1DNAME_FORMATTED_SIZE = 52;
  const uint8_t EMOJI_FORMATTED_SIZE = 13;
  
  // VARIABLES
  static char formatted_t1dname[52] = {0};
  char emoji_string[13] = {0};
  
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD T1DNAME, CURRENT FEELING VALUE: %i ", feeling_value);
  if (HardCodeNoVibrations == 111) {
    strncpy(emoji_string, " \U0001F634 ", EMOJI_FORMATTED_SIZE);
  }
  else {
  switch (feeling_value) {
    case 0:; strncpy(emoji_string, " \U0001F603 ", EMOJI_FORMATTED_SIZE); break;
    case 1:; strncpy(emoji_string, " \U0001F602 ", EMOJI_FORMATTED_SIZE); break;
    case 2:; strncpy(emoji_string, " \U0001F60B ", EMOJI_FORMATTED_SIZE); break;
    case 3:; strncpy(emoji_string, " \U0001F60D ", EMOJI_FORMATTED_SIZE); break;
    case 4:; strncpy(emoji_string, " \U0001F607 ", EMOJI_FORMATTED_SIZE); break;
    case 5:; strncpy(emoji_string, " \U0001F495 ", EMOJI_FORMATTED_SIZE); break;
    case 6:; strncpy(emoji_string, " \U0001F64F ", EMOJI_FORMATTED_SIZE); break;
    case 7:; strncpy(emoji_string, " \U0001F389 ", EMOJI_FORMATTED_SIZE); break;
    case 8:; strncpy(emoji_string, " \U0001F60E ", EMOJI_FORMATTED_SIZE); break;
    case 9:; strncpy(emoji_string, " \U0001F425 ", EMOJI_FORMATTED_SIZE); break;
    case 10:; strncpy(emoji_string, " \U0001F621 ", EMOJI_FORMATTED_SIZE); break;
    case 11:; strncpy(emoji_string, " \U0001F4A9 ", EMOJI_FORMATTED_SIZE); break;
    case 12:; strncpy(emoji_string, " \U0001F37B ", EMOJI_FORMATTED_SIZE); break;
    default:; strncpy(emoji_string, " \U0001F603 ", EMOJI_FORMATTED_SIZE); break;
  }
  }
  
  strncpy(formatted_t1dname, emoji_string, T1DNAME_FORMATTED_SIZE);
  strcat(formatted_t1dname, current_t1dname);
  strcat(formatted_t1dname, emoji_string);
  text_layer_set_text(t1dname_layer, formatted_t1dname);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD T1DNAME, END FUNCTION");
} // end load_t1dname

static void set_mainfont_layer (bool set_digi_layer, bool set_classic_layer) {
  
    layer_set_hidden(text_layer_get_layer(time_watch_digi_layer), set_digi_layer);
    layer_set_hidden(text_layer_get_layer(calcraw_last1_digi_layer), set_digi_layer);
    layer_set_hidden(text_layer_get_layer(calcraw_last2_digi_layer), set_digi_layer);
    layer_set_hidden(text_layer_get_layer(calcraw_last3_digi_layer), set_digi_layer);
    
    layer_set_hidden(text_layer_get_layer(time_watch_classic_layer), set_classic_layer);
    layer_set_hidden(text_layer_get_layer(calcraw_last1_classic_layer), set_classic_layer);
    layer_set_hidden(text_layer_get_layer(calcraw_last2_classic_layer), set_classic_layer);
    layer_set_hidden(text_layer_get_layer(calcraw_last3_classic_layer), set_classic_layer);
  
} // end set_mainfont_layer

static void load_mainfont() {
  //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD MAIN FONT, FUNCTION START");
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD MAIN FONT, CURRENT MAIN FONT VALUE: %i ", mainfont_value);
  
  switch (mainfont_value) {
  case 0:;
    // Digital Font
    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD MAIN FONT, SELECTED DIGITAL FONT");
    text_layer_set_font(bg_layer, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
    
    set_mainfont_layer (false, true);
    
    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD MAIN FONT, LOADED DIGITAL FONT");
    break;
    
  case 1:;
    // Classic Font
    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD MAIN FONT, SELECTED CLASSIC FONT");
    
    text_layer_set_font(bg_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD MAIN FONT, LOADING CLASSIC FONT, SET BG FONT");
    
    set_mainfont_layer (true, false);
    
    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD MAIN FONT, LOADED CLASSIC FONT");
    break;
  }
  
} // end load_mainfont

static void load_color() {
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD COLOR, FUNCTION START");
  int number_of_colors = 2;
  
  #ifdef PBL_PLATFORM_APLITE
    number_of_colors = 2;
    top_color = GColorWhite;
    bottom_color = GColorWhite;
    text_default_color = GColorBlack;
    text_warning_color = GColorBlack;
    text_urgent_color = GColorBlack;
  
  #else
  // can select colors
  
  switch (color_value)  {
    // Zebra
    case 0:; 
      number_of_colors = 2;
      top_color = GColorWhite;
      bottom_color = GColorWhite;
      text_default_color = GColorBlack;
      text_warning_color = GColorBlack;
      text_urgent_color = GColorBlack;
      break;
    // Sunshine
    case 1:;
      top_color = GColorIcterine;
      bottom_color = GColorChromeYellow;
      text_default_color = GColorBlack;
      text_warning_color = GColorDarkCandyAppleRed;
      text_urgent_color = GColorDarkCandyAppleRed;
      break;
    // Ocean
    case 2:;
      top_color = GColorCeleste;
      bottom_color = GColorCadetBlue;
      text_default_color = GColorBlack;
      text_warning_color = GColorDarkCandyAppleRed;
      text_urgent_color = GColorDarkCandyAppleRed;
      break;
    // Mahogany
    case 3:;
      top_color = GColorPastelYellow;
      bottom_color = GColorWindsorTan;
      text_default_color = GColorBlack;
      text_warning_color = GColorBulgarianRose;
      text_urgent_color = GColorBulgarianRose;
      break;
    // Forest
    case 4:;
      top_color = GColorInchworm;
      bottom_color = GColorKellyGreen;
      text_default_color = GColorBlack;
      text_warning_color = GColorBulgarianRose;
      text_urgent_color = GColorBulgarianRose;
      break;
    // Unicorn
    case 5:;
      top_color = GColorRichBrilliantLavender;
      bottom_color = GColorPurpureus;
      text_default_color = GColorBlack;
      text_warning_color = GColorBulgarianRose;
      text_urgent_color = GColorBulgarianRose;
      break;
    // Rainbow
    case 6:;
      number_of_colors = 6;
      top_color = GColorSunsetOrange;
      topleft_color = GColorChromeYellow;
      topright_color = GColorIcterine;
      bottomleft_color = GColorInchworm;
      bottomright_color = GColorCeleste;
      bottom_color = GColorPurpureus;
      text_default_color = GColorBlack;
      text_warning_color = GColorDarkCandyAppleRed;
      text_urgent_color = GColorDarkCandyAppleRed;
      break;
    }
    #endif

    if (number_of_colors <= 2) {
      topleft_color = bottom_color;
      topright_color = bottom_color;
      bottomleft_color = top_color;
      bottomright_color = top_color;
    }
  
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD COLOR, CURRENT TOP COLOR: %d ", top_color);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "LOAD COLOR, CURRENT BOTTOM COLOR: %d ", bottom_color);
  
    // do stuff with top and bottom colors
    layer_mark_dirty(rectangle_layers);
  
    //APP_LOG(APP_LOG_LEVEL_INFO, "LOAD COLOR, END FUNCTION");
} // end load_color

void sync_tuple_changed_callback_cgm(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
	//APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE");
	
  // VARIABLES
  uint8_t need_to_reset_outage_flag = 100;
  uint8_t get_new_cgm_time = 100;
  
	// CONSTANTS
	const uint8_t ICON_MSGSTR_SIZE = 4;
	const uint8_t BG_MSGSTR_SIZE = 6;
	const uint8_t BGDELTA_MSGSTR_SIZE = 6;
	const uint8_t BATTLEVEL_MSGSTR_SIZE = 4;
	const uint8_t VALUE_MSGSTR_SIZE = 60;
  const uint8_t T1DNAME_MSGSTR_SIZE = 25;

	// CODE START
  
  // reset appsync retries counter
  appsyncandmsg_retries_counter = 0;
  
  //parse key and tuple
	switch (key) {

	case CGM_ICON_KEY:;
      //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: ICON ARROW");
      strncpy(current_icon, new_tuple->value->cstring, ICON_MSGSTR_SIZE);
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "SYNC TUPLE, ICON VALUE: %s ", current_icon);
	  load_icon();
      break; // break for CGM_ICON_KEY

	case CGM_BG_KEY:;
	  //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: BG CURRENT");
      strncpy(last_bg, new_tuple->value->cstring, BG_MSGSTR_SIZE);
	  //APP_LOG(APP_LOG_LEVEL_DEBUG, "SYNC TUPLE, BG VALUE: %s ", last_bg);
      load_bg();
      break; // break for CGM_BG_KEY

	case CGM_TCGM_KEY:;
      //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: READ CGM TIME");
      current_cgm_time = new_tuple->value->uint32;
      cgm_time_now = time(NULL);
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "SYNC TUPLE, CLEARED OUTAGE IN: %i ", ClearedOutage);
      
      // set up proper CGM time before calling load CGM time
      if ( ((ClearedOutage == 111) || (ClearedBTOutage == 111)) && (stored_cgm_time != 0)) {
        //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: LOAD CGM TIME, CLEARED OUTAGE");
        stored_cgm_time = current_cgm_time;
        current_cgm_timeago = 0;
        init_loading_cgm_timeago = 111;
        need_to_reset_outage_flag = 111;
        
      }
      // get stored cgm time again for bluetooth race condition
      if (get_new_cgm_time == 111) { 
         stored_cgm_time = current_cgm_time;
         current_cgm_timeago = 0;
        // reset flag
         get_new_cgm_time = 100;
      }
    
      // clear CHECK RIG message if still there
      if ((CGMOffAlert == 111) && (need_to_reset_outage_flag = 111) && (stored_cgm_time != current_cgm_time)) {
        load_bg_delta();
      }
     
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "SYNC TUPLE, CURRENT CGM TIME: %lu ", current_cgm_time);
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "SYNC TUPLE, STORED CGM TIME: %lu ", stored_cgm_time);
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "SYNC TUPLE, TIME NOW: %lu ", cgm_time_now);
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "SYNC TUPLE, CLEARED OUTAGE OUT: %i ", ClearedOutage);
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "SYNC TUPLE, CLEARED BT OUTAGE OUT: %i ", ClearedBTOutage);
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "SYNC TUPLE, CURRENT CGM TIMEAGO: %lu ", current_cgm_timeago);
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "SYNC TUPLE, CURRENT CGM TIMEAGO DIFF: %i ", cgm_timeago_diff);   
    
      load_cgmtime();
 
      // if just cleared an outage, reset flags
      if (need_to_reset_outage_flag == 111) {
        // reset stored cgm_time for bluetooth race condition
        if (ClearedBTOutage == 111) { 
            // just cleared a BT outage, so make sure we are still in init_loading
            //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: LOAD CGM TIME, CLEARED BT OUTAGE");
            init_loading_cgm_timeago = 111;
            // set get new CGM time flag
            get_new_cgm_time = 111;
        }
        // reset the ClearedOutages flag
        ClearedOutage = 100;
        ClearedBTOutage = 100;      
        // reset outage flag
        need_to_reset_outage_flag = 100;
      }
    
      //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: READ CGM TIME OUT");
      break; // break for CGM_TCGM_KEY

	case CGM_TAPP_KEY:;
      //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: READ APP TIME NOW");
      current_app_time = new_tuple->value->uint32;
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "SYNC TUPLE, APP TIME VALUE: %i ", current_app_time);
      load_apptime();    
      break; // break for CGM_TAPP_KEY

	case CGM_DLTA_KEY:;
   	  //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: BG DELTA");
	  strncpy(current_bg_delta, new_tuple->value->cstring, BGDELTA_MSGSTR_SIZE);
   	  //APP_LOG(APP_LOG_LEVEL_DEBUG, "SYNC TUPLE, BG DELTA VALUE: %s ", current_bg_delta);
	  load_bg_delta();
	  break; // break for CGM_DLTA_KEY
	
	case CGM_UBAT_KEY:;
   	  //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: UPLOADER BATTERY LEVEL");
      strncpy(last_battlevel, new_tuple->value->cstring, BATTLEVEL_MSGSTR_SIZE);
   	  //APP_LOG(APP_LOG_LEVEL_DEBUG, "SYNC TUPLE, BATTERY LEVEL VALUE: %s ", last_battlevel);
      load_rig_battlevel();
      break; // break for CGM_UBAT_KEY

	case CGM_NAME_KEY:;
      //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: T1D NAME");
      strncpy(current_t1dname, new_tuple->value->cstring, T1DNAME_MSGSTR_SIZE);
      load_t1dname();
      break; // break for CGM_NAME_KEY
    
  case CGM_VALS_KEY:;
      //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: VALUES");
      strncpy(current_values, new_tuple->value->cstring, VALUE_MSGSTR_SIZE);
      load_values();
      //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: LOADED VALUES");
      load_mainfont();
      //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: LOADED MAIN FONT");
      load_color();
      //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: LOADED COLOR");
      break; // break for CGM_VALS_KEY
    
  case CGM_CLRW_KEY:;
      //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: CALCULATED RAW");
      strncpy(last_calc_raw, new_tuple->value->cstring, BG_MSGSTR_SIZE);
      if ( (strcmp(last_calc_raw, "0") == 0) || (strcmp(last_calc_raw, "0.0") == 0) || (TurnOnCalcRaw == 100) ) {
        strncpy(last_calc_raw, " ", BG_MSGSTR_SIZE);
        HaveCalcRaw = 100;
      }
      else { HaveCalcRaw = 111; }  
      text_layer_set_text(raw_calc_layer, last_calc_raw);
      break; // break for CGM_CLRW_KEY
    
 	case CGM_RWUF_KEY:;
      //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: RAW UNFILTERED");
      strncpy(last_raw_unfilt, new_tuple->value->cstring, BG_MSGSTR_SIZE);
      if ( (strcmp(last_raw_unfilt, "0") == 0) || (strcmp(last_raw_unfilt, "0.0") == 0) || (TurnOnUnfilteredRaw == 100) ) {
        strncpy(last_raw_unfilt, " ", BG_MSGSTR_SIZE);
      }
      else { HaveURaw = 111; }
      text_layer_set_text(raw_unfilt_layer, last_raw_unfilt);
      break; // break for CGM_RWUF_KEY
    
  case CGM_NOIZ_KEY:;
      //APP_LOG(APP_LOG_LEVEL_INFO, "SYNC TUPLE: NOISE");
	    current_noise_value = new_tuple->value->uint8;
      //APP_LOG(APP_LOG_LEVEL_DEBUG, "SYNC TUPLE, NOISE: %i ", current_noise_value);
      load_noise();
      break; // break for CGM_NOIZ_KEY
  }  // end switch(key)

} // end sync_tuple_changed_callback_cgm()

static void send_cmd_cgm(void) {
  
  DictionaryIterator *iter = NULL;
  AppMessageResult sendcmd_openerr = APP_MSG_OK;
  AppMessageResult sendcmd_senderr = APP_MSG_OK;
  DictionaryResult sendcmd_dicterr = DICT_OK;
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "SEND CMD IN, ABOUT TO OPEN APP MSG OUTBOX");
  sendcmd_openerr = app_message_outbox_begin(&iter);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "SEND CMD, MSG OUTBOX OPEN, CHECK FOR ERROR");
  if (sendcmd_openerr != APP_MSG_OK) {
     //APP_LOG(APP_LOG_LEVEL_INFO, "WATCH SENDCMD OPEN ERROR");
     APP_LOG(APP_LOG_LEVEL_DEBUG, "WATCH SENDCMD OPEN ERR CODE: %i RES: %s", sendcmd_openerr, translate_app_error(sendcmd_openerr));
     sync_error_callback_cgm(sendcmd_dicterr, sendcmd_openerr, iter);
     return;
  }

  //APP_LOG(APP_LOG_LEVEL_INFO, "SEND CMD, MSG OUTBOX OPEN, NO ERROR, ABOUT TO SEND MSG TO APP");
  sendcmd_senderr = app_message_outbox_send();
  
  if (sendcmd_senderr != APP_MSG_OK) {
     //APP_LOG(APP_LOG_LEVEL_INFO, "WATCH SENDCMD SEND ERROR");
     APP_LOG(APP_LOG_LEVEL_DEBUG, "WATCH SENDCMD SEND ERR CODE: %i RES: %s", sendcmd_senderr, translate_app_error(sendcmd_senderr));
     sync_error_callback_cgm(sendcmd_dicterr, sendcmd_senderr, iter);
  }

  //APP_LOG(APP_LOG_LEVEL_INFO, "SEND CMD OUT, SENT MSG TO APP");
  
} // end send_cmd_cgm

void timer_callback_cgm(void *data) {

  //APP_LOG(APP_LOG_LEVEL_INFO, "TIMER CALLBACK IN, TIMER POP, ABOUT TO CALL SEND CMD");
  // reset msg timer to NULL
  null_and_cancel_timer (&timer_cgm, false);
  
  // send message
  send_cmd_cgm();
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "TIMER CALLBACK, SEND CMD DONE, ABOUT TO REGISTER TIMER");
  // set msg timer
  timer_cgm = app_timer_register((WATCH_MSGSEND_SECS*MS_IN_A_SECOND), timer_callback_cgm, NULL);

  //APP_LOG(APP_LOG_LEVEL_INFO, "TIMER CALLBACK, REGISTER TIMER DONE");
  
} // end timer_callback_cgm

void cgm_draw_rectangle (GContext* draw_ctx, GRect draw_bounds, GRect fill_bounds, GColor draw_color) {
  
  int rect_radius = 0;
  
  graphics_draw_rect(draw_ctx, draw_bounds);
  graphics_context_set_fill_color(draw_ctx, draw_color);
  graphics_fill_rect(draw_ctx, fill_bounds, rect_radius, GCornersAll);
  
} // cgm_draw_rectangle

void rectangle_layers_update_callback(Layer *me, GContext* ctx) {
  
  int draw_rect_width = 28;
  int draw_rect_height = 49;
  int fill_rect_width = 26;
  int fill_rect_height = 47;
  
  graphics_context_set_stroke_color(ctx, GColorBlack);
  
  // rectangles are built how many over, how many down, width, height
  
  // top rectangle for messages and delta
  cgm_draw_rectangle(ctx, GRect (1, 1, 142, 25), GRect (2, 2, 140, 23), top_color); 
  
  // bottom rectangle for name and date
  cgm_draw_rectangle(ctx, GRect (1, 127, 142, 40), GRect (2, 128, 140, 38), bottom_color); 
  
  // top left
  cgm_draw_rectangle(ctx, GRect (1, 27, draw_rect_width+3, draw_rect_height), 
                          GRect (2, 28, fill_rect_width+3, fill_rect_height), topleft_color); 
  
  // bottom left
  cgm_draw_rectangle(ctx, GRect (1, 77, draw_rect_width+3, draw_rect_height), 
                          GRect (2, 78, fill_rect_width+3, fill_rect_height), bottomleft_color); 
  
  // top right
  cgm_draw_rectangle(ctx, GRect (115, 27, draw_rect_width, draw_rect_height), 
                          GRect (116, 28, fill_rect_width, fill_rect_height), topright_color); 
  
  // bottom right
  cgm_draw_rectangle(ctx, GRect (115, 77, draw_rect_width, draw_rect_height), 
                          GRect (116, 78, fill_rect_width, fill_rect_height), bottomright_color); 
  
} // end rectangle_layers_update_callback

void window_cgm_add_text_layer (TextLayer **cgm_text_layer, GRect cgm_text_layer_pos, char *cgm_text_font) {
  
  *cgm_text_layer = text_layer_create(cgm_text_layer_pos);
  text_layer_set_text_color(*cgm_text_layer, text_default_color);
  text_layer_set_background_color(*cgm_text_layer, GColorClear);
  text_layer_set_font(*cgm_text_layer, fonts_get_system_font(cgm_text_font));
  text_layer_set_text_alignment(*cgm_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer_cgm, text_layer_get_layer(*cgm_text_layer));
  
} // end window_cgm_add_text_layer
  
void window_cgm_add_bitmap_layer (BitmapLayer **cgm_bitmap_layer, GRect cgm_bmap_layer_pos, GAlign bmap_align) {
  
  *cgm_bitmap_layer = bitmap_layer_create(cgm_bmap_layer_pos);
  bitmap_layer_set_alignment(*cgm_bitmap_layer, bmap_align);
  bitmap_layer_set_background_color(*cgm_bitmap_layer, GColorClear);
  bitmap_layer_set_compositing_mode(*cgm_bitmap_layer, GCompOpSet);
  layer_add_child(window_layer_cgm, bitmap_layer_get_layer(*cgm_bitmap_layer));

} // end window_cgm_add_bitmap_layer

void window_load_cgm(Window *window_cgm) {
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD IN");
  
  // VARIABLES
  
  // CODE START
  
  window_layer_cgm = window_get_root_layer(window_cgm);
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, GOT ROOT LAYER FOR WINDOW");
  
  GRect window_cgm_frame = layer_get_frame(window_layer_cgm);
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, GOT FRAME FOR WINDOW");
  
   // BG
  window_cgm_add_text_layer(&bg_layer, GRect(1, 17, 144, 47), FONT_KEY_LECO_42_NUMBERS);
  //window_cgm_add_text_layer(&bg_layer, GRect(2, 17, 144, 47), FONT_KEY_BITHAM_42_BOLD);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP BG LAYER");
  
  // BG IMAGE
  window_cgm_add_bitmap_layer(&bg_image_layer, GRect(2, 17, 144, 47), GAlignCenter);
  layer_set_hidden(bitmap_layer_get_layer(bg_image_layer),true);

  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP BG IMAGE LAYER");
  
  // ICON, ARROW OR SPECIAL VALUE
  window_cgm_add_bitmap_layer(&icon_layer, GRect(2, 58, 144, 50), GAlignCenter);

  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP ICON LAYER");
  
  // draw rectangles
  // init rectangle layers
  rectangle_layers = layer_create(window_cgm_frame);
  layer_set_update_proc(rectangle_layers, rectangle_layers_update_callback);
  layer_add_child(window_layer_cgm, rectangle_layers);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP RECTANGLE LAYERS");
  
   // DELTA BG / MESSAGE LAYER
  window_cgm_add_text_layer(&message_layer, GRect(0, -4, 144, 28), FONT_KEY_GOTHIC_24_BOLD);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP MESSAGE LAYER");
  
  // RIG BATTERY LEVEL
  window_cgm_add_text_layer(&rig_battlevel_layer, GRect(114, 23, 28, 62), FONT_KEY_GOTHIC_24_BOLD);
 
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP RIG BATTERY LAYER");
  
  // CALCULATED RAW INSTEAD OF BG - LAST VALUE (1)
  window_cgm_add_text_layer(&calcraw_last1_digi_layer, GRect(32, 21, 45, 25), FONT_KEY_LECO_20_BOLD_NUMBERS);
  window_cgm_add_text_layer(&calcraw_last1_classic_layer, GRect(32, 18, 40, 25), FONT_KEY_GOTHIC_24_BOLD);
  layer_set_hidden(text_layer_get_layer(calcraw_last1_classic_layer),true);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP CALCULATED RAW 1 LAYER");
  
  // CALCULATED RAW INSTEAD OF BG - 2ND LAST VALUE (2)
  window_cgm_add_text_layer(&calcraw_last2_digi_layer, GRect(52, 36, 45, 25), FONT_KEY_LECO_20_BOLD_NUMBERS);
  window_cgm_add_text_layer(&calcraw_last2_classic_layer, GRect(55, 33, 40, 25), FONT_KEY_GOTHIC_24_BOLD);
  layer_set_hidden(text_layer_get_layer(calcraw_last2_classic_layer),true);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP CALCULATED RAW 2 LAYER");
  
  // CALCULATED RAW INSTEAD OF BG - 3RD LAST VALUE (3)
  window_cgm_add_text_layer(&calcraw_last3_digi_layer, GRect(71, 51, 45, 25), FONT_KEY_LECO_20_BOLD_NUMBERS);
  window_cgm_add_text_layer(&calcraw_last3_classic_layer, GRect(77, 48, 40, 25), FONT_KEY_GOTHIC_24_BOLD);
  layer_set_hidden(text_layer_get_layer(calcraw_last3_classic_layer),true);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP CALCULATED RAW 3 LAYER");
  
  // CGM TIME AGO ICON
  window_cgm_add_bitmap_layer(&cgmicon_layer, GRect(4, 34, 50, 19), GAlignLeft); 
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP CGM TIME AGO ICON");
  
  // CGM TIME AGO READING
  window_cgm_add_text_layer(&cgmtime_layer, GRect(-2, 51, 36, 24), FONT_KEY_GOTHIC_18_BOLD);

  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP CGM TIME AGO READING");
  
  // T1D NAME
  window_cgm_add_text_layer(&t1dname_layer, GRect(0, 123, 144, 28), FONT_KEY_GOTHIC_24_BOLD);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP T1D NAME");
  
  // WATCH BATTERY LEVEL
  window_cgm_add_text_layer(&watch_battlevel_layer, GRect(114, 73, 28, 72), FONT_KEY_GOTHIC_24_BOLD);
  handle_watch_battery_cgm(battery_state_service_peek());

  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP WATCH BATTERY LEVEL");
  
  // TIME; CURRENT ACTUAL TIME FROM WATCH
  window_cgm_add_text_layer(&time_watch_digi_layer, GRect(2, 98, 144, 28), FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM);
  window_cgm_add_text_layer(&time_watch_classic_layer, GRect(2, 95, 144, 30), FONT_KEY_BITHAM_30_BLACK);
  layer_set_hidden(text_layer_get_layer(time_watch_classic_layer),true);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP TIME FOR WATCH");
  
  // DATE (has to be retrieved after time)
  window_cgm_add_text_layer(&date_app_layer, GRect(0, 146, 144, 24), FONT_KEY_GOTHIC_18_BOLD);
  draw_date_from_app();
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP DATE FOR WATCH");
  
  // RAW CALCULATED
  window_cgm_add_text_layer(&raw_calc_layer, GRect(-1, 70, 34, 24), FONT_KEY_GOTHIC_24_BOLD);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP CALCULATED RAW");
  
  // NOISE
 window_cgm_add_text_layer(&noise_layer, GRect(0, 92, 34, 16), FONT_KEY_GOTHIC_14_BOLD);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP NOISE");
  
  // RAW UNFILT
  window_cgm_add_text_layer(&raw_unfilt_layer, GRect(-1, 99, 34, 24), FONT_KEY_GOTHIC_24_BOLD);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP UNFILTERED RAW");
  
  // HAPPY MSG LAYER
  window_cgm_add_text_layer(&happymsg_layer, GRect(-10, 117, 144, 55), FONT_KEY_GOTHIC_24_BOLD);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP HAPPY MESSAGE");
  
  // put " " (space) in bg field so logo continues to show
  // " " (space) also shows these are init values, not bad or null values
  Tuplet initial_values_cgm[] = {
  TupletCString(CGM_ICON_KEY, " "),
	TupletCString(CGM_BG_KEY, " "),
	TupletInteger(CGM_TCGM_KEY, 0),
	TupletInteger(CGM_TAPP_KEY, 0),
	TupletCString(CGM_DLTA_KEY, "LOAD"),
	TupletCString(CGM_UBAT_KEY, " "),
	TupletCString(CGM_NAME_KEY, " "),
  TupletCString(CGM_VALS_KEY, " "),
  TupletCString(CGM_CLRW_KEY, " "),
  TupletCString(CGM_RWUF_KEY, " "),
  TupletInteger(CGM_NOIZ_KEY, 0)
  };
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, SET UP TUPLETS");
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, ABOUT TO CALL APP SYNC INIT");
  app_sync_init(&sync_cgm, sync_buffer_cgm, sizeof(sync_buffer_cgm), initial_values_cgm, ARRAY_LENGTH(initial_values_cgm), sync_tuple_changed_callback_cgm, sync_error_callback_cgm, NULL);
  
  // init timer to null if needed, and register timer
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, APP INIT DONE, ABOUT TO REGISTER TIMER");  
  null_and_cancel_timer (&timer_cgm, false);
  
  timer_cgm = app_timer_register((LOADING_MSGSEND_SECS*MS_IN_A_SECOND), timer_callback_cgm, NULL);
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD, TIMER REGISTER DONE");
  
  // check bluetooth
  //handle_bluetooth_cgm(connection_service_peek_pebble_app_connection());
  handle_bluetooth_cgm(bluetooth_connection_service_peek());
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW LOAD OUT");
} // end window_load_cgm

void window_unload_cgm(Window *window_cgm) {
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW UNLOAD IN");
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW UNLOAD, APP SYNC DEINIT");
  app_sync_deinit(&sync_cgm);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW UNLOAD, DESTROY GBITMAPS IF EXIST");
  destroy_null_GBitmap(&bg_image_bitmap);
  destroy_null_GBitmap(&icon_bitmap);
  destroy_null_GBitmap(&cgmicon_bitmap);
  destroy_null_GBitmap(&specialvalue_bitmap);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW UNLOAD, DESTROY BITMAPS IF EXIST");  
  destroy_null_BitmapLayer(&bg_image_layer);
  destroy_null_BitmapLayer(&icon_layer);
  destroy_null_BitmapLayer(&cgmicon_layer);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW UNLOAD, DESTROY TEXT LAYERS IF EXIST");  
  destroy_null_TextLayer(&bg_layer);
  destroy_null_TextLayer(&cgmtime_layer);
  destroy_null_TextLayer(&message_layer);
  destroy_null_TextLayer(&rig_battlevel_layer);
  destroy_null_TextLayer(&watch_battlevel_layer);  
  destroy_null_TextLayer(&t1dname_layer);
  destroy_null_TextLayer(&time_watch_digi_layer);
  destroy_null_TextLayer(&time_watch_classic_layer);
  destroy_null_TextLayer(&date_app_layer);
  destroy_null_TextLayer(&happymsg_layer);
  destroy_null_TextLayer(&raw_calc_layer);
  destroy_null_TextLayer(&raw_unfilt_layer);
  destroy_null_TextLayer(&noise_layer);
  destroy_null_TextLayer(&calcraw_last1_digi_layer);
  destroy_null_TextLayer(&calcraw_last2_digi_layer);
  destroy_null_TextLayer(&calcraw_last3_digi_layer);
  destroy_null_TextLayer(&calcraw_last1_classic_layer);
  destroy_null_TextLayer(&calcraw_last2_classic_layer);
  destroy_null_TextLayer(&calcraw_last3_classic_layer);

  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW UNLOAD, DESTROY INVERTER LAYERS IF EXIST");  
  //destroy_null_InverterLayer(&inv_rig_battlevel_layer);
  
  destroy_null_Layer(&rectangle_layers);
  
  // destroy animation
  
  //destroy_happymsg_animation(&happymsg_animation);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "WINDOW UNLOAD OUT");
} // end window_unload_cgm

static void init_cgm(void) {
  //APP_LOG(APP_LOG_LEVEL_INFO, "INIT CODE IN");
  
  // subscribe to the tick timer service
  tick_timer_service_subscribe(MINUTE_UNIT, &handle_minute_tick_cgm);

  // subscribe to the bluetooth connection service
  //bluetooth_connection_service_subscribe(&handle_bluetooth_cgm);
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = handle_bluetooth_cgm
  });
  
  // subscribe to the watch battery state service
  battery_state_service_subscribe(&handle_watch_battery_cgm);
  
  // init the window pointer to NULL if it needs it
  if (window_cgm != NULL) {
    window_cgm = NULL;
  }

  // create the windows
  window_cgm = window_create();
  window_set_background_color(window_cgm, GColorWhite);
//  window_set_fullscreen(window_cgm, true);
  window_set_window_handlers(window_cgm, (WindowHandlers) {
    .load = window_load_cgm,
    .unload = window_unload_cgm  
  });
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "INIT CODE, REGISTER APP MESSAGE ERROR HANDLERS"); 
  //app_message_register_inbox_dropped(inbox_dropped_handler_cgm);
  //app_message_register_outbox_failed(outbox_failed_handler_cgm);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "INIT CODE, ABOUT TO CALL APP MSG OPEN"); 
  //app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  
  // Dictionary calculation 1 + 11x7 (11 tuplets, header) + 89 bytes (data) = 167 bytes; pad with 33 bytes
  app_message_open(200, 200);
  //APP_LOG(APP_LOG_LEVEL_INFO, "INIT CODE, APP MSG OPEN DONE");
  
  const bool animated_cgm = true;
  if (window_cgm != NULL) {
    window_stack_push(window_cgm, animated_cgm);
  }
  //else {
    //APP_LOG(APP_LOG_LEVEL_INFO, "INIT CODE, UNABLE TO PUSH WINDOW");
    //text_layer_set_text_color(message_layer, text_default_color);
    //text_layer_set_text(message_layer, "WINDOW ERROR");
  //}
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "INIT CODE OUT"); 
}  // end init_cgm

static void deinit_cgm(void) {
  //APP_LOG(APP_LOG_LEVEL_INFO, "DEINIT CODE IN");
  
  // unsubscribe to the tick timer service
  //APP_LOG(APP_LOG_LEVEL_INFO, "DEINIT, UNSUBSCRIBE TICK TIMER");
  tick_timer_service_unsubscribe();

  // unsubscribe to the bluetooth connection service
  //APP_LOG(APP_LOG_LEVEL_INFO, "DEINIT, UNSUBSCRIBE BLUETOOTH");
  //bluetooth_connection_service_unsubscribe();
  connection_service_unsubscribe();
  
  
  // unsubscribe to the watch battery state service
  battery_state_service_unsubscribe();
  
  // cancel timers if they exist
  //APP_LOG(APP_LOG_LEVEL_INFO, "DEINIT, CANCEL APP TIMER");
  null_and_cancel_timer(&timer_cgm, true);
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "DEINIT, CANCEL BLUETOOTH TIMER");
  null_and_cancel_timer(&BT_timer, true);
  
  // destroy the window if it exists
  //APP_LOG(APP_LOG_LEVEL_INFO, "DEINIT, CHECK WINDOW POINTER FOR DESTROY");
  if (window_cgm != NULL) {
    //APP_LOG(APP_LOG_LEVEL_INFO, "DEINIT, WINDOW POINTER NOT NULL, DESTROY");
    window_destroy(window_cgm);
  }
  //APP_LOG(APP_LOG_LEVEL_INFO, "DEINIT, CHECK WINDOW POINTER FOR NULL");
  if (window_cgm != NULL) {
    //APP_LOG(APP_LOG_LEVEL_INFO, "DEINIT, WINDOW POINTER NOT NULL, SET TO NULL");
    window_cgm = NULL;
  }
  
  //APP_LOG(APP_LOG_LEVEL_INFO, "DEINIT CODE OUT");
} // end deinit_cgm

int main(void) {

  init_cgm();

  app_event_loop();
  deinit_cgm();
  
} // end main
