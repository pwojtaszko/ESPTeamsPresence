/**
 * ESPTeamsPresence -- A standalone Microsoft Teams presence light 
 *   based on ESP32 and RGB neopixel LEDs.
 *   https://github.com/toblum/ESPTeamsPresence
 *
 * Copyright (C) 2020 Tobias Blum <make@tobiasblum.de>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <Arduino.h>
#include <IotWebConf.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

// LED Matrix settings
#ifndef LED_MATRIX_PIN
#define LED_MATRIX_PIN 5
#endif
#ifndef NUM_LEDS
#define NUM_LEDS 25  // 5x5 matrix
#endif
// ESP32-C3 has limited RMT channels, use channel 0 explicitly
Adafruit_NeoPixel ledMatrix(NUM_LEDS, LED_MATRIX_PIN, NEO_GRB + NEO_KHZ800);

#include <EEPROM.h>
#include "FS.h"
#include "SPIFFS.h"
#include "ESP32_RMT_Driver.h"


// Global settings
// #define NUMLEDS 16							// Number of LEDs on the strip (if not set via build flags)
// #define DATAPIN 26							// GPIO pin used to drive the LED strip (20 == GPIO/D13) (if not set via build flags)
// #define DISABLECERTCHECK 1					// Uncomment to disable https certificate checks (if not set via build flags)
// #define STATUS_PIN LED_BUILTIN				// User builtin LED for status (if not set via build flags)
#define DEFAULT_POLLING_PRESENCE_INTERVAL "30"	// Default interval to poll for presence info (seconds)
#define DEFAULT_ERROR_RETRY_INTERVAL 30			// Default interval to try again after errors
#define TOKEN_REFRESH_TIMEOUT 60	 			// Number of seconds until expiration before token gets refreshed
#define CONTEXT_FILE "/context.json"			// Filename of the context file
#define VERSION "0.18.3"						// Version of the software

#define DBG_PRINT(x) Serial.print(x)
#define DBG_PRINTLN(x) Serial.println(x)


#ifndef DISABLECERTCHECK
// Tool to get certs: https://projects.petrucci.ch/esp32/

// certificate for https://graph.microsoft.com and https://login.microsoftonline.com
// DigiCert Global Root CA, valid until Mon Sep 23 2030, size: 1761 bytes 
const char* rootCACertificate = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIE6DCCA9CgAwIBAgIQAnQuqhfKjiHHF7sf/P0MoDANBgkqhkiG9w0BAQsFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
"QTAeFw0yMDA5MjMwMDAwMDBaFw0zMDA5MjIyMzU5NTlaME0xCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxJzAlBgNVBAMTHkRpZ2lDZXJ0IFNIQTIg\n" \
"U2VjdXJlIFNlcnZlciBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n" \
"ANyuWJBNwcQwFZA1W248ghX1LFy949v/cUP6ZCWA1O4Yok3wZtAKc24RmDYXZK83\n" \
"nf36QYSvx6+M/hpzTc8zl5CilodTgyu5pnVILR1WN3vaMTIa16yrBvSqXUu3R0bd\n" \
"KpPDkC55gIDvEwRqFDu1m5K+wgdlTvza/P96rtxcflUxDOg5B6TXvi/TC2rSsd9f\n" \
"/ld0Uzs1gN2ujkSYs58O09rg1/RrKatEp0tYhG2SS4HD2nOLEpdIkARFdRrdNzGX\n" \
"kujNVA075ME/OV4uuPNcfhCOhkEAjUVmR7ChZc6gqikJTvOX6+guqw9ypzAO+sf0\n" \
"/RR3w6RbKFfCs/mC/bdFWJsCAwEAAaOCAa4wggGqMB0GA1UdDgQWBBQPgGEcgjFh\n" \
"1S8o541GOLQs4cbZ4jAfBgNVHSMEGDAWgBQD3lA1VtFMu2bwo+IbG8OXsj3RVTAO\n" \
"BgNVHQ8BAf8EBAMCAYYwHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMBIG\n" \
"A1UdEwEB/wQIMAYBAf8CAQAwdgYIKwYBBQUHAQEEajBoMCQGCCsGAQUFBzABhhho\n" \
"dHRwOi8vb2NzcC5kaWdpY2VydC5jb20wQAYIKwYBBQUHMAKGNGh0dHA6Ly9jYWNl\n" \
"cnRzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RDQS5jcnQwewYDVR0f\n" \
"BHQwcjA3oDWgM4YxaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0R2xv\n" \
"YmFsUm9vdENBLmNybDA3oDWgM4YxaHR0cDovL2NybDQuZGlnaWNlcnQuY29tL0Rp\n" \
"Z2lDZXJ0R2xvYmFsUm9vdENBLmNybDAwBgNVHSAEKTAnMAcGBWeBDAEBMAgGBmeB\n" \
"DAECATAIBgZngQwBAgIwCAYGZ4EMAQIDMA0GCSqGSIb3DQEBCwUAA4IBAQB3MR8I\n" \
"l9cSm2PSEWUIpvZlubj6kgPLoX7hyA2MPrQbkb4CCF6fWXF7Ef3gwOOPWdegUqHQ\n" \
"S1TSSJZI73fpKQbLQxCgLzwWji3+HlU87MOY7hgNI+gH9bMtxKtXc1r2G1O6+x/6\n" \
"vYzTUVEgR17vf5irF0LKhVyfIjc0RXbyQ14AniKDrN+v0ebHExfppGlkTIBn6rak\n" \
"f4994VH6npdn6mkus5CkHBXIrMtPKex6XF2firjUDLuU7tC8y7WlHgjPxEEDDb0G\n" \
"w6D0yDdVSvG/5XlCNatBmO/8EznDu1vr72N8gJzISUZwa6CCUD7QBLbKJcXBBVVf\n" \
"8nwvV9GvlW+sbXlr\n" \
"-----END CERTIFICATE-----\n" \
"";

// Use the same cert for login and graph
const char* rootCACertificateLogin = rootCACertificate;
const char* rootCACertificateGraph = rootCACertificate;
#endif



// IotWebConf
// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "ESPTeamsPresence";
// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "presence";

DNSServer dnsServer;
WebServer server(80);

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword);

// Add parameter
#define STRING_LEN 64
#define INTEGER_LEN 16
char paramClientIdValue[STRING_LEN];
char paramTenantValue[STRING_LEN];
char paramPollIntervalValue[INTEGER_LEN];
char paramNumLedsValue[INTEGER_LEN];
IotWebConfSeparator separator = IotWebConfSeparator();
IotWebConfParameter paramClientId = IotWebConfParameter("Client-ID (Generic ID: 3837bbf0-30fb-47ad-bce8-f460ba9880c3)", "clientId", paramClientIdValue, STRING_LEN, "text", "e.g. 3837bbf0-30fb-47ad-bce8-f460ba9880c3", "3837bbf0-30fb-47ad-bce8-f460ba9880c3");
IotWebConfParameter paramTenant = IotWebConfParameter("Tenant hostname / ID", "tenantId", paramTenantValue, STRING_LEN, "text", "e.g. contoso.onmicrosoft.com");
IotWebConfParameter paramPollInterval = IotWebConfParameter("Presence polling interval (sec) (default: 30)", "pollInterval", paramPollIntervalValue, INTEGER_LEN, "number", "10..300", DEFAULT_POLLING_PRESENCE_INTERVAL, "min='10' max='300' step='5'");
IotWebConfParameter paramNumLeds = IotWebConfParameter("Number of LEDs (default: 16)", "numLeds", paramNumLedsValue, INTEGER_LEN, "number", "1..500", "16", "min='1' max='500' step='1'");
byte lastIotWebConfState;

// HTTP client
WiFiClientSecure client;

// OTA update
HTTPUpdateServer httpUpdater;

// Global variables
String user_code = "";
String device_code = "";
uint8_t interval = 5;

String access_token = "";
String refresh_token = "";
String id_token = "";
unsigned int expires = 0;

String availability = "";
String activity = "";

// Statemachine
#define SMODEINITIAL 0               // Initial
#define SMODEWIFICONNECTING 1        // Wait for wifi connection
#define SMODEWIFICONNECTED 2         // Wifi connected
#define SMODEDEVICELOGINSTARTED 10   // Device login flow was started
#define SMODEDEVICELOGINFAILED 11    // Device login flow failed
#define SMODEAUTHREADY 20            // Authentication successful
#define SMODEPOLLPRESENCE 21         // Poll for presence
#define SMODEREFRESHTOKEN 22         // Access token needs refresh
#define SMODEPRESENCEREQUESTERROR 23 // Access token needs refresh
uint8_t state = SMODEINITIAL;
uint8_t laststate = SMODEINITIAL;
static unsigned long tsPolling = 0;
uint8_t retries = 0;

// Multicore
TaskHandle_t TaskNeopixel; 

// Smooth LED transition variables
struct RGBColor {
	uint8_t r, g, b;
};

RGBColor currentColor = {0, 0, 0};    // Current LED color
RGBColor targetColor = {0, 0, 0};     // Target LED color
bool transitionActive = false;        // Whether a transition is active
unsigned long transitionStartTime = 0; // When the transition started
#define TRANSITION_DURATION 1000      // Transition duration in milliseconds


/**
 * Helper
 */
