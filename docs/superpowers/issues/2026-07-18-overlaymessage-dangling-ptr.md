# Fix Dangling OverlayMessage::m_instance Pointer

## Summary

Add a destructor to `OverlayMessage` that nulls the static `m_instance` pointer when the widget is destroyed, preventing a use-after-free risk.

## Motivation

`overlaymessage.cpp:38` declares a static `OverlayMessage* m_instance` pointer. This is set at line 17 in the constructor:

```cpp
m_instance = this;
```

The `OverlayMessage` is parented to `CaptureWidget` (created at overlaymessage.cpp:40). When `CaptureWidget` is destroyed (user closes the capture), Qt deletes all child widgets including `OverlayMessage`. However, no destructor is defined to set `m_instance = nullptr`, leaving a dangling pointer.

Any call to `OverlayMessage::instance()` or any static method that dereferences `m_instance` after `CaptureWidget` is destroyed would cause undefined behavior (use-after-free).

The risk is currently **low** because:
- `OverlayMessage` is only used during active capture sessions
- No code paths currently call `instance()` after capture closure

However, as the codebase evolves, this is a latent bug.

## Changes

### 1. Add a destructor

```cpp
OverlayMessage::~OverlayMessage()
{
    if (m_instance == this) {
        m_instance = nullptr;
    }
}
```

### 2. Declare in header

Add the destructor declaration to `overlaymessage.h`.

## Acceptance criteria

- [ ] `OverlayMessage` has a destructor that nulls `m_instance` when it matches `this`
- [ ] No change to OverlayMessage behavior during capture sessions
- [ ] After `CaptureWidget` is destroyed, `m_instance` is `nullptr`

## Blocked by

None — can start immediately
