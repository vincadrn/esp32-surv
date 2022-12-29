#include "WiFi.h"
#include <WiFiMulti.h>
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include "camera_utils.h"
#include "firebase_utils.h"
//Provide the token generation process info (for Firebase)
#include <addons/TokenHelper.h>
#include "time.h"
#include <Preferences.h>

#define MAX_TIMEOUT 45000
#define RED_LED 12

// Ultrasonic
#define TRIG_PIN 13
#define ECHO_PIN 15
#define SOUND_SPEED 0.034
#define MIN_DISTANCE 85.0

// Prefs for network credentials
Preferences preferences;

// WiFiMulti obj
WiFiMulti wifiMulti;

//Firebase obj
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configF;

// time
struct tm timeInfo;
const char* ntpServer = "pool.ntp.org";
const long gmtOffsetSeconds = 0; // time in GMT (Zulu)
const int daylightOffsetSeconds = 0;

// flags
bool takeNewPhoto = false;
bool photoTaskCompleted = false;
bool uploadTaskCompleted = false;

// Task handler for multitasking
TaskHandle_t captureAndSavePicture = NULL;
TaskHandle_t uploadToFirebase = NULL;

void initWiFi() {
  // Import credentials
  preferences.begin("netcreds", false);
  String ssid1 = preferences.getString("ssid-mob", "").c_str();
  String password1 = preferences.getString("password-mob", "").c_str();
  String ssid2 = preferences.getString("ssid-ap1", "").c_str();
  String password2 = preferences.getString("password-ap1", "").c_str();
  String ssid3 = preferences.getString("ssid-ap2", "").c_str();
  String password3 = preferences.getString("password-ap2", "").c_str();
  preferences.end();

  // Establish connection
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(ssid1.c_str(), password1.c_str());
  wifiMulti.addAP(ssid2.c_str(), password2.c_str());
  wifiMulti.addAP(ssid3.c_str(), password3.c_str());
  Serial.print("Connecting to AP");
  for(;;) {
    if (wifiMulti.run() == WL_CONNECTED) break;
  }
  Serial.println();
  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}

/* vFunctions */

void vUploadToFirebase(void *param) {
  Serial.println("Uploading task initiated.");
  // Establish connection to AP
  initWiFi();

  // Time
  configTime(gmtOffsetSeconds, daylightOffsetSeconds, ntpServer);

  //Firebase
  // Assign the api key
  configF.api_key = API_KEY;
  //Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  configF.service_account.data.client_email = FIREBASE_CLIENT_EMAIL;
  configF.service_account.data.project_id = FIREBASE_PROJECT_ID;
  configF.service_account.data.private_key = PRIVATE_KEY;
  //Assign the callback function for the long running token generation task
  configF.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  Firebase.begin(&configF, &auth);
  Firebase.reconnectWiFi(true);

  for(;;) {
    // Put ESP to sleep if nothing happens more than MAX_TIMEOUT after waking up
    if (millis() > MAX_TIMEOUT && !photoTaskCompleted) {
      photoTaskCompleted = true;
      uploadTaskCompleted = true;
      break;
    }

    if (Firebase.ready() && photoTaskCompleted) {
      if(getLocalTime(&timeInfo)) break;
    }

    vTaskDelay(10/portTICK_PERIOD_MS);
  }

  String timestamp = (String)(timeInfo.tm_year + 1900) + "-" + (String)(timeInfo.tm_mon + 1) + "-" + (String)timeInfo.tm_mday + "T" + (String)timeInfo.tm_hour + ":" + (String)timeInfo.tm_min + ":" + (String)timeInfo.tm_sec + "Z";

  FirebaseJson content;
  String picturePath = "/data/" + timestamp + ".jpg";

  /* Save picture */
  Serial.print("Uploading picture... ");

  //MIME type should be valid to avoid the download problem.
  //The file systems for flash and SD/SDMMC can be changed in FirebaseFS.h.
  if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID /* Firebase Storage bucket id */, FILE_PHOTO /* path to local file */, mem_storage_type_flash /* memory storage type, mem_storage_type_flash and mem_storage_type_sd */, picturePath.c_str() /* path of remote file stored in the bucket */, "image/jpeg" /* mime type */)) {
    Serial.printf("\nDownload URL: %s\n", fbdo.downloadURL().c_str());
  } else {
    Serial.println(fbdo.errorReason());
  }

  /* Save log and link to the picture */
  content.set("fields/timestamp/timestampValue", timestamp.c_str()); // save timestamp
  content.set("fields/path/stringValue", picturePath.c_str());

  String logPath = "logs/" + timestamp;

  Serial.print("Creating document...");

  if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", logPath.c_str(), content.raw())) {
    Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
  } else {
    Serial.println(fbdo.errorReason());
  }

  /* Notify user */
  String documentPath = "users/mobile";
  String mask = "tokens";

  Serial.print("Get a document... ");

  if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), mask.c_str()))
    Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
  else
    Serial.println(fbdo.errorReason());

  FirebaseJsonData result;
  content.clear();
  content.setJsonData(fbdo.payload());
  content.get(result, "fields/tokens/stringValue");

  FCM_HTTPv1_JSON_Message msg;

  msg.token = result.to<String>().c_str();

  msg.notification.title = "Alert";
  msg.notification.body = "Perimeter compromised.";

  FirebaseJson payload;

  if (Firebase.FCM.send(&fbdo, &msg)) // send message to recipient
    Serial.printf("ok\n%s\n\n", Firebase.FCM.payload(&fbdo).c_str());
  else
    Serial.println(fbdo.errorReason());

  uploadTaskCompleted = true;

  vTaskDelete(uploadToFirebase);
}

void vCaptureAndSavePicture(void *param) {  
  // Wakeup
  print_wakeup_reason();

  // Initialize SPIFFS for saving picture locally
  initSPIFFS();

  // Initialize camera
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Turn-off the 'brownout detector'
  initCamera();
  vTaskDelay(1000); // for camera to get the settings and config right
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1); // Turn-on the 'brownout detector' back

  // Proximity sensing
  for(;;) {
    if (!takeNewPhoto) {
      digitalWrite(TRIG_PIN, LOW);
      delayMicroseconds(2);
      digitalWrite(TRIG_PIN, HIGH);
      delayMicroseconds(10);
      digitalWrite(TRIG_PIN, LOW);
      float duration = pulseIn(ECHO_PIN, HIGH);
      float distance = duration * SOUND_SPEED / 2;
      Serial.printf("Distance: %.2f\n", distance);
      vTaskDelay(500/portTICK_PERIOD_MS);
      if (distance <= MIN_DISTANCE) {
        takeNewPhoto = true;
        digitalWrite(RED_LED, HIGH); // indicating that proximity is compromised
        break;
      }   
    }
  }

  // Capture
  if (takeNewPhoto) {
    capturePhotoSaveSpiffs();
    photoTaskCompleted = true;
  }

  vTaskDelete(captureAndSavePicture);
}

void setup() {
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 1); // 1 means HIGH
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RED_LED, OUTPUT);
  xTaskCreatePinnedToCore(vUploadToFirebase, "Upload to Firebase", 48000, NULL, 0, &uploadToFirebase, 0);
  xTaskCreatePinnedToCore(vCaptureAndSavePicture, "Capture and save picture", 25000, NULL, 1, &captureAndSavePicture, 1);
}

void loop() {
  if (photoTaskCompleted && uploadTaskCompleted) {
    Serial.println("Sleeping...");
    esp_deep_sleep_start();
  }
  vTaskDelay(10/portTICK_PERIOD_MS);
}