// Calculate token lifetime
int getTokenLifetime() {
	return (expires - millis()) / 1000;
}

// Save context information to file in SPIFFS
void saveContext() {
	const size_t capacity = JSON_OBJECT_SIZE(3) + 5000;
	DynamicJsonDocument contextDoc(capacity);
	contextDoc["access_token"] = access_token.c_str();
	contextDoc["refresh_token"] = refresh_token.c_str();
	contextDoc["id_token"] = id_token.c_str();

	File contextFile = SPIFFS.open(CONTEXT_FILE, FILE_WRITE);
	size_t bytesWritten = serializeJsonPretty(contextDoc, contextFile);
	contextFile.close();
	DBG_PRINT(F("saveContext() - Success: "));
	DBG_PRINTLN(bytesWritten);
	// DBG_PRINTLN(contextDoc.as<String>());
}

boolean loadContext() {
	File file = SPIFFS.open(CONTEXT_FILE);
	boolean success = false;

	if (!file) {
		DBG_PRINTLN(F("loadContext() - No file found"));
	} else {
		size_t size = file.size();
		if (size == 0) {
			DBG_PRINTLN(F("loadContext() - File empty"));
		} else {
			const int capacity = JSON_OBJECT_SIZE(3) + 10000;
			DynamicJsonDocument contextDoc(capacity);
			DeserializationError err = deserializeJson(contextDoc, file);

			if (err) {
				DBG_PRINT(F("loadContext() - deserializeJson() failed with code: "));
				DBG_PRINTLN(err.c_str());
			} else {
				int numSettings = 0;
				if (!contextDoc["access_token"].isNull()) {
					access_token = contextDoc["access_token"].as<String>();
					numSettings++;
				}
				if (!contextDoc["refresh_token"].isNull()) {
					refresh_token = contextDoc["refresh_token"].as<String>();
					numSettings++;
				}
				if (!contextDoc["id_token"].isNull()){
					id_token = contextDoc["id_token"].as<String>();
					numSettings++;
				}
				if (numSettings == 3) {
					success = true;
					DBG_PRINTLN(F("loadContext() - Success"));
					if (strlen(paramClientIdValue) > 0 && strlen(paramTenantValue) > 0) {
						DBG_PRINTLN(F("loadContext() - Next: Refresh token."));
						state = SMODEREFRESHTOKEN;
					} else {
						DBG_PRINTLN(F("loadContext() - No client id or tenant setting found."));
					}
				} else {
					Serial.printf("loadContext() - ERROR Number of valid settings in file: %d, should be 3.\n", numSettings);
				}
				// DBG_PRINTLN(contextDoc.as<String>());
			}
		}
		file.close();
	}

	return success;
}

