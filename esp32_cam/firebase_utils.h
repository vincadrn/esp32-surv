#ifndef FIREBASE_UTILS_H
#define FIREBASE_UTILS_H

#include <Firebase_ESP_Client.h>

// Firebase project API Key
#define API_KEY "xxxxxxxxxxxdq4KG_gLFZYeHp66Txxxxxxxxxxx"

// Authorized Email and Corresponding Password
#define USER_EMAIL "xxx"
#define USER_PASSWORD "xxx"

// Firebase storage bucket ID e.g bucket-name.appspot.com
#define STORAGE_BUCKET_ID "xxx.appspot.com"

// Firebase Project ID
#define FIREBASE_PROJECT_ID "xxx"

// Firebase Client Email
#define FIREBASE_CLIENT_EMAIL "firebase-xxx-xxx@xxx.iam.gserviceaccount.com"

// PRIVATE KEY
const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY-----\nMIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwxxx...xxxxxxxxxxx\n-----END PRIVATE KEY-----\n";

void fcsUploadCallback(CFS_UploadStatusInfo info);

#endif
