# Norcoast Ambience — Brand Style Guide

The single source of truth for colours, type, and the logo. All values pulled
directly from the live synth UI. Match these exactly across web, App Store,
and any marketing.

---

## 1. Logo / Brand mark

The mark is a circle containing three stacked sine-waves (gold, green, pink)
with an optional uppercase **N** above. The waves represent layered ambient
pads — never replace them with anything else.

### Primary mark (square, with N) — for app icon, OG images, social

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512">
  <rect width="512" height="512" rx="115" fill="#060810"/>
  <circle cx="256" cy="256" r="182" stroke="rgba(196,145,94,0.35)" stroke-width="7" fill="none"/>
  <path d="M 82 256 Q 144 153 205 256 Q 266 359 327 256 Q 388 153 449 256"
        stroke="rgba(196,145,94,0.82)" stroke-width="11" stroke-linecap="round" fill="none"/>
  <path d="M 82 256 Q 144 199 205 256 Q 266 313 327 256 Q 388 199 449 256"
        stroke="rgba(94,184,138,0.48)" stroke-width="8" stroke-linecap="round" fill="none"/>
  <path d="M 100 256 Q 163 222 226 256 Q 289 290 352 256 Q 415 222 449 256"
        stroke="rgba(212,107,138,0.3)" stroke-width="5" stroke-linecap="round" fill="none"/>
  <text x="256" y="148" text-anchor="middle" fill="rgba(196,145,94,0.55)"
        font-family="sans-serif" font-size="62" font-weight="600" letter-spacing="5">N</text>
  <circle cx="256" cy="256" r="156" stroke="rgba(255,255,255,0.04)" stroke-width="2" fill="none"/>
</svg>
```

### Inline mark (no background, no letter) — for nav, footer, splash

```svg
<svg viewBox="0 0 100 100" fill="none">
  <circle cx="50" cy="50" r="46" stroke="rgba(196,145,94,0.4)" stroke-width="1.5"/>
  <path d="M 20 50 Q 30 35, 40 50 Q 50 65, 60 50 Q 70 35, 80 50"
        stroke="rgba(196,145,94,0.85)" stroke-width="2" stroke-linecap="round" fill="none"/>
  <path d="M 20 50 Q 30 40, 40 50 Q 50 60, 60 50 Q 70 40, 80 50"
        stroke="rgba(94,184,138,0.55)" stroke-width="1.5" stroke-linecap="round" fill="none"/>
</svg>
```

### Animated splash mark (with concentric ring + 3 waves)

```svg
<svg viewBox="0 0 100 100" fill="none">
  <circle cx="50" cy="50" r="46" stroke="rgba(196,145,94,0.18)" stroke-width="1"/>
  <circle cx="50" cy="50" r="46" stroke="rgba(196,145,94,0.55)" stroke-width="1.5"/>
  <path d="M 18 50 Q 29 30, 40 50 Q 51 70, 62 50 Q 73 30, 84 50"
        stroke="rgba(196,145,94,0.92)" stroke-width="2.5" stroke-linecap="round" fill="none"/>
  <path d="M 18 50 Q 29 38, 40 50 Q 51 62, 62 50 Q 73 38, 84 50"
        stroke="rgba(94,184,138,0.6)" stroke-width="1.6" stroke-linecap="round" fill="none"/>
  <path d="M 22 50 Q 33 42, 44 50 Q 55 58, 66 50 Q 77 42, 88 50"
        stroke="rgba(212,107,138,0.4)" stroke-width="1.1" stroke-linecap="round" fill="none"/>
</svg>
```

### Wordmark

The brand wordmark is the word **Ambience** set in **Open Sans 200** with
**4px letter-spacing**. Pair with the eyebrow **NORCOAST AUDIO** in
**Open Sans 400, 5px letter-spacing, uppercase, gold @ 50% opacity**.

---

## 2. Typography

**Family:** Open Sans (Google Fonts).
Always include weights `200;300;400;500;600`.

```html
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Open+Sans:wght@200;300;400;500;600&display=swap" rel="stylesheet">
```

```css
body {
  font-family: 'Open Sans', sans-serif;
  -webkit-font-smoothing: antialiased;
}
```

### Type scale

| Use                    | Size     | Weight | Letter-spacing | Case      |
| ---------------------- | -------- | ------ | -------------- | --------- |
| Hero headline          | 44–72px  | 200    | -1px           | Title     |
| Section headline (h2)  | 28–40px  | 200    | -0.5px         | Title     |
| Splash wordmark        | 32px     | 200    | 4px            | Title     |
| Sub-headline / lead    | 17–21px  | 300    | normal         | Sentence  |
| Body                   | 14–15px  | 400    | 0.2px          | Sentence  |
| Body muted             | 13–14px  | 300    | normal         | Sentence  |
| Eyebrow (category)     | 10–11px  | 600    | 3–5px          | UPPERCASE |
| Pill / button          | 11–13px  | 400–500| 1.5–3px        | UPPERCASE |
| Micro / version / meta | 9–10px   | 400    | 0.5–1px        | Mixed     |

**Letter-spacing is part of the brand.** Never drop it on uppercase elements.

---

## 3. Colour

### 3.1 Background — the most-violated rule

> Never go darker than `#06080d`. The website was using `#040609` at the
> bottom of the radial gradient — that is **outside** the brand range and
> reads as black. Match the synth gradient instead.