// Remove context information file in SPIFFS
void removeContext() {
	SPIFFS.remove(CONTEXT_FILE);
	DBG_PRINTLN(F("removeContext() - Success"));
}

void startMDNS() {
	DBG_PRINTLN("startMDNS()");
	// Set up mDNS responder
    if (!MDNS.begin(thingName)) {
        DBG_PRINTLN("Error setting up MDNS responder! Continuing without mDNS...");
        return; // Continue without mDNS instead of getting stuck
    }
	
	// Try to add HTTP service, but don't fail if it doesn't work
	if (!MDNS.addService("http", "tcp", 80)) {
		DBG_PRINTLN("Failed adding HTTP service to mDNS, continuing...");
	}

    DBG_PRINT("mDNS responder started: ");
    DBG_PRINT(thingName);
    DBG_PRINTLN(".local");
}


#include "request_handler.h"
#include "spiffs_webserver.h"

// Color interpolation function
RGBColor interpolateColor(RGBColor from, RGBColor to, float progress) {
	// Ensure progress is between 0.0 and 1.0
	if (progress < 0.0) progress = 0.0;
	if (progress > 1.0) progress = 1.0;
	
	RGBColor result;
	result.r = from.r + (to.r - from.r) * progress;
	result.g = from.g + (to.g - from.g) * progress;
	result.b = from.b + (to.b - from.b) * progress;
	
	return result;
}

