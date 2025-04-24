/*
Project: NASA APOD Image
Platform: ESP32-S3-DevKitC-1 (ESP32-S3-WROOM-1-N16R8)

Arduino IDE: 2.3.6
ESP32 Board Manager: 2.0.14
*/

// Include required libraries.
#include <Arduino_JSON.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <TJpg_Decoder.h>
#include <TFT_eSPI.h>
#include <WiFi.h>

// Include secrets (e.g. Wi-Fi network settings).
#include "secrets.h"

// Define a line break for output formatting.
const char* lineBreak = "----------------------------------------";

// Create a TFT screen object.
TFT_eSPI tft = TFT_eSPI();


/*
Establishes a Wi-Fi network connection.

Parameters
----------
ssid : const char*
    The SSID of the Wi-Fi network.

password : const char*
    The password for the Wi-Fi network.

timeout : const uint32_t&
    The amount of time to attempt establishing a Wi-Fi network connection 
    before timing out, in seconds.

Returns
-------
bool
    True if a Wi-Fi network connection was established. Otherwise, false.
*/
bool connectWiFi(const char* ssid, const char* password, const uint32_t& timeout) {
    // Output information.
    Serial.println(lineBreak);
    Serial.println("Attempting Wi-Fi Network Connection");
    Serial.print("SSID: ");
    Serial.println(ssid);

    // Convert the timeout time to milliseconds.
    const uint64_t timeoutMillis = timeout * 1000;

    // Start a timer.
    const uint64_t startTime = millis();

    // Attempt to establish a Wi-Fi network connection.
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    // Wait until a connection is established or the timeout time is reached.
    while ((WiFi.status() != WL_CONNECTED) && (millis() - startTime < timeoutMillis)) {
        // Wait 1 second.
        Serial.print(".");
        delay(1000);
    }

    // Output format.
    Serial.println();

    // If a Wi-Fi network connection was established, return true.
    if (WiFi.status() == WL_CONNECTED) {
        // Output information.
        Serial.println("Network Connected");
        Serial.print("ESP32 IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.println(lineBreak);
    
        return true;
    }

    // Otherwise, return false.
    else {
        // Disconnect from the Wi-Fi network.
        WiFi.disconnect(false, true);
    
        // Output error information.
        Serial.println("Network Connection Failed");
        Serial.println(lineBreak);

        return false;
    }
}


/*
Processes a HTTP GET request to a URL.

Parameters
----------
http : HTTPClient&
    The HTTP client for the GET request.

url : const char*
    The URL for the GET request.

retries : const uint_8t&
    The number of times to attempt the GET request for a successful response 
    code (200).

waitTime : const uint16_t&
    The amount of time to wait between GET request attempts, in seconds.

Returns
-------
bool 
    True if the response code was 200. Otherwise, false.
*/
bool getRequest(
    HTTPClient& http, 
    const char* url, 
    const uint8_t& retries, 
    const uint16_t& waitTime
) {
    // If no Wi-Fi network connection exists, return false.
    if (WiFi.status() != WL_CONNECTED) {
        // Output error information.
        Serial.println(lineBreak);
        Serial.println("No Wi-Fi Network Connection");
        Serial.println(lineBreak);

        return false;
    }

    // Convert the time between retries to milliseconds.
    const uint64_t waitTimeMillis = waitTime * 1000;

    // Open a HTTP connection to the URL.
    http.begin(url);

    // Attempt the GET request for the given number of retries, until success.
    for (uint8_t i = 0; i < retries; i++) {
        // Output information.
        Serial.println(lineBreak);
        Serial.println("Sending GET Request");

        // Send a GET request and store the response code.
        const uint16_t responseCode = http.GET();

        // If the response code is 200 (success), return true.
        if (responseCode == HTTP_CODE_OK) {
            // Output success information.
            Serial.println("Request Succeeded");
            Serial.print("Response Code: ");
            Serial.println(responseCode);
            Serial.println(lineBreak);

            return true;
        }

        // Otherwise, wait before re-attempting the GET request.
        else {
            // Output error information.
            Serial.println("Request Failed");
            Serial.print("Response Code: ");
            Serial.println(responseCode);
            Serial.println(lineBreak);

            // Wait before re-attempt.
            delay(waitTimeMillis);
        }
    }

    // Close the HTTP connection to the URL.
    http.end();

    return false;
}


/*
Downloads an image from a URL and saves it to a SPIFFS mount.

Parameters
----------
http : HTTPClient&
    The HTTP client for downloading the image.

url : const char*
    The URL for the image.

path : const char*
    The path to save the image to in the SPIFFS mount. 

Returns
-------
bool
    True if the image was saved in the SPIFFS mount. Otherwise, false.
*/
bool downloadImage(HTTPClient& http, const char* url, const char* path) {
    // If there is a file saved at the given path, remove it.
    if (SPIFFS.exists(path)) {
        // Output information.
        Serial.println(lineBreak);
        Serial.println("Removing Existing File");

        // Remove the previous file.
        SPIFFS.remove(path);
    }

    // Send a GET request to the image URL.
    if (!getRequest(http, url, 5, 10)) {
        return false;
    }

    // Open a file on the SPIFFS mount, with the given path.
    File file = SPIFFS.open(path, FILE_WRITE);

    // If the file count not be created, return false.
    if (!file) {
        // Close the HTTP connection to the image URL.
        http.end();

        return false;
    }
    
    // Create a buffer for loading in the image file.
    // Use "ps_malloc" for PSRAM buffer.
    uint8_t* buffer = (uint8_t*) malloc(1024);

    // If the buffer could not be allocated, return false.
    if (!buffer) {
        // Close the HTTP connection to the image URL.
        http.end();

        return false;
    }

    // Get the data stream pointer to the image file.
    WiFiClient* stream = http.getStreamPtr();
    
    // Get the size of the image file (bytes).
    const uint32_t fileSize = http.getSize();
    uint32_t remainingData = fileSize;

    // Output information.
    Serial.print("Image File Size: ");
    Serial.println(fileSize);

    // Download the image file in chunks.
    while (http.connected() && (remainingData > 0 || remainingData == -1)) {
        // Get the amount of available data from the data stream (bytes).
        size_t availableData = stream->available();
        
        // If there is available data, download it.
        if (availableData) {
            // Store the amount of data loaded from the data stream (bytes).
            uint32_t loadedData;
            
            // If there is more available data than the buffer size, limit the downloaded data.
            if (availableData > sizeof(buffer)) {
                loadedData = stream->readBytes(buffer, sizeof(buffer));
            }

            // Otherwise, download the available data.
            else {
                loadedData = stream->readBytes(buffer, availableData);
            }

            // Write the downloaded data to the local file in the SPIFFS mount.
            file.write(buffer, loadedData);

            // Update the amount of remaining data to download (bytes).
            if (remainingData > 0) {
                remainingData -= loadedData;   
            }
        }

        // Take a small break.
        delay(1);
    }
    
    // Close the file in the SPIFFS mount.
    file.close();

    // Close the HTTP connection to the image URL.
    http.end();

    // Free the allocated buffer (PSRAM).
    free(buffer);

    // Output information.
    Serial.print("Downloaded File Size: ");
    Serial.println(fileSize - remainingData);
    Serial.println(lineBreak);
    
    // If the whole image file was not downloaded, return false.
    if (remainingData > 0) {
        return false;
    }

    return true;
}


/*
Callback function for TJpg_Decoder. Draws an image block on the TFT screen.

Parameters
----------
horizontalPosition : int16_t
    The horizontal position of the image block on the TFT screen.

verticalPosition : int16_t
    The vertical position of the image block on the TFT screen.

width : uint16_t
    The width of the image block.

height : uint16_t
    The width of the image block.

bitmap : uint16_t*
    The pixel values of the image block.

Returns
-------
bool
    True to continue drawing the next image block. Otherwise, false.
*/
bool drawCallback(
    int16_t horizontalPosition, 
    int16_t verticalPosition, 
    uint16_t width, 
    uint16_t height, 
    uint16_t* bitmap
) {
    // If the image block starts off the TFT screen, return false.
    if (horizontalPosition >= tft.width() || verticalPosition >= tft.height()) {
        return false;
    }

    // Draw the image block on the TFT screen.
    tft.pushImage(horizontalPosition, verticalPosition, width, height, bitmap);

    return true;
}


/*
Decodes and draws a baseline JPEG image, saved in a SPIFFS mount, on a TFT 
screen.

Parameters
----------
path : const char*
    The path to the image in the SPIFFS mount.

Returns
-------
bool
    True if the image was drawn on the TFT screen. Otherwise, false.
*/
bool drawImage(const char* path) {
    // Setup the TFT screen.
    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);

    // Setup the JPEG decoder (TJpg_Decoder).
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(drawCallback);

    // Store the width and height of the image.
    uint16_t width = 0, height = 0;
    TJpgDec.getFsJpgSize(&width, &height, path);

    // // If the width is larger than the height, set the rotation to landscape.
    // if (width >= height) {
    //     tft.setRotation(3);
    // }

    // // Otherwise, set the rotation to portrait.
    // else {
    //     tft.setRotation(0);
    // }

    // Store the image scaling (default = 8).
    uint8_t scale = 8;

    // Calculate the image scaling (1, 2, 4, 8).
    for (scale = 1; scale <= 8; scale <<= 1) {
        // If the scaling fits the image inside the TFT screen, break.
        if ((tft.width() >= width / scale) && (tft.height() >= height / scale)) { 
            break;
        }
    }

    // Set the image scaling.
    TJpgDec.setJpgScale(scale);

    // Calculate the scaled width and height of the image.
    uint16_t scaledWidth = width / scale;
    uint16_t scaledHeight = height / scale;

    // Calculate the positions that centre the image.
    uint16_t horizontalPosition = (tft.width()  - scaledWidth) / 2;
    uint16_t verticalPosition = (tft.height() - scaledHeight) / 2;

    // Decode and draw the JPEG image on the TFT screen.
    TJpgDec.drawFsJpg(horizontalPosition, verticalPosition, path);

    return true;
}


