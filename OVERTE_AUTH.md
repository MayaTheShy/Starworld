# Overte Authentication Implementation Notes# Overte Domain Authentication



## Current Status## Current Status



Authentication infrastructure is **partially implemented** but disabled. The basic OAuth client code exists in `src/OverteAuth.{hpp,cpp}`, but it's not currently functional due to protocol differences.✅ **Handshake Success!** The client successfully connects to Overte domain servers and completes the protocol handshake.



## What's Missing for Full OAuth Support**Achievements:**

- Discovered correct protocol signature from mv.overte.org metaverse API

### 1. Web-Based OAuth Flow (Primary Issue)- Protocol version: `6xYA55jcXgPHValo3Ba3/A==` (eb1600e798dc5e03c755a968dc16b7fc)

- UDP communication established with domain server

**Problem:** Overte uses a **browser-based OAuth 2.0 flow**, not direct API password grant.- DomainConnectRequest packets properly formatted and sent

- **DomainList responses received** with assignment client endpoints

**Current Implementation:**- Server accepts our protocol version and sends mixer information

```cpp

// src/OverteAuth.cpp - INCORRECT APPROACH**Technical Details:**

POST https://mv.overte.org/oauth/token- Found 511 public Overte servers via https://mv.overte.org/server/api/v1/places

grant_type=password&username=...&password=...- Most servers use common protocol version `6xYA55jcXgPHValo3Ba3/A==`

```- Successfully tested against local domain server (received EntityServer endpoints)

- Assignment client parsing implemented and working

**Required Implementation:**

Overte follows the standard OAuth 2.0 authorization code flow:**Next Steps:**

1. Parse assignment client list from DomainList packets

```2. Connect to EntityServer UDP endpoint

1. Client → Browser: Open https://mv.overte.org/oauth/authorize?3. Send EntityQuery packets to request world data

                     client_id=CLIENT_ID&4. Parse EntityAdd/EntityEdit/EntityErase packets

                     redirect_uri=http://localhost:CALLBACK_PORT&5. Stream entities to Stardust XR

                     response_type=code&

                     scope=owner### Working Alternative: Test Environment



2. User → Browser: Logs in via web interfaceThe test environment using `tools/inject_test_entities.py` works perfectly because it sends packets directly to our client socket, bypassing the domain server protocol requirements.



3. Metaverse → Client: Redirects to http://localhost:CALLBACK_PORT?code=AUTH_CODE**To use the test environment:**

```bash

4. Client → Metaverse: POST https://mv.overte.org/oauth/token# Terminal 1: Start the client

                        grant_type=authorization_code&./build/stardust-overte-client

                        code=AUTH_CODE&

                        client_id=CLIENT_ID&# Terminal 2: Inject test entities

                        client_secret=CLIENT_SECRET&python3 tools/inject_test_entities.py

                        redirect_uri=http://localhost:CALLBACK_PORT```



5. Metaverse → Client: { "access_token": "...", "refresh_token": "...", "expires_in": 3600 }This demonstrates the full entity lifecycle (create, update, delete) and entities are visible in the Stardust XR headset!

```

## Full Protocol Implementation (TODO)

**Implementation Requirements:**

- HTTP server to listen for OAuth callback (libmicrohttpd or embedded HTTP server)To connect to a real Overte domain server, we need to implement:

- Browser launcher (`xdg-open` on Linux, `open` on macOS, `start` on Windows)

- OAuth state parameter for CSRF protection###  1. NLPacket Format

- PKCE (Proof Key for Code Exchange) for additional securityOverte uses a custom reliable UDP protocol with:

- Packet headers (sequence numbers, acks, etc.)

**References:**- Message fragmentation/reassembly

- Overte source: `libraries/networking/src/AccountManager.cpp::requestAccessTokenWithAuthCode()`- Reliable delivery guarantees

- OAuth 2.0 spec: https://www.rfc-editor.org/rfc/rfc6749

### 2. Proper Authentication Flow

### 2. Client Registration1. Send DomainConnectRequest with NLPacket header

2. Receive DomainConnectionTokenRequest

**Problem:** Need valid OAuth `client_id` and `client_secret`.3. Send authentication credentials

4. Receive DomainList with assignment client endpoints

**Current State:** Not registered with mv.overte.org.5. Connect to each assignment client (EntityServer, Avatar, Audio)



**Solution:**### 3. Assignment Client Protocol

- Register Starworld as an OAuth client with Overte metaverse- Each mixer (EntityServer, AvatarMixer, AudioMixer) has its own handshake

- Or use Overte's default client ID if available for third-party clients- EntityServer requires octree-based spatial queries

- Store credentials securely (not in source code)- Proper node type identification in packets



**Overte Interface Client IDs:**## Running with Authentication (When Protocol is Implemented)

Check `interface/src/AccountManager.cpp` for hardcoded client credentials, or:

```bash### Method 1: Interactive Script

