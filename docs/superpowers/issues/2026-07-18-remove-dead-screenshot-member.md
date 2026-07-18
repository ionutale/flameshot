# Remove Dead `Screenshot` Member from ScreenGrabber

## Summary

Remove the `Screenshot` member variable from `ScreenGrabber` — declared but never used in any implementation code.

## Motivation

`screengrabber.h:45` declares:

```cpp
QPixmap Screenshot;
```

Searching the entire codebase confirms this member is never read or written anywhere in `screengrabber.cpp` or any other file. All screenshot data is returned by value from the grab methods (e.g., `grabEntireDesktop()`, `grabFullDesktop()`). The member is dead code.

Dead code adds:
- Confusion for developers reading the class API
- Unnecessary object tracking overhead (though minimal for a single `QPixmap`)
- Potential copy/move constructor overhead if the compiler doesn't optimize it out

## Changes

### 1. Remove from header

Delete line 45 from `screengrabber.h`:

```diff
-    QPixmap Screenshot;
```

### 2. Verify no references

Confirm no `.cpp` file references `this->Screenshot` or `m.Screenshot` or `grabber.Screenshot`.

## Acceptance criteria

- [ ] `QPixmap Screenshot` is removed from `ScreenGrabber` class
- [ ] Build succeeds with no references to the removed member
- [ ] No behavioral change

## Blocked by

None — can start immediately
