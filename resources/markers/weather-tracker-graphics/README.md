# Weather Tracker – extra graphics

Drop these folders into your resources folder. Every look uses the same folder names as the weather icons (`clay-light`, `clay-dark`, `contrast-light`, `contrast-dark`).

## markers/<look>/
- `24/`, `32/`, `48/`: `marker-weather.png` (thermometer), `marker-note.png` (folded note page), `marker-event.png` (flag)
- `svg/`: editable sources. The clay SVGs use filters, so the program should load the PNGs.

Measured contrast of each marker body against its day tile (median of the solid pixels):

| Look | Weather | Note | Event |
|---|---|---|---|
| clay-light | 5.5 : 1 | 4.1 : 1 | 5.2 : 1 |
| clay-dark | 5.1 : 1 | 6.8 : 1 | 7.2 : 1 |
| contrast-light | 10.0 : 1 | 6.8 : 1 | 7.4 : 1 |
| contrast-dark | 9.8 : 1 | 14.7 : 1 | 16.2 : 1 |

The clay shading goes slightly darker than the colours in the brief, which keeps the light amber note above 3:1 everywhere except its soft highlight rim.

## empty-day/<look>/
- `128/empty-day.png`, `256/empty-day.png`, `svg/empty-day.svg`: a blank calendar page with a small sun peeking out

## backgrounds/<clay-light | clay-dark>/
Each PNG has its SVG source beside it. There are no files for the high-contrast looks.

- `background-page.png`: 512 × 512, repeats seamlessly in both directions. Tile it, don't stretch it. For example:
  `palette.setBrush(QPalette::Window, QBrush(QPixmap(":/backgrounds/clay-light/background-page.png")));`
- `banner-sunny|cloudy|rain|snow|plain.png`: 1200 × 160. Every pixel is text-safe, so the date heading can go anywhere on the banner.
- `tile-winter|spring|summer|autumn.png`: 360 × 240. The middle 60% is plain, and the side edges at mid-height are also kept clear so long names like "September" stay clean.

Worst-case text contrast, measured on every pixel where text can sit:

| | Main text | Secondary text |
|---|---|---|
| Page (light / dark) | 11.5 / 12.4 : 1 | 5.7 / 7.4 : 1 |
| Banners (light / dark) | ≥ 9.1 / ≥ 7.5 : 1 | ≥ 4.5 / ≥ 4.5 : 1 |
| Tile centres (light / dark) | ≥ 12.2 / ≥ 10.7 : 1 | ≥ 6.1 / ≥ 6.4 : 1 |

The brief's limit for banners and tiles (#B0BAD0 / #434D6D) only guarantees about 3.6:1 for secondary text. These files use a stricter limit, so both main and secondary text pass.
