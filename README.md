# Norcoast Audio

Static site for [norcoastaudio.com](https://norcoastaudio.com) — landing page + web tools.

## Structure

```
public/
  index.html          ← Landing page
  ambience/
    index.html        ← Norcoast Ambience synth
```

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

## Adding the Ambience synth

Replace `public/ambience/index.html` with the standalone Norcoast Ambience HTML file.
