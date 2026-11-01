#pragma once
#include <cstddef>
namespace smoke {
// Match the limiter's credential-shape policy; this does not authenticate WiFi.
inline bool validCredentialShape(size_t ssidLength,size_t passwordLength) {
 return ssidLength>0 && ssidLength<=32 && passwordLength<=63;
}
inline bool applyNetworkDefaults(bool localSecrets,size_t storedSsid,size_t storedPassword,size_t localSsid,size_t localPassword) {
 return localSecrets && !validCredentialShape(storedSsid,storedPassword) && validCredentialShape(localSsid,localPassword);
}
}
