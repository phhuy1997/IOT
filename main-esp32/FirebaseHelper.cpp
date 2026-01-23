#include "FirebaseHelper.h"

void setupFirebase(FirebaseData &data,
									 FirebaseAuth &auth,
									 FirebaseConfig &config,
									 const char *host,
									 const char *authToken)
{
	config.database_url = host;
	config.signer.tokens.legacy_token = authToken;

	Firebase.begin(&config, &auth);
	Firebase.reconnectWiFi(true); // Automatically reconnect WiFi
}