// Convert RGB values to NeoPixel color
uint32_t rgbToColor(RGBColor rgb) {
	return ledMatrix.Color(rgb.r, rgb.g, rgb.b);
}

// Extract RGB components from a 32-bit color
RGBColor colorToRGB(uint32_t color) {
	RGBColor rgb;
	rgb.r = (color >> 16) & 0xFF;
	rgb.g = (color >> 8) & 0xFF;
	rgb.b = color & 0xFF;
	return rgb;
}

// Update LED transition
void updateLedTransition() {
	if (!transitionActive) return;
	
	unsigned long elapsed = millis() - transitionStartTime;
	float progress = (float)elapsed / TRANSITION_DURATION;
	
	if (progress >= 1.0) {
		// Transition complete
		currentColor = targetColor;
		transitionActive = false;
		progress = 1.0;
	}
	
	RGBColor interpolatedColor = interpolateColor(currentColor, targetColor, progress);
	uint32_t color = rgbToColor(interpolatedColor);
	
	// Update all LEDs immediately during transition
	for(int i = 0; i < NUM_LEDS; i++) {
		ledMatrix.setPixelColor(i, color);
	}
	ledMatrix.show();
	
	// Update current color if transition is complete
	if (!transitionActive) {
		currentColor = targetColor;
	}
}

// Start a smooth transition to a new color
void startColorTransition(uint32_t newColor) {
	RGBColor newRGB = colorToRGB(newColor);
	
	// If already at target color, no transition needed
	if (currentColor.r == newRGB.r && currentColor.g == newRGB.g && currentColor.b == newRGB.b) {
		return;
	}
	
	// Start transition
	targetColor = newRGB;
	transitionActive = true;
	transitionStartTime = millis();
}



// LED Matrix Control Functions
void setLedMatrixColor(uint32_t color) {
	// Add delay between LED operations to prevent RMT conflicts on ESP32-C3
	static unsigned long lastLedUpdate = 0;
	if (millis() - lastLedUpdate < 100) { // Minimum 100ms between updates
		return;
	}
	
	for(int i = 0; i < NUM_LEDS; i++) {
		ledMatrix.setPixelColor(i, color);
	}
	ledMatrix.show();
	lastLedUpdate = millis();
}

void setLedMatrixOff() {
	static unsigned long lastLedUpdate = 0;
	if (millis() - lastLedUpdate < 100) { // Minimum 100ms between updates
		return;
	}
	
	ledMatrix.clear();
	ledMatrix.show();
	lastLedUpdate = millis();
}

void updateLedMatrixFromStatus() {
	// Determine LED color based on Teams availability and activity
	uint32_t newColor;
	
	if (availability == "Available" || availability == "AvailableIdle") {
		newColor = ledMatrix.Color(0, 255, 0);  // Green when people can interrupt
	} else if (availability == "Busy" || availability == "InACall" || availability == "InAMeeting" || 
			   activity == "InACall" || activity == "InAMeeting") {
		newColor = ledMatrix.Color(255, 0, 0);  // Red when in meeting/busy
	} else if (availability == "Away" || availability == "BeRightBack") {
		newColor = ledMatrix.Color(255, 255, 0); // Yellow for away status
	} else if (availability == "DoNotDisturb") {
		newColor = ledMatrix.Color(255, 0, 0);  // Red for do not disturb
	} else if (availability == "Offline" || availability == "PresenceUnknown") {
		newColor = ledMatrix.Color(0, 0, 0);    // Off when offline or unknown
	} else {
		// Default case - dim white for unknown status
		newColor = ledMatrix.Color(64, 64, 64);
	}
	
	// Start smooth transition to new color
	startColorTransition(newColor);
}

