/*************************************************** 
  ProtoStaxAG Arduino Giga Display Demo

  This is a example sketch using Arduino Giga Display Shield,
  Arduino Giga R1 WIFI,
  and
  ProtoStax-AG Enclosure for Arduino Giga Display --> https://www.protostax.com/products/protostax-ag-enclosure-for-arduino-giga-display

  A lot of time and effort has gone into providing this and other code. 
  Please support ProtoStax by purchasing products from us!
 
  Written by Sridhar Rajagopal for ProtoStax
  BSD license. All text above must be included in any redistribution
 */


#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include <Arduino_GigaDisplay.h>
#include "Arduino_BMI270_BMM150.h"

#include <lvgl.h>
#include <PDM.h>

BoschSensorClass imu(Wire1);

//Create rgb object
GigaDisplayRGB rgb;

Arduino_H7_Video          Display(800, 480, GigaDisplayShield);
Arduino_GigaDisplayTouch  TouchDetector;

static lv_obj_t * chart_pdm;
static lv_chart_series_t * pdm_series;

static lv_obj_t * chart_imu;
static lv_chart_series_t * imu_series_x;
static lv_chart_series_t * imu_series_y;
static lv_chart_series_t * imu_series_z;

// Buffer to read PDM samples into, each sample is 16-bits
#define PDM_NUMBER_SAMPLES 512
short sampleBuffer[PDM_NUMBER_SAMPLES];

// default number of output channels for PDM
static const char channels = 1;

// default PDM output frequency
static const int frequency = 16000;

// Number of audio samples read
volatile int samplesRead = 0;

/* Number of data points for the IMU Chart */
#define IMU_CHART_RESOLUTION 384


/* Slider red value changed event callback */
static void red_event_cb(lv_event_t * e) {
  lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
  int32_t value = lv_slider_get_value(slider);
  set_rgb_color(0, (uint8_t)value);
}

/* Slider green value changed event callback */
static void green_event_cb(lv_event_t * e) {
  lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
  int32_t value = lv_slider_get_value(slider);
  set_rgb_color(1, (uint8_t)value);
}

/* Slider blue value changed event callback */
static void blue_event_cb(lv_event_t * e) {
  lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
  int32_t value = lv_slider_get_value(slider);
  set_rgb_color(2, (uint8_t)value);
}

/* Convenience method to set the red, green, and blue values of the RGB LED individually while remembering the other values */
void set_rgb_color(int color, uint8_t val) {
  static uint8_t red = 0;
  static uint8_t green = 0;
  static uint8_t blue = 0;

  if (color == 0) {
    red = val;
  } else if (color == 1) {
    green = val;
  } else if (color == 2) {
    blue = val;
  }
  rgb.on(red, green, blue);
}

void setup() {
  Serial.begin(9600);

  // while (!Serial);
  Serial.println("Started");

  rgb.begin();

  Display.begin();
  TouchDetector.begin();

  DisplaySetup();

  // Configure the data receive callback
  PDM.onReceive(onPDMdata);

  if (!PDM.begin(channels, frequency)) {
    Serial.println("Failed to start PDM!");
    while (1);
  }

  if (!imu.begin()) {
    Serial.println("Failed to initialize imu!");
    while (1);
  }
  Serial.print("Gyroscope sample rate = ");
  Serial.print(imu.gyroscopeSampleRate());
  Serial.println(" Hz");
  Serial.println();
  Serial.println("Gyroscope in degrees/second");
  Serial.println("X\tY\tZ");
}

