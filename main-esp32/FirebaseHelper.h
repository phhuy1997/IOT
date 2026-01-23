#ifndef FIREBASE_HELPER_H
#define FIREBASE_HELPER_H

#include <FirebaseESP32.h>

void setupFirebase(FirebaseData &data,
									 FirebaseAuth &auth,
									 FirebaseConfig &config,
									 const char *host,
									 const char *authToken);

#endif // FIREBASE_HELPER_H
