#!/usr/bin/env bash
# Norcoast Ambience — iOS project setup
# Run after cloning AND after every git pull to keep the Xcode project in sync.
# Requires: Homebrew, Xcode command-line tools

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "→ Checking for xcodegen..."
if ! command -v xcodegen &>/dev/null; then
  echo "  Installing xcodegen via Homebrew..."
  brew install xcodegen
fi

echo "→ Generating NorcoastAmbience.xcodeproj..."
xcodegen generate --spec project.yml

echo ""
echo "✓ Done. Open ios/NorcoastAmbience.xcodeproj in Xcode."
echo ""
echo "Before building:"
echo "  1. Set your Team in Signing & Capabilities"
echo "  2. Add a 1024×1024 app icon PNG to:"
echo "     ios/NorcoastAmbience/Resources/Assets.xcassets/AppIcon.appiconset/"
echo "     and update AppIcon.appiconset/Contents.json with the filename"
echo "  3. Build → Product > Archive to submit to the App Store"