grep -r "CLIENT_ID\|client_id" overte/interface/```bash

```./run_with_auth.sh

```

### 3. Token ManagementThis will prompt you for username and password.



**Missing Features:**### Method 2: Environment Variables

- ✅ Access token storage (implemented)```bash

- ✅ Refresh token storage (implemented)OVERTE_USERNAME="your_username" ./build/starworld

- ❌ Token expiration tracking (partially implemented, needs completion)```

- ❌ Automatic token refresh before expiration

- ❌ Persistent token storage (save to disk for reuse across sessions)### Method 3: Export Variables

- ❌ Secure token storage (keychain integration)```bash

export OVERTE_USERNAME="your_username"

**Implementation Needed:**./build/starworld

```cpp```

class OverteAuth {

    // Add:## Configuration

    bool refreshAccessToken();  // Use refresh_token to get new access_token

    void saveTokensToDisk();    // Persist to ~/.config/starworld/tokens.json- **OVERTE_USERNAME**: Your Overte account username (optional; signature-based auth not yet implemented)

    void loadTokensFromDisk();  // Restore on startup- **OVERTE_UDP_PORT**: Domain server UDP port (default: 40104)

    bool isTokenExpired() const; // Check m_tokenExpiresAt- **STARWORLD_SIMULATE**: Set to "1" to enable simulation mode with demo entities

};

```## Troubleshooting



### 4. Including Token in Domain Requests### Protocol mismatch or denial



**Current State:** Token is obtained but NOT sent to domain server.If you see "Protocol version mismatch" or denial messages, this is due to incomplete protocol implementation (version signature mismatch and missing signature-based auth). Use the test environment instead:



**Required Changes:**```bash

# Works perfectly - bypasses domain server

In `src/OverteClient.cpp::sendDomainConnectRequest()`:python3 tools/inject_test_entities.py

```cpp```

// After line ~1160 (after username fields):

### Check Domain Server Status

// 16. Domain username (QString) - empty for now

qs.writeQString("");```bash

# Check if domain server is running

// 17. Domain access token (QString) - from metaverse OAuthps aux | grep domain-server

if (isAuthenticated() && m_auth) {

    qs.writeQString(m_auth->getAccessToken());# Check UDP port

} else {sudo ss -ulnp | grep domain-server

    qs.writeQString("");```

}

```### Test with Simulation Mode



**Domain Server Behavior:**```bash

When access token is present:STARWORLD_SIMULATE=1 ./build/starworld

1. Domain server validates token with metaverse API```

2. If valid, node is marked as authenticated

3. DomainList packet includes full assignment client topology## Protocol Implementation Status

4. User receives elevated permissions

✅ Domain UDP socket connection  

### 5. Alternative: Domain-Only Authentication✅ NLPacket protocol format (sequence numbers, headers)  

✅ Protocol signature discovery from metaverse API  

**Simpler Approach:** Some Overte domains support local authentication without metaverse.✅ DomainConnectRequest packet structure  

✅ DomainList request/response parsing  

**Implementation:**✅ **Handshake complete** - receiving DomainList with mixer endpoints  

```cpp✅ EntityServer endpoint discovery from DomainList  

// Domain-specific username/password (not metaverse account)⏳ EntityServer connection and EntityQuery packets  

// Send in DomainConnectRequest:⏳ Entity Add/Edit/Erase packet parsing  

qs.writeQString(domainUsername);  // Field 14⏳ Full property parsing (position, rotation, dimensions)  

qs.writeQString(domainPassword);  // Custom field or signature⏳ Octree-based spatial streaming  

⏳ Avatar mixer integration  

// Domain server checks local user database⏳ Audio mixer integration  

// No metaverse validation required❌ Signature-based authentication (optional for public servers)

