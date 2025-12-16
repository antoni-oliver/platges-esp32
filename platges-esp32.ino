#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "board_config.h"
#include "credentials.h"

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  Serial.println("hola");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  //config.frame_size = FRAMESIZE_FHD;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

    // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  //setupLedFlash();
#endif

}

void loop() {
  // put your main code here, to run repeatedly:
  
  // 1. Capture the image
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed!");
    return;
  }
  
  // WIFI CONNECTION
  WiFi.begin(ssid, password);
  //WiFi.setSleep(false);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // 2. Initialize HTTPClient
  HTTPClient http;
  http.begin(serverName);
  
  // 3. Set the POST headers
  String contentType = "multipart/form-data; boundary=";
  contentType += boundary;
  http.addHeader("Content-Type", contentType);

  // --- Start of Body (Header for the image file) ---
  String header = "";
  header += "--";
  header += boundary;
  header += "\r\n";
  header += "Content-Disposition: form-data; name=\"";
  header += formFieldName;
  header += "\"; filename=\"esp32-photo.jpg\"\r\n";
  header += "Content-Type: image/jpeg\r\n\r\n"; // End of header block
  
  // --- End of Body (Footer) ---
  String footer = "\r\n";
  footer += "--";
  footer += boundary;
  footer += "--\r\n"; // Closing boundary

  // Calculate the total content length
  size_t totalLen = header.length() + fb->len + footer.length();

  // 5. Send the request
  Serial.printf("Uploading %u bytes...\n", fb->len);
  
  // Set the total length *before* sending the first part
  http.addHeader("Content-Length", String(totalLen));

  // Start the actual POST request and send the header part
  int httpResponseCode = http.sendRequest("POST", (uint8_t*)header.c_str(), header.length());

  // Send the raw JPEG data (the largest part)
  if (httpResponseCode > 0) {
      httpResponseCode = http.sendRequest("POST", fb->buf, fb->len);
  }

  // Send the footer part to close the form data
  if (httpResponseCode > 0) {
      httpResponseCode = http.sendRequest("POST", (uint8_t*)footer.c_str(), footer.length());
  }

  // 6. Handle the response
  if (httpResponseCode > 0) {
    Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);
  } else {
    Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  // Clean up
  http.end();
  esp_camera_fb_return(fb);
  WiFi.disconnect();
  delay(60000);
}
