# OAuth Authentication Testing Guide

## Overview

Starworld now includes full OAuth 2.0 browser-based authentication for connecting to Overte servers with your user account.

## Quick Test

### Test 1: Browser OAuth Flow

```bash
cd /home/mayatheshy/stardust/Starworld
./build/starworld --auth --overte=127.0.0.1:40102
```

**Expected Behavior:**
1. Application starts callback server on port 8765
2. Browser opens to Overte login page
3. You log in with your Overte credentials
4. Browser redirects to localhost callback
5. Application receives authorization code
6. Token is exchanged and saved
7. Connection to domain server proceeds with authentication

**Success Indicators:**
- `[Auth] Starting browser-based authentication...`
- `[OverteAuth] Callback server listening on port 8765`
- `[OverteAuth] Opening browser to: https://...`
- `[OverteAuth] Received authorization code: ...`
- `[OverteAuth] Successfully exchanged authorization code for token`
- `[OverteAuth] Token saved to ~/.config/starworld/overte_token.txt`

### Test 2: Saved Token Reuse

After Test 1 succeeds, run again:

```bash
./build/starworld --auth --overte=127.0.0.1:40102
```

**Expected Behavior:**
- No browser opens
- Token loaded from file
- If token is still valid, no re-authentication needed
- If token expired but refresh token valid, automatic refresh

**Success Indicators:**
- `[Auth] Using saved token for <username>`
- `[OverteAuth] Loaded saved token for <username>`
- No browser window opens

### Test 3: Token Refresh

To test automatic token refresh, manually edit the token file to make it expire soon:

```bash
# Edit expiration timestamp
nano ~/.config/starworld/overte_token.txt
# Change last line (timestamp) to current time + 500 seconds
```

Then run:
```bash
./build/starworld --auth --overte=127.0.0.1:40102
```

**Expected Behavior:**
- Token detected as expiring soon
- Automatic refresh attempt
- New token saved

**Success Indicators:**
- `[OverteAuth] Token expiring soon, refreshing...`
- `[OverteAuth] Successfully refreshed access token`

### Test 4: Entity Data with Authentication

With authentication enabled, you should receive full assignment client topology:

```bash
./build/starworld --auth --overte=127.0.0.1:40102
```

**Look for:**
- `[OverteClient] Parsed X assignment clients` (X > 0)
- `[OverteClient] Assignment client: EntityServer at ...`
- `[OverteClient] Entity server found at ...`
- Entity data packets received

Compare with anonymous connection:
```bash
./build/starworld --overte=127.0.0.1:40102  # No --auth
```

You should see:
- `[OverteClient] Parsed 0 assignment clients`
- `[OverteClient] Warning: No EntityServer found in assignment client list`

## Troubleshooting

### Browser Doesn't Open

**Symptom:** `[OverteAuth] Failed to open browser`

**Solution:**
1. Manually copy the URL from console
2. Paste into browser
3. Complete authentication
4. Application will receive callback automatically

### Port 8765 Already in Use

**Symptom:** `[OverteAuth] Failed to bind socket`

**Solution:**
The callback server will automatically try a different port (OS-assigned). Check console for actual port number.

### CSRF State Mismatch

**Symptom:** `[OverteAuth] State mismatch - possible CSRF attack!`

**Cause:** Browser navigated to old/cached authorization URL

**Solution:**
1. Clear browser cache
2. Restart authentication flow
3. Use the freshly generated URL

### Token File Permissions

**Symptom:** Cannot save/load token

**Solution:**
```bash
# Ensure config directory exists
mkdir -p ~/.config/starworld
chmod 700 ~/.config/starworld

# Fix token file permissions
chmod 600 ~/.config/starworld/overte_token.txt
```

### Testing Against Different Metaverse Servers

```bash
# Use custom metaverse server
OVERTE_METAVERSE=https://my-custom-server.org ./build/starworld --auth

# Or for self-hosted domain servers
./build/starworld --auth --overte=my-domain.local:40102
```

## File Locations

- **Token Storage:** `~/.config/starworld/overte_token.txt`
- **Token Format:** 
  ```
  <metaverse_url>
  <username>
  <access_token>
  <refresh_token>
  <expiration_timestamp>
  ```

## Security Notes

1. **Token File:** Contains sensitive authentication tokens. File permissions are automatically set to 0600 (user read/write only).

2. **Callback Server:** Binds to `localhost` only, preventing external access.

3. **CSRF Protection:** Uses random state parameter to prevent cross-site request forgery.

4. **HTTPS:** OAuth endpoints use HTTPS for secure token exchange.

5. **Token Lifetime:** Tokens automatically refresh before expiration (1 hour threshold).

## Comparison: Anonymous vs Authenticated

| Feature | Anonymous | Authenticated |
|---------|-----------|---------------|
| Public domains | ✅ | ✅ |
| Private domains | ❌ | ✅ |
| Assignment client list | ❌ (empty) | ✅ (full topology) |
| Direct EntityServer | ❌ | ✅ |
| Entity editing | ❌ | ✅ (future) |
| Voice chat | ❌ | ✅ (future) |
| Performance | Slower (domain server fallback) | Faster (direct connections) |

## Next Steps

Once OAuth authentication works, the next phase is to:
1. Use access token in EntityQuery packets
2. Establish direct connections to EntityServer
3. Parse full entity data with authentication
4. Test with your local Overte server's entities

## Example Session

```
$ ./build/starworld --auth --overte=127.0.0.1:40102

[Auth] ===================================
[Auth] Overte OAuth Authentication
[Auth] Metaverse: https://mv.overte.org
[Auth] ===================================
[Auth] Starting browser-based authentication...
[OverteAuth] Callback server listening on port 8765
[OverteAuth] ===================================
[OverteAuth] Opening browser for authentication
[OverteAuth] If browser doesn't open automatically, navigate to:
[OverteAuth] https://mv.overte.org/oauth/authorize?response_type=code&client_id=starworld&redirect_uri=http%3A%2F%2Flocalhost%3A8765%2Fcallback&scope=owner&state=a1b2c3d4...
[OverteAuth] ===================================
[OverteAuth] Opening browser to: https://mv.overte.org/oauth/...
[OverteAuth] Waiting for authentication callback...

(User logs in via browser...)

[OverteAuth] Callback server thread started
[OverteAuth] Received authorization code: Xy7zQ9...
[OverteAuth] Exchanging authorization code for access token...
[OverteAuth] Token received, expires in 7200 seconds
[OverteAuth] Access token: eyJhbGciOiJSUzI1...
[OverteAuth] Token saved to /home/user/.config/starworld/overte_token.txt
[OverteAuth] Successfully exchanged authorization code for token
[OverteAuth] Callback server thread stopped
[Auth] ✓ Successfully authenticated!
[Auth] Username: your_username

[StardustBridge] Loaded Rust bridge: ./bridge/target/debug/libstardust_bridge.so
[StardustBridge] Connected via Rust bridge (C-ABI).
[main] Connecting to Overte domain: 127.0.0.1:40102
[OverteClient] Session UUID: a1b2c3d4-...
[OverteClient] Connecting to domain at 127.0.0.1 (HTTP:40100, UDP:40102)
...
[OverteClient] Parsed 6 assignment clients
[OverteClient] Assignment client: EntityServer at 192.168.1.100:33237
[OverteClient] Entity server found at 192.168.1.100:33237
[OverteClient] Domain connected! Sending entity query to entity-server...
```