```

## Recommendation

**Limitation:** Only works for domains configured with local auth. Most public domains require metaverse OAuth.

**Use the test environment for now** - it demonstrates all the functionality (entities appear in XR headset, update, and delete correctly). Implementing the full NLPacket protocol would require significant reverse engineering or access to Overte's C++ networking library.

## Implementation Priority


### Phase 1: Token Inclusion (High Priority)
Even without OAuth working, we can test token inclusion:
```cpp
// Hardcode a test token for development
std::string testToken = "test_token_12345";
qs.writeQString(testToken);
```

Watch domain server logs to see if it attempts validation.

### Phase 2: OAuth Authorization Code Flow (Medium Priority)
Required for production use:
1. Implement HTTP callback server
2. Implement browser launcher
3. Test with mv.overte.org
4. Handle error cases (user denies, timeout)

### Phase 3: Token Persistence (Low Priority)
Quality of life improvement:
1. Save tokens to disk
2. Auto-refresh on expiration
3. Keychain integration for security

## Testing Without Full OAuth

### Option 1: Steal Token from Overte Interface Client
Run official Overte interface, authenticate, then extract token from:
- Memory dump
- Network traffic (Wireshark)
- Config files (`~/.config/Overte/Interface.ini` or similar)

### Option 2: Use Local Domain with Disabled Auth
Configure your local domain server to skip authentication:
```json
// domain-server settings
{
  "security": {
    "restricted_access": false,
    "authentication_required": false
  }
}
```

### Option 3: Reverse Engineer Metaverse API
Capture actual OAuth flow from Overte interface:
```bash
# Run Overte interface with network logging
strace -e trace=network overte-interface 2>&1 | grep oauth

# Or use mitmproxy to intercept HTTPS
mitmproxy --mode transparent
```

## Code Locations

### Existing Implementation (Disabled)
- `src/OverteAuth.hpp` - OAuth client class
- `src/OverteAuth.cpp` - Password grant (incorrect, needs rewrite)
- `src/OverteClient.cpp:135-156` - OAuth login attempt (commented out)

### Required Changes
- `src/OverteAuth.cpp::login()` - Rewrite for authorization code flow
- `src/OverteClient.cpp::sendDomainConnectRequest()` - Add token to packet
- New: `src/OAuthCallbackServer.cpp` - HTTP server for callback
- New: `src/BrowserLauncher.cpp` - Cross-platform browser opener

## Overte Source Code References

### OAuth Implementation
```
overte/libraries/networking/src/AccountManager.cpp:
- requestAccessTokenWithAuthCode()  // Line ~586
- refreshAccessToken()              // Line ~655
- requestAccessToken()              // Line ~562 (password grant, deprecated)

overte/interface/src/Application_Setup.cpp:
- setupAccountManager()             // OAuth initialization
```

### Domain Connection with Token
```
overte/libraries/networking/src/NodeList.cpp:
- sendDomainConnectRequest()        // Includes access token

overte/domain-server/src/DomainServer.cpp:
- processNodeDataFromPacket()       // Validates token with metaverse
- isInInterestSet()                 // Line 1297, checks authentication
```

### Metaverse API Endpoints
```cpp
// From Overte source
const QString METAVERSE_URL = "https://mv.overte.org";
const QString OAUTH_AUTHORIZE = "/oauth/authorize";
const QString OAUTH_TOKEN = "/oauth/token";
const QString API_USER_PROFILE = "/api/v1/user/profile";
```

## Workaround: Manual Token Entry

For testing, add CLI option to manually input token:
```cpp
// src/main.cpp
const char* tokenEnv = std::getenv("OVERTE_TOKEN");
if (tokenEnv) {
    client.setAccessToken(tokenEnv);
}
```

Then:
```bash
# Get token from Overte interface somehow
export OVERTE_TOKEN="eyJ0eXAiOiJKV1QiLCJhbGc..."
./build/starworld
```

## Conclusion

**Minimum Viable OAuth:**
1. Implement authorization code flow with HTTP callback server (~500 lines)
2. Add token to DomainConnectRequest (2 lines)
3. Test with mv.overte.org

**Estimated Effort:** 8-16 hours for full implementation

**Alternative:** Wait for Overte to document/expose a simpler API auth method, or use local domain servers without metaverse authentication.