// Update LED status based on Teams presence
void updatePresenceStatus() {
	// Update LED matrix - but with rate limiting
	static unsigned long lastStatusUpdate = 0;
	if (millis() - lastStatusUpdate > 1000) { // Update LEDs max once per second
		updateLedMatrixFromStatus();
		lastStatusUpdate = millis();
	}
}


/**
 * Application logic
 */

// Handler: Wifi connected
void onWifiConnected() {
	state = SMODEWIFICONNECTED;
}

// Poll for access token
void pollForToken() {
	String payload = "client_id=" + String(paramClientIdValue) + "&grant_type=urn:ietf:params:oauth:grant-type:device_code&device_code=" + device_code;
	Serial.printf("pollForToken()\n");

	// const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(7) + 530; // Case 1: HTTP 400 error (not yet ready)
	const size_t capacity = JSON_OBJECT_SIZE(7) + 10000; // Case 2: Successful (bigger size of both variants, so take that one as capacity)
	DynamicJsonDocument responseDoc(capacity);
	boolean res = requestJsonApi(responseDoc, "https://login.microsoftonline.com/" + String(paramTenantValue) + "/oauth2/v2.0/token", payload, capacity);

	if (!res) {
		state = SMODEDEVICELOGINFAILED;
	} else if (responseDoc.containsKey("error")) {
		const char* _error = responseDoc["error"];
		const char* _error_description = responseDoc["error_description"];

		if (strcmp(_error, "authorization_pending") == 0) {
			Serial.printf("pollForToken() - Wating for authorization by user: %s\n\n", _error_description);
		} else {
			Serial.printf("pollForToken() - Unexpected error: %s, %s\n\n", _error, _error_description);
			state = SMODEDEVICELOGINFAILED;
		}
	} else {
		if (responseDoc.containsKey("access_token") && responseDoc.containsKey("refresh_token") && responseDoc.containsKey("id_token")) {
			// Save tokens and expiration
			access_token = responseDoc["access_token"].as<String>();
			refresh_token = responseDoc["refresh_token"].as<String>();
			id_token = responseDoc["id_token"].as<String>();
			unsigned int _expires_in = responseDoc["expires_in"].as<unsigned int>();
			expires = millis() + (_expires_in * 1000); // Calculate timestamp when token expires

			// Set state
			state = SMODEAUTHREADY;
		} else {
			Serial.printf("pollForToken() - Unknown response: %s\n", responseDoc.as<const char*>());
		}
	}
}

// Get presence information
void pollPresence() {
	// See: https://github.com/microsoftgraph/microsoft-graph-docs/blob/ananya/api-reference/beta/resources/presence.md
	const size_t capacity = 1024;
	DynamicJsonDocument responseDoc(capacity);
	boolean res = requestJsonApi(responseDoc, "https://graph.microsoft.com/v1.0/me/presence", "", capacity, "GET", true);

	if (!res) {
		state = SMODEPRESENCEREQUESTERROR;
		retries++;
	} else if (responseDoc.containsKey("error")) {
		const char* _error_code = responseDoc["error"]["code"];
		if (strcmp(_error_code, "InvalidAuthenticationToken")) {
			DBG_PRINTLN(F("pollPresence() - Refresh needed"));
			tsPolling = millis();
			state = SMODEREFRESHTOKEN;
		} else {
			Serial.printf("pollPresence() - Error: %s\n", _error_code);
			state = SMODEPRESENCEREQUESTERROR;
			retries++;
		}
	} else {
		// Store presence info
		availability = responseDoc["availability"].as<String>();
		activity = responseDoc["activity"].as<String>();
		retries = 0;

		updatePresenceStatus();
	}
}

