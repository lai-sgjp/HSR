import json
from PIL import Image

"""
UI Spec Extraction — Cropping Template

Usage:
  1. Set SCREENSHOT_PATH to your reference image
  2. Adjust crop boundaries based on screenshot analysis
  3. Run: python crop_panels.py
  4. Output: crop_*.png files + color_samples.json
"""

SCREENSHOT_PATH = "C:/path/to/screenshot.png"
OUTPUT_DIR = "C:/path/to/output/"
TARGET_W = 1920
TARGET_H = 1080

# =============================================
# Step 1: Define crop boundaries
# Adjust these based on the actual screenshot
# =============================================
CROP_CONFIG = {
    # name: (x1, y1, x2, y2) in source coordinates
    "nav_bar":       (0, 0, 1920, 75),
    "left_list":     (0, 75, 1230, 1040),
    "right_detail":  (1230, 75, 1920, 1040),
    "bottom_bar":    (0, 1040, 1920, 1080),
}

# =============================================
# Step 2: Define color sample points
# A dict of {name: (x, y)} in source coordinates
# =============================================
COLOR_SAMPLES = {
    "top_left_corner":   (10, 10),
    "nav_bg":            (960, 10),
    "nav_text":          (200, 50),
    "left_panel_bg":     (50, 200),
    "card_bg_1":         (100, 300),
    "search_bg":         (50, 150),
    "divider":           (50, 210),
    "right_panel_bg":    (1400, 200),
    "right_image_bg":    (1450, 300),
    "right_text":        (1450, 400),
    "button_bg":         (1450, 700),
    "bottom_bar_bg":     (960, 1060),
    "bottom_text":       (300, 1060),
}


def crop_panels(img, config):
    """Crop image into panel pieces."""
    for name, (x1, y1, x2, y2) in config.items():
        cropped = img.crop((x1, y1, x2, y2))
        path = f"{OUTPUT_DIR}crop_{name}.png"
        cropped.save(path)
        yield name, cropped.size, path


def sample_colors(img, samples):
    """Sample colors at specified points."""
    results = {}
    for name, (x, y) in samples.items():
        c = img.getpixel((x, y))
        results[name] = {
            "hex": "#{:02x}{:02x}{:02x}".format(*c),
            "rgb": list(c),
            "avg": sum(c) / 3,
            "is_washed": sum(c) / 3 > 180,  # flag for correction
        }
    return results


def suggest_correction(color_data):
    """Suggest color override for washed-out screenshots."""
    suggested = {}
    for name, data in color_data.items():
        if data["avg"] > 180:
            suggested[name] = "⚠ NEEDS OVERRIDE — likely dark (avg > 180)"
        elif data["avg"] < 50:
            suggested[name] = "✓ Likely accurate (dark)"
        else:
            suggested[name] = f"? Check manually (avg = {data['avg']:.0f})"
    return suggested


if __name__ == "__main__":
    img = Image.open(SCREENSHOT_PATH)
    sw, sh = img.size

    print(f"Source: {sw}×{sh}")
    print(f"Target: {TARGET_W}×{TARGET_H}")
    print(f"Scale: {TARGET_W/sw:.3f}x, {TARGET_H/sh:.3f}y")
    print()

    # Crop panels
    print("=== Crops ===")
    for name, size, path in crop_panels(img, CROP_CONFIG):
        print(f"  {name:20s}: {size} → {path}")
    print()

    # Sample colors
    print("=== Color Samples ===")
    samples = sample_colors(img, COLOR_SAMPLES)
    for name, data in samples.items():
        flag = " ⚠ WASHED" if data["avg"] > 180 else " ✓" if data["avg"] < 50 else ""
        print(f"  {name:20s}: {data['hex']}  avg={data['avg']:.0f}{flag}")
    print()

    # Suggestions
    print("=== Correction Suggestions ===")
    for name, msg in suggest_correction(samples).items():
        print(f"  {name:20s}: {msg}")

    # Save color data
    with open(f"{OUTPUT_DIR}color_samples.json", "w") as f:
        json.dump(samples, f, indent=2)