/* Set up LVGL Display */ 
void DisplaySetup() {
  /* Create a container with grid 2x2 */
  static lv_coord_t col_dsc[] = {370, 370, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t row_dsc[] = {215, 215, LV_GRID_TEMPLATE_LAST};
  lv_obj_t * cont = lv_obj_create(lv_scr_act());
  lv_obj_set_grid_dsc_array(cont, col_dsc, row_dsc);
  lv_obj_set_size(cont, Display.width(), Display.height());
  lv_obj_set_style_bg_color(cont, lv_color_hex(0x03989e), LV_PART_MAIN);
  lv_obj_center(cont);

  lv_obj_t * label;
  lv_obj_t * obj;
  uint16_t idx = 0u;


  short *dataPDM = sampleBuffer;

  /* [0;0] - LOGO Image */
  obj = lv_obj_create(cont);
  lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, 0, 1,
                        LV_GRID_ALIGN_STRETCH, 0, 1);

  LV_IMG_DECLARE(ProtoStaxLogo_200x150);
  lv_obj_t * img1 = lv_img_create(obj);
  lv_img_set_src(img1, &ProtoStaxLogo_200x150);
  lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_size(img1, 200, 150);

  /* [1;0] - Chart MIC */
  obj = lv_obj_create(cont);
  lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, 1, 1,
                        LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);

  // Create a chart object
  chart_pdm = lv_chart_create( obj );
  // Create a label for Title text
  lv_obj_t * lbl_title = lv_label_create( chart_pdm );
  lv_label_set_text( lbl_title, "MIC Graph");
  lv_obj_set_style_text_align( lbl_title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align( lbl_title, LV_ALIGN_TOP_MID, 0, 0 );

  lv_obj_set_size( chart_pdm, 320, 180 );
  lv_obj_align( chart_pdm, LV_ALIGN_CENTER, 0, 0 );

  // Set Chart Type to Line Chart
  lv_chart_set_type( chart_pdm, LV_CHART_TYPE_LINE );
  lv_chart_set_point_count( chart_pdm, PDM_NUMBER_SAMPLES );
  // Update mode shift or circular, here shift is selected
  lv_chart_set_update_mode( chart_pdm, LV_CHART_UPDATE_MODE_SHIFT );
  // Specify Vertical Range
  lv_chart_set_range( chart_pdm, LV_CHART_AXIS_PRIMARY_Y, -8000, 8000);

  // Add Data Series
  pdm_series = lv_chart_add_series(chart_pdm, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
  int32_t * pdm_y_points = lv_chart_get_y_array(chart_pdm, pdm_series);

  for( idx=0; idx<PDM_NUMBER_SAMPLES; idx++ )
  {
    pdm_y_points[idx] = (lv_coord_t)*(dataPDM+idx);
  }

  lv_chart_refresh(chart_pdm); /*Required after direct set*/ 


  /* [0;1] - Sliders for RGB LED */
  obj = lv_obj_create(cont);
  lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, 0, 1,
                        LV_GRID_ALIGN_STRETCH, 1, 1);
  lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);      

  label = lv_label_create(obj);                  
  lv_label_set_text(label, "LED COLOR PICKER");
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align( label, LV_ALIGN_TOP_MID, 0, 0 );

  lv_obj_t * slider = lv_slider_create(obj);
  lv_slider_set_range(slider, 0, 255);
  lv_slider_set_value(slider, 75, LV_ANIM_OFF);
  lv_obj_center(slider);
  label = lv_label_create(obj);
  lv_label_set_text(label, "RED");
  lv_obj_align_to(label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
  lv_obj_add_event_cb(slider, red_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

  slider = lv_slider_create(obj);
  lv_slider_set_range(slider, 0, 255);
  lv_slider_set_value(slider, 80, LV_ANIM_OFF);
  lv_obj_center(slider);
  label = lv_label_create(obj);
  lv_label_set_text(label, "GREEN");
  lv_obj_align_to(label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 20); 
  lv_obj_add_event_cb(slider, green_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

  slider = lv_slider_create(obj);
  lv_slider_set_range(slider, 0, 255);
  lv_slider_set_value(slider, 50, LV_ANIM_OFF);
  lv_obj_center(slider);
  label = lv_label_create(obj);
  lv_label_set_text(label, "BLUE");
  lv_obj_align_to(label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);   
  lv_obj_add_event_cb(slider, blue_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

  rgb.on(75, 80, 50);

  /* [1;1] - Chart IMU */
  obj = lv_obj_create(cont);
  lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, 1, 1,
                        LV_GRID_ALIGN_STRETCH, 1, 1);

  // Create a chart object
  chart_imu = lv_chart_create( obj );
  // Create a label for Title text
  lbl_title = lv_label_create( chart_imu );
  lv_label_set_text( lbl_title, "IMU Graph");
  lv_obj_set_style_text_align( lbl_title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align( lbl_title, LV_ALIGN_TOP_MID, 0, 0 );

  lv_obj_set_size( chart_imu, 320, 180 );
  lv_obj_align( chart_imu, LV_ALIGN_CENTER, 0, 0 );

  // Set Chart Type to Line Chart
  lv_chart_set_type( chart_imu, LV_CHART_TYPE_LINE );
  lv_chart_set_point_count( chart_imu, IMU_CHART_RESOLUTION );
  // Update mode shift or circular, here shift is selected
  lv_chart_set_update_mode( chart_imu, LV_CHART_UPDATE_MODE_SHIFT );
  // Specify Vertical Range
  lv_chart_set_range( chart_imu, LV_CHART_AXIS_PRIMARY_Y, -1000, 1000);

  // Add Data Series
  imu_series_x = lv_chart_add_series(chart_imu, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
  imu_series_y = lv_chart_add_series(chart_imu, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
  imu_series_z = lv_chart_add_series(chart_imu, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

  lv_chart_set_next_value(chart_imu, imu_series_x, 0);
  lv_chart_set_next_value(chart_imu, imu_series_y, 0);
  lv_chart_set_next_value(chart_imu, imu_series_z, 0);

  lv_chart_refresh(chart_imu); /*Required after direct set*/ 
}

/**
 * Callback function to process the data from the PDM microphone.
 * NOTE: This callback is executed as part of an ISR.
 * Therefore using `Serial` to print messages inside this function isn't supported.
 * */
void onPDMdata() {
  // Query the number of available bytes
  int bytesAvailable = PDM.available();

  // Read into the sample buffer
  PDM.read(sampleBuffer, bytesAvailable);

  // 16-bit, 2 bytes per sample
  samplesRead = bytesAvailable / 2;
}


void loop() { 
  /* Feed LVGL engine */
  lv_timer_handler();

  // Wait for samples to be read
  if (samplesRead) {
     Display_MICChartRefresh();
  }

  float x, y, z;

  if (imu.gyroscopeAvailable()) {
    imu.readGyroscope(x, y, z);
    Display_IMUChartRefresh(x, y, z);
  }  
}

static void Display_MICChartRefresh( void )
{
  uint16_t idx = 0u;
  short *dataPDM = sampleBuffer;

  int32_t * pdm_y_points = lv_chart_get_y_array(chart_pdm, pdm_series);

  for( idx=0; idx<PDM_NUMBER_SAMPLES; idx++ )
  {
    pdm_y_points[idx] = (lv_coord_t)*(dataPDM+idx);  
  }

  lv_chart_refresh(chart_pdm); /*Required after direct set*/
}

static void Display_IMUChartRefresh( float x, float y, float z )
{
  lv_chart_set_next_value(chart_imu, imu_series_x, x);
  lv_chart_set_next_value(chart_imu, imu_series_y, y);
  lv_chart_set_next_value(chart_imu, imu_series_z, z);

  lv_chart_refresh(chart_imu); /*Required after direct set*/
}