// Refresh the access token
boolean refreshToken() {
	boolean success = false;
	// See: https://docs.microsoft.com/de-de/azure/active-directory/develop/v1-protocols-oauth-code#refreshing-the-access-tokens
	String payload = "client_id=" + String(paramClientIdValue) + "&grant_type=refresh_token&refresh_token=" + refresh_token;
	DBG_PRINTLN(F("refreshToken()"));

	const size_t capacity = JSON_OBJECT_SIZE(7) + 10000;
	DynamicJsonDocument responseDoc(capacity);
	boolean res = requestJsonApi(responseDoc, "https://login.microsoftonline.com/" + String(paramTenantValue) + "/oauth2/v2.0/token", payload, capacity);

	// Replace tokens and expiration
	if (res && responseDoc.containsKey("access_token") && responseDoc.containsKey("refresh_token")) {
		if (!responseDoc["access_token"].isNull()) {
			access_token = responseDoc["access_token"].as<String>();
			success = true;
		}
		if (!responseDoc["refresh_token"].isNull()) {
			refresh_token = responseDoc["refresh_token"].as<String>();
			success = true;
		}
		if (!responseDoc["id_token"].isNull()) {
			id_token = responseDoc["id_token"].as<String>();
		}
		if (!responseDoc["expires_in"].isNull()) {
			int _expires_in = responseDoc["expires_in"].as<unsigned int>();
			expires = millis() + (_expires_in * 1000); // Calculate timestamp when token expires
		}

		DBG_PRINTLN(F("refreshToken() - Success"));
		state = SMODEPOLLPRESENCE;
	} else {
		DBG_PRINTLN(F("refreshToken() - Error:"));
		Serial.println(responseDoc.as<String>());
		// Set retry after timeout
		tsPolling = millis() + (DEFAULT_ERROR_RETRY_INTERVAL * 1000);
	}
	return success;
}

// Implementation of a statemachine to handle the different application states
void statemachine() {
	// Statemachine: Check states of iotWebConf to detect AP mode and WiFi Connection attepmt
	byte iotWebConfState = iotWebConf.getState();
	if (iotWebConfState != lastIotWebConfState) {
		if (iotWebConfState == IOTWEBCONF_STATE_NOT_CONFIGURED || iotWebConfState == IOTWEBCONF_STATE_AP_MODE) {
			DBG_PRINTLN(F("Detected AP mode"));
		}
		if (iotWebConfState == IOTWEBCONF_STATE_CONNECTING) {
			DBG_PRINTLN(F("WiFi connecting"));
			state = SMODEWIFICONNECTING;
		}
	}
	lastIotWebConfState = iotWebConfState;

	// Statemachine: Wifi connection start
	if (state == SMODEWIFICONNECTING && laststate != SMODEWIFICONNECTING) {
		// Could update OLED here
	}

	// Statemachine: After wifi is connected
	if (state == SMODEWIFICONNECTED && laststate != SMODEWIFICONNECTED) {
		startMDNS();
		loadContext();
		DBG_PRINTLN(F("Wifi connected, waiting for requests ..."));
	}

	// Statemachine: Devicelogin started
	if (state == SMODEDEVICELOGINSTARTED) {
		if (laststate != SMODEDEVICELOGINSTARTED) {
			// Could update OLED here
		}
		if (millis() >= tsPolling) {
			pollForToken();
			tsPolling = millis() + (interval * 1000);
		}
	}

	// Statemachine: Devicelogin failed
	if (state == SMODEDEVICELOGINFAILED) {
		DBG_PRINTLN(F("Device login failed"));
		state = SMODEWIFICONNECTED; // Return back to initial mode
	}

	// Statemachine: Auth is ready, start polling for presence immediately
	if (state == SMODEAUTHREADY) {
		saveContext();
		state = SMODEPOLLPRESENCE;
		tsPolling = millis();
	}

	// Statemachine: Poll for presence information, even if there was a error before (handled below)
	if (state == SMODEPOLLPRESENCE) {
		if (millis() >= tsPolling) {
			DBG_PRINTLN(F("Polling presence info ..."));
			pollPresence();
			tsPolling = millis() + (atoi(paramPollIntervalValue) * 1000);
			Serial.printf("--> Availability: %s, Activity: %s\n\n", availability.c_str(), activity.c_str());
		}

		if (getTokenLifetime() < TOKEN_REFRESH_TIMEOUT) {
			Serial.printf("Token needs refresh, valid for %d s.\n", getTokenLifetime());
			state = SMODEREFRESHTOKEN;
		}
	}

	// Statemachine: Refresh token
	if (state == SMODEREFRESHTOKEN) {
		if (millis() >= tsPolling) {
			boolean success = refreshToken();
			if (success) {
				saveContext();
			}
		}
	}

	// Statemachine: Polling presence failed
	if (state == SMODEPRESENCEREQUESTERROR) {
		if (laststate != SMODEPRESENCEREQUESTERROR) {
			retries = 0;
		}
		Serial.printf("Polling presence failed, retry #%d.\n", retries);
		if (retries >= 5) {
			state = SMODEREFRESHTOKEN;
		} else {
			state = SMODEPOLLPRESENCE;
		}
	}

	// Update laststate
	if (laststate != state) {
		laststate = state;
		DBG_PRINTLN(F("======================================================================"));
	}
}





