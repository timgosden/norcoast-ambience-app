# Norcoast Ambience — marketing site

Marketing landing page for [Norcoast Ambience](https://timgosden.github.io/norcoast-ambience-app/) — a single-page funnel into the App Store.

Live at [norcoastaudio.com](https://norcoastaudio.com).

The synth itself is developed in a separate repo and hosted on GitHub Pages.

## Structure

```
public/
  index.html        ← Marketing landing page (App Store funnel)
  sw.js             ← Cleanup worker (unregisters legacy root SW)
  icons/
    icon.svg
```

Replace the `href="#"` on the App Store buttons in `public/index.html`
with the real App Store URL once the app is live (two spots — search for
`TODO: replace href`).

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
