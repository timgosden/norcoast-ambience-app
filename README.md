# Norcoast Ambience

Standalone generative ambient synthesizer — web app + iOS wrapper.

Live at [norcoastaudio.com](https://norcoastaudio.com) (linked from the main site).

## Structure

```
public/
  index.html                   ← Ambience synth (self-contained, ~2MB)
  manifest.json                ← PWA manifest (install to home screen)
  sw.js                        ← Service worker (offline support)
  dattorro-reverb-worklet.js   ← AudioWorklet for the plate reverb
  phase-vocoder-worklet.js     ← AudioWorklet for shimmer pitch shift
  icons/
    icon.svg
    icon-maskable.svg
ios/
  project.yml                  ← xcodegen spec for the WKWebView wrapper
  setup.sh                     ← Generates the Xcode project from project.yml
  NorcoastAmbience/            ← Swift sources, Info.plist, entitlements
```

## Local dev

```bash
npm install
npm run dev
# → http://localhost:3000
```

## Deploy

GitHub Actions auto-deploys to GitHub Pages on every push to `main`. The
workflow lives at `.github/workflows/deploy.yml` and uploads the contents
of `public/` as the Pages artefact.

## Install as a PWA

**Android:** Open in Chrome → three-dot menu → Add to Home Screen

**iPhone / iPad:** Open in Safari → Share → Add to Home Screen

The app caches fully after first load and works offline.

## Plugin (parallel experiment)

A separate JUCE-based port lives under `plugin/`. It builds to AU,
VST3, CLAP, and Standalone from one C++ codebase. Currently a
phase-1 skeleton; see `plugin/README.md` for build instructions
and roadmap. The standalone web app is unaffected by anything in
that directory.

## iOS app (App Store)

The Xcode project lives in `ios/`. It wraps the web app in a `WKWebView`
with `AVAudioSession` configured for `.playback` so audio runs in the
background and routes through the lock screen.

```bash
cd ios
./setup.sh                # generates NorcoastAmbience.xcodeproj
open NorcoastAmbience.xcodeproj
```

Before building:

1. Set your Team in Signing & Capabilities
2. Bump `MARKETING_VERSION` and `CURRENT_PROJECT_VERSION` in `project.yml`
3. Add a 1024×1024 app icon PNG under `NorcoastAmbience/Resources/Assets.xcassets/AppIcon.appiconset/`
4. Product → Archive → submit