/**
 * Main functions
 */
void setup()
{
	// LED Matrix init - with error handling for ESP32-C3
	DBG_PRINTLN(F("Initializing LED matrix..."));
	ledMatrix.begin();
	ledMatrix.setBrightness(10);  // Set low brightness (0-255) - much dimmer
	
	// Test LED matrix with a brief flash to verify it's working
	for(int i = 0; i < NUM_LEDS; i++) {
		ledMatrix.setPixelColor(i, ledMatrix.Color(0, 0, 32)); // Dim blue
	}
	ledMatrix.show();
	delay(500);
	
	setLedMatrixOff();  // Start with LEDs off
	DBG_PRINTLN(F("LED matrix initialized"));
	
	Serial.begin(115200);
	DBG_PRINTLN();
	DBG_PRINTLN(F("setup() Starting up..."));
	#ifdef DISABLECERTCHECK
		DBG_PRINTLN(F("WARNING: Checking of HTTPS certificates disabled."));
	#endif

	// iotWebConf - Initializing the configuration.
	#ifdef LED_BUILTIN
	iotWebConf.setStatusPin(LED_BUILTIN);
	#endif
	iotWebConf.setWifiConnectionTimeoutMs(5000);
	iotWebConf.addParameter(&separator);
	iotWebConf.addParameter(&paramClientId);
	iotWebConf.addParameter(&paramTenant);
	iotWebConf.addParameter(&paramPollInterval);
	iotWebConf.addParameter(&paramNumLeds);
	iotWebConf.setWifiConnectionCallback(&onWifiConnected);
	iotWebConf.setConfigSavedCallback(&onConfigSaved);
	iotWebConf.setupUpdateServer(&httpUpdater);
	iotWebConf.skipApStartup();
	iotWebConf.init();

	// HTTP server - Set up required URL handlers on the web server.
	server.on("/", HTTP_GET, handleRoot);
	server.on("/config", HTTP_GET, [] { iotWebConf.handleConfig(); });
	server.on("/config", HTTP_POST, [] { iotWebConf.handleConfig(); });
	server.on("/upload", HTTP_GET, [] { handleMinimalUpload(); });
	server.on("/api/startDevicelogin", HTTP_GET, [] { handleStartDevicelogin(); });
	server.on("/api/settings", HTTP_GET, [] { handleGetSettings(); });
	server.on("/api/clearSettings", HTTP_GET, [] { handleClearSettings(); });
	server.on("/fs/delete", HTTP_DELETE, handleFileDelete);
	server.on("/fs/list", HTTP_GET, handleFileList);
	server.on("/fs/upload", HTTP_POST, []() {
		server.send(200, "text/plain", "");
	}, handleFileUpload);

	server.onNotFound([]() {
		iotWebConf.handleNotFound();
		if (!handleFileRead(server.uri())) {
			server.send(404, "text/plain", "FileNotFound");
		}
	});

	DBG_PRINTLN(F("setup() ready..."));

	// SPIFFS.begin() - Format if mount failed
	DBG_PRINTLN(F("SPIFFS.begin() "));
	if(!SPIFFS.begin(true)) {
		DBG_PRINTLN("SPIFFS Mount Failed");
		return;
	}
}

void loop()
{
	// iotWebConf - doLoop should be called as frequently as possible.
	iotWebConf.doLoop();

	statemachine();
	
	// Update LED transitions continuously for smooth color changes
	updateLedTransition();
	
	// Update LED status periodically
	static unsigned long lastLedUpdate = 0;
	if (millis() - lastLedUpdate > 1000) { // Update every second
		updatePresenceStatus();
		lastLedUpdate = millis();
	}
}
