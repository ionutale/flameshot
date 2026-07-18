# Optimize PixelateTool CPU Usage

## Summary

Replace the per-pixel `QImage::pixel()`/`setPixel()` calls with direct memory access via `QImage::bits()`, pre-generate noise tables instead of per-pixel `std::normal_distribution`, and reduce fringe image copies from 4 to 1.

## Motivation

`pixelatetool.cpp:101-222` implements the "secure" pixelation mode with three nested loops and heavy per-pixel operations:

```cpp
for (int x = 0; x < width; ++x)
    for (int y = 0; y < height; ++y)
        for (int i = 0; i < 4; ++i)
            // QImage::pixel() — virtual dispatch per call
            // std::normal_distribution(prng) — heavy RNG
```

For a 500x500 pixelated area with `size=5`:
- 50 × 50 × 4 = 10,000 `QImage::pixel()` calls
- 10,000 `std::normal_distribution` draws from `std::mt19937`
- 4 `QPixmap::copy().toImage()` chains for fringe extraction

`QImage::pixel()` is a virtual dispatch that does bounds checking. `std::mt19937` + `std::normal_distribution` is relatively heavy for per-pixel use.

## Changes

### 1. Direct pixel buffer access

Replace `QImage::pixel(x, y)` with `QImage::bits()` pointer arithmetic. Access pixel data as 32-bit ARGB values directly. This avoids virtual dispatch and bounds checking on every pixel.

```
// Before: qRed(fringe[i].pixel(fringeX, fringeY));
// After: reinterpret_cast<const QRgb*>(fringe[i].constBits())[y * stride + x]
```

### 2. Pre-generate noise tables

Generate a small noise lookup table (e.g., 256 entries) from `std::normal_distribution` once, and index into it with `(x + y * width) & 255` during the inner loop. Eliminates per-pixel RNG draws.

### 3. Single fringe copy

Instead of 4 separate `pixmap.copy().toImage()` calls for the fringe regions (lines 131-150 in source), copy the full selection once as a QImage, then access fringe pixels via offset arithmetic on the same buffer.

### 4. Optional: SIMD for weight interpolation

The inner loop does 3-channel float interpolation with clamping (lines 186-213). This is a good candidate for SSE/NEON vectorization via a small `#if defined(__SSE2__)` path or by structuring the code for auto-vectorization.

## Acceptance criteria

- [ ] No `QImage::pixel()` or `setPixel()` calls in the inner loop
- [ ] Noise is pre-generated into a lookup table, not generated per pixel
- [ ] Visual output is identical to current (same noise pattern since seed is fixed at 42)
- [ ] Fringe extraction uses a single copy instead of 4
- [ ] Measurable 2-5x performance improvement in secure pixelation mode
- [ ] Insecure pixelation path is unaffected

## Blocked by

None — can start immediately
