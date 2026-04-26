# Norcoast Ambience

Standalone generative ambient synthesizer — web app + iOS wrapper.

Live at [norcoastaudio.com](https://norcoastaudio.com) (linked from the main site).

## Structure

```
public/
  index.html        ← Marketing landing page (App Store funnel)
  sw.js             ← Cleanup worker (unregisters legacy root SW)
  icons/
    icon.svg        ← Favicon for the marketing site
  app/
    index.html      ← Ambience synth (self-contained, ~2MB)
    manifest.json   ← PWA manifest (install to home screen)
    sw.js           ← Service worker (offline support)
    pitch-shifter-worklet.js
    icons/
      icon.svg
      icon-maskable.svg
```

The marketing site lives at `/`; the synth itself runs at `/app`. Replace
the `href="#"` on the App Store buttons in `public/index.html` with the
real App Store URL once the app is live.

## Local dev

```bash
npm install
npm run dev
# → http://localhost:3000
```

## Deploy (Railway)

1. Push to GitHub
2. Connect repo in Railway dashboard
3. Railway auto-detects Node, runs `npm start`
4. Add custom domain in Railway settings

The `start` script uses `serve` to host the `public/` directory. Railway provides `$PORT` automatically.

## Install on phone

**Android:** Open `/app` in Chrome → three-dot menu → Add to Home Screen

**iPhone:** Open `/app` in Safari → Share → Add to Home Screen

The app caches fully after first load and works offline.

## iOS app

The Xcode project wrapping the synth in a WKWebView lives in this repo alongside the web app. See `ios/` (coming).
