#include "WiFiHelper.h"

void connectWiFi(const char *ssid, const char *password)
{
	Serial.println("[WiFi] Connecting...");
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(1000);
		Serial.print(".");
	}
	Serial.println("connected");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());
}