**Canonical surface (use this on the marketing site too):**

```css
background: linear-gradient(160deg, #07090e, #0b0f1a 40%, #06080d);
```

Solid fallback for splash / icon backgrounds: `#080b14` (or `#060810` for
the app icon mask).

### 3.2 Brand accents

| Token         | Value     | Usage                                              |
| ------------- | --------- | -------------------------------------------------- |
| `gold`        | `#c4915e` | **Primary accent.** Active states, CTAs, headline punctuation. |
| `gold-dim`    | `#a08060` | Disabled / paused state of gold elements.          |
| `green`       | `#5eb88a` | Secondary — waveform layer, "OK" / valid state.    |
| `pink`        | `#d46b8a` | Secondary — waveform layer, fade / decay.          |
| `blue`        | `#6a9fd8` | Secondary — pending / scheduled state.             |

**Rule:** gold is the only accent that competes with white text. Green,
pink, and blue exist in the icon waveforms and inside the synth UI for
status — they should rarely appear in marketing UI as solid swatches.

### 3.3 Text on dark

| Use            | Value                       |
| -------------- | --------------------------- |
| Primary text   | `#ffffff` / `rgba(255,255,255,0.92)` |
| Body           | `rgba(255,255,255,0.85)`    |
| Muted          | `rgba(255,255,255,0.55)`    |
| Dim            | `rgba(255,255,255,0.32)`    |
| Very dim       | `rgba(255,255,255,0.15)`    |
| Ghost / disabled | `rgba(255,255,255,0.07)`  |

### 3.4 Surfaces and borders

| Use                | Value                       |
| ------------------ | --------------------------- |
| Card bg            | `rgba(255,255,255,0.04)` to `0.07` |
| Hairline border    | `rgba(255,255,255,0.08)` to `0.12` |
| Strong border      | `rgba(255,255,255,0.18)`    |

### 3.5 Gold tint stack (active states)

```css
--gold-bg-soft:    rgba(196,145,94,0.08);   /* hover */
--gold-bg:         rgba(196,145,94,0.15);   /* active fill */
--gold-bg-strong:  rgba(196,145,94,0.22);   /* pressed */
--gold-border:     rgba(196,145,94,0.35);
--gold-border-hi:  rgba(196,145,94,0.55);
--gold-glow:       0 0 20px rgba(196,145,94,0.18);
```

### 3.6 Drop-in CSS variables

```css
:root {
  --bg:            #06080d;
  --bg-2:          #0b0f1a;
  --bg-3:          #07090e;
  --surface:       linear-gradient(160deg, #07090e, #0b0f1a 40%, #06080d);

  --gold:          #c4915e;
  --gold-dim:      #a08060;
  --green:         #5eb88a;
  --pink:          #d46b8a;
  --blue:          #6a9fd8;

  --text:          rgba(255,255,255,0.92);
  --text-body:     rgba(255,255,255,0.85);
  --text-mute:     rgba(255,255,255,0.55);
  --text-dim:      rgba(255,255,255,0.32);
  --text-ghost:    rgba(255,255,255,0.15);

  --line:          rgba(255,255,255,0.08);
  --line-strong:   rgba(255,255,255,0.18);

  --gold-bg:       rgba(196,145,94,0.15);
  --gold-border:   rgba(196,145,94,0.35);
  --gold-glow:     0 0 20px rgba(196,145,94,0.18);
}
```

---

## 4. Voice and copy patterns

- Lowercase technical, Title-case headlines.
- Period at end of short headlines: **"Ambience."** "Five dollars. Once."
- Eyebrow words are short and uppercase: `NORCOAST AUDIO`, `WHAT'S INSIDE`.
- Numbers are featured: **$5**, **2MB**, **0** — used as visual weight.
- Tone: confident, plain-spoken, no hype words. Never "revolutionary",
  "next-gen", "AI-powered". The product is the proof.

---

## 5. Component patterns

### Pill (status / price tag)
```css
font-size: 11px;
letter-spacing: 2.5px;
text-transform: uppercase;
color: var(--gold);
background: var(--gold-bg-soft);   /* rgba(196,145,94,0.08) */
border: 1px solid rgba(196,145,94,0.25);
padding: 6px 14px;
border-radius: 20px;
```

### Primary button (the only one with a pure-white surface)
The white App Store button is the **only** non-dark surface in the system.
It exists because Apple's badge convention demands it. Don't add other
white buttons — secondary actions are ghost links in gold or muted white.

### Ghost link / secondary CTA
```css
color: var(--gold);
border-bottom: 1px solid rgba(196,145,94,0.2);
padding-bottom: 1px;
```

### Card / pillar
```css
padding: 28px 24px;
border: 1px solid var(--line);
border-radius: 16px;
background: rgba(255,255,255,0.015);
```

---

## 6. Don't

- Don't use pure black (`#000`) or anything below `#06080d`.
- Don't introduce new accent colours — use gold, or dim white, period.
- Don't use sans-serif other than Open Sans.
- Don't drop letter-spacing on uppercase elements.
- Don't centre everything — the synth is left-aligned grid; marketing can
  be centred for hero/CTA but body sections (features) should be left-aligned.
- Don't add gradients with more than 3 stops, or any gradient that's not
  along the 160° axis.