/*
Makes the ESP32 enter Deep Sleep mode.

Parameters
----------
sleepTime : const uint32_t&
    The amount of time to go into Deep Sleep mode, in seconds.
*/
void enterDeepSleep(const uint32_t& sleepTime) {
    // Output information. 
    Serial.println(lineBreak);
    Serial.println("Entering Deep Sleep");
    
    // Enter Deep Sleep mode for the given sleep time (microseconds).
    esp_sleep_enable_timer_wakeup(sleepTime * 1e6);
    esp_deep_sleep_start();
}


void setup() {
    /* SETUP */ //--------------------------------------------------------------

    // Define the APOD image URL (preprocessed). -- REPLACE IF NEEDED
    const char* apodImageURL = "https://raw.githubusercontent.com/Pavan365/ESP32-APOD/main/image/apod.jpg";

    // Define the APOD image path for storage in SPIFFS mount.
    const char* imagePath = "/apod.jpg";

    // Define the sleep time for Deep Sleep mode as 24 hours (seconds).
    const uint32_t sleepTime = 24 * 60 * 60;

    // Begin serial communication (Baud = 115200).
    Serial.begin(115200);

    // Turn on the Wi-Fi module.
    WiFi.mode(WIFI_STA);

    // Initialise a SPIFFS mount.
    if (!SPIFFS.begin(false)) {
        // Output error information.
        Serial.println(lineBreak);
        Serial.println("SPIFFS Mount Failed");

        // Enter Deep Sleep mode for 24 hours.
        enterDeepSleep(sleepTime);      
    }

    // Create a HTTP client object.
    HTTPClient http;

    /* WIFI CONNECTION */ //----------------------------------------------------

    // Establish a Wi-Fi network connection.
    // "ssid" & "password" loaded from "secrets.h".
    if (!connectWiFi(ssid, password, 120)) {
        // Enter Deep Sleep mode for 24 hours.
        enterDeepSleep(sleepTime);
    }

    /* APOD API (NOT REQUIRED) */ //--------------------------------------------

    // // Output information.
    // Serial.println("Contacting APOD API");

    // // Send a GET request to the APOD API.
    // // "apodAPI" loaded from "secrets.h".
    // if (!getRequest(http, apodAPI, 5, 10)) {
    //     // Output error information.
    //     Serial.println("Error Contacting APOD API");

    //     // Enter Deep Sleep mode for 24 hours.
    //     enterDeepSleep(sleepTime);
    // }

    // // Store the response from the APOD API.
    // String apodResponse = http.getString();

    // // Close the HTTP connection to the APOD API.
    // http.end();

    // // Parse the JSON response from the APOD API.
    // JSONVar apodJSON = JSON.parse(apodResponse);

    // // If the response could not be parsed, enter Deep Sleep mode.
    // if (JSON.typeof(apodJSON) == "undefined") {
    //     // Output error information.
    //     Serial.println("Error Parsing JSON Response");

    //     // Enter Deep Sleep mode for 24 hours.
    //     enterDeepSleep(sleepTime);
    // }
    
    // // Store the APOD image URL.
    // const char* apodImageURL = apodJSON["url"];

    /* DOWNLOAD APOD IMAGE */ //------------------------------------------------

    // Output information.
    Serial.println("1. Downloading APOD Image");
    
    // Download the APOD image.
    if (!downloadImage(http, apodImageURL, imagePath)) {
        // Output error information.
        Serial.println("Error Downloading APOD Image");

        // Enter Deep Sleep mode for 24 hours.
        enterDeepSleep(sleepTime);
    }

    /* DRAW APOD IMAGE */ //----------------------------------------------------

    // Output information.
    Serial.println("2. Drawing APOD Image");

    // Draw the APOD image.
    if (!drawImage(imagePath)) {
        // Output error information.
        Serial.println("Error Drawing APOD Image");

        // Enter Deep Sleep mode for 24 hours.
        enterDeepSleep(sleepTime);
    }

    // Output information.
    Serial.println(lineBreak);
    Serial.println("APOD Image Drawn!");

    /* CLEAN UP */ //-----------------------------------------------------------

    // Close the SPIFFS mount.
    SPIFFS.end();

    // Disconnect from the Wi-Fi network.
    WiFi.disconnect(false, true);

    // Turn off the Wi-Fi module.
    WiFi.mode(WIFI_OFF);
}


void loop() {
    // Do nothing.
}
