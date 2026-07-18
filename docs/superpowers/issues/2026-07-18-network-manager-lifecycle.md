# Destroy QNetworkAccessManager After Update Check

## Summary

Destroy the `QNetworkAccessManager` after the update check reply handler completes, instead of leaving it alive indefinitely. Saves 1-5 MB during standby.

## Motivation

In `flameshotdaemon.cpp:218`, the update checker creates a `QNetworkAccessManager`:

```cpp
m_networkCheckUpdates = new QNetworkAccessManager(this);
```

This allocates:
- Internal connection pool and thread for network operations
- SSL/TLS configuration data
- Request/response buffer management
- Typically 1-5 MB depending on platform SSL libraries

Once the update reply is processed and the 24-hour timer fires, the `QNetworkAccessManager` is no longer needed but persists until process exit. It's only needed again when the 24-hour timer fires for the next check.

## Changes

### 1. Clean up after reply

In the reply handler lambda (daemon.cpp ~lines 228-281), after processing the reply, call:

```cpp
m_networkCheckUpdates->deleteLater();
m_networkCheckUpdates = nullptr;
```

### 2. Re-create on the next check

The 24-hour timer (`QTimer::singleShot`) or manual update check will call `getLatestAvailableVersion()` which re-creates the manager when needed.

### 3. Handle the 24h timer edge case

The existing `QTimer::singleShot(24h, ...)` is a one-shot that fires once. On the next invocation 24 hours later, `getLatestAvailableVersion()` will be called again and create a fresh `QNetworkAccessManager`. No changes needed to the timer logic — just ensure the code path handles `m_networkCheckUpdates == nullptr`.

## Acceptance criteria

- [ ] `QNetworkAccessManager` is destroyed after the update check reply is processed
- [ ] A 24-hour timer fires correctly and creates a fresh instance for the next check
- [ ] Manual "Check for updates" button works correctly
- [ ] No crashes from accessing a dangling `m_networkCheckUpdates` pointer
- [ ] Measurable reduction in standby memory (~1-5 MB) after the first update check completes

## Blocked by

None — can start immediately
