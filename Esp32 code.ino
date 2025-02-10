#include <WiFi.h>
#include <FirebaseESP32.h>

// WiFi credentials
const char* WIFI_SSID = "DESKTOP-CBA64UC 0661";  // Replace with your WiFi SSID
const char* WIFI_PASSWORD = "sarvesh17";         // Replace with your WiFi password

// Firebase configuration
#define API_KEY "AIzaSyB3Wg6wn6dsAnTuHmdLvEsNhrPx2c8dfGw"  // Replace with your Firebase API key
#define DATABASE_URL "https://smart-street-light-c846b-default-rtdb.firebaseio.com"  // Replace with your Firebase database URL

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Street light serial code
const char* serialCode = "SL001";  // Change this for different street lights

// Pin definitions
const int ldr = 36;        // LDR pin
const int powerpin = 34;   // Power sensor pin

// Thresholds
const int samint = 25;         // Sampling interval (25 ms)
const int glowchange = 600;    // Glow change threshold
const int flickrate = 10;      // Flicker rate threshold
const int workrate = 10;       // Work rate threshold
const int longFlickRate = 50;  // Long flicker rate threshold
const int brightthresh = 700;  // Brightness threshold
const int offthresh = 1200;    // Off threshold
const int powerthresh = 1000;  // Power threshold

// Variables for detection
long currentmilli = 0, prevmilli = 0;
int currentglow = 0, prevglow = 0;
int flick = 0, work = 0;
int longFlickCount = 0;
int poweron = 0;

// Moving average
const float alpha = 0.1;
float avgGlow = 0;

// Min/Max tracking
int maxGlow = 0, minGlow = 4095;

// Firebase update interval
const int firebaseUpdateInterval = 100;  // Update Firebase every 1 second
long lastFirebaseUpdate = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("⚡ ESP32 Starting...");

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("🔗 Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ WiFi Connected!");

    // Firebase configuration
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    // Firebase authentication
    auth.user.email = "sarveshraj1712@gmail.com";  // Replace with your Firebase email
    auth.user.password = "MEENARASI";             // Replace with your Firebase password

    // Increase timeout for better stability
    config.timeout.serverResponse = 10 * 1000;

    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());

    // Initialize Firebase
    Serial.println("🔥 Connecting to Firebase...");
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    // Check Firebase connection
    if (Firebase.ready()) {
        Serial.println("✅ Firebase Connected!");
    } else {
        Serial.println("❌ Firebase NOT Connected!");
    }
}
String status = "Stable";
String faultType = "None";

void loop() {
    currentmilli = millis();
    currentglow = analogRead(ldr);
    poweron = analogRead(powerpin);

    if (currentmilli - prevmilli >= samint) {
        prevmilli = currentmilli;

        // Calculate moving average
        avgGlow = (alpha * currentglow) + ((1 - alpha) * avgGlow);

        // Update min/max glow
        if (currentglow > maxGlow) maxGlow = currentglow;
        if (currentglow < minGlow) minGlow = currentglow;


        // Check for low-frequency flicker
        if ((maxGlow - minGlow) >= glowchange / 1.5) {
            longFlickCount++;
            if (longFlickCount >= longFlickRate) {
                status = "Faulty";
                faultType = "Low-Frequency Flicker";
                Serial.println("🌊 Low-Frequency Flickering / Dimming Detected!");
                longFlickCount = 0;
                maxGlow = currentglow;
                minGlow = currentglow;
            }
        } else {
            longFlickCount = 0;
        }

        // Check for high-frequency flicker
        if (abs(currentglow - prevglow) >= glowchange/1.5) {
            flick++;
            if (flick >= flickrate) {
                status = "Faulty";
                faultType = "High-Frequency Flicker";
                Serial.println("⚡ High-Frequency Flickering Detected!");
                flick = 0;
            }
        } else {
            work++;
        }
        // Check for brightness and power issues
        Serial.println(work,workrate);
        if (work >= workrate) {
          Serial.println("inside work");
            if (avgGlow >= brightthresh) {
                if (avgGlow >= offthresh && poweron >= powerthresh) {
                    status = "Faulty";
                    faultType = "Fused";
                    Serial.println("Fused");
                } else if (avgGlow >= offthresh && poweron < powerthresh) {
                    status = "Power Off";
                    faultType = "Power Off";  // Set faultType to "Power Off"
                    Serial.println("power off");
                } else if (avgGlow > brightthresh) {
                    status = "Faulty";
                    faultType = "Low Brightness";
                    Serial.println("low brightness");
                }
                else if(avgGlow < brightthresh && poweron >= powerthresh){
                  status = "Stable";
                  faultType = "None";
                }
            }
            work = 0;
        }

        prevglow = currentglow;

        // Update Firebase every 1 second
        if (currentmilli - lastFirebaseUpdate >= firebaseUpdateInterval) {
            lastFirebaseUpdate = currentmilli;

            // Create a FirebaseJson object
            FirebaseJson json;
            json.set("serialCode", serialCode);
            json.set("status", status);
            json.set("faultType", faultType);
            json.set("brightness", avgGlow);
            json.set("power", poweron);
            json.set("updateCount", millis());

            // Send data to Firebase
            String path = "/streetLights/" + String(serialCode);
            if (Firebase.setJSON(fbdo, path, json)) {
                Serial.println("✅ Data sent to Firebase!");
            } else {
                Serial.println("❌ Failed to send data to Firebase: " + fbdo.errorReason());
            }

            // Debug output
            // if (status == "Faulty") {
                Serial.print("Updated Firebase for ");
                Serial.print(serialCode);
                Serial.print(": ");
                Serial.println(status);
            // }
        }
    }
}
