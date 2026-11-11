# Banner squares

To build one banner square, draw a sky, then three ground pieces side by side on top of it.

## File names

Every name is lowercase, uses only `a-z 0-9 _ -`, and splits its fields with `_`.

### Skies: `skies/sky_<time>_<parts>.png` (162 × 162, opaque)

- `<time>` is `day` or `night`. Night uses `moon` where day uses `sun`.
- `<parts>` come in this order: body, cloud, precipitation. Any of them can be missing.
  - body: `sun` or `moon`
  - cloud: `cloud-<small|large>-<high|low>`, or `cloud-<small|large>-<bottom-right|top-left>` when it sits in front of the sun or moon. `-x2` means two clouds.
  - precipitation: `rain` or `snow`
  - `clear` is a sky with nothing in it (faint stars at night).

Examples: `sky_day_sun_cloud-small-top-left.png`, `sky_night_cloud-small-high-x2_rain.png`, `sky_day_snow.png`

### Ground: `ground/ground_<time>_<surface>_<h0>_<h1>_<concave-up|concave-down>.png` (54 × 162, transparent sky)

- `<surface>` is `grass` or `snow`.
- `<h0>` and `<h1>` are the heights at the piece's left and right edges: 0 is low (15% up), 1 is mid (25%), 2 is high (35%). They differ by at most 1.
- On a flat piece (`h0 == h1`), `concave-up` is a shallow dip and `concave-down` is a shallow bump.
- The next piece starts where this one ends: its `h0` equals this piece's `h1`.

```cpp
QString groundFile(bool night, bool snow, int h0, int h1, bool concaveUp) {
    return QString("ground/ground_%1_%2_%3_%4_%5.png")
        .arg(night ? "night" : "day", snow ? "snow" : "grass")
        .arg(h0).arg(h1)
        .arg(concaveUp ? "concave-up" : "concave-down");
}
```

The **Ground shapes** tab of the checklist lists all 328 three-piece shapes. Each row gives its three pieces as `h0_h1_concavity`; put `ground_<time>_<surface>_` in front and `.png` after.

### One folder per square: `ground-squares/<time>_<surface>/<h0>_<h1>_<h2>_<h3>_<c1>_<c2>_<c3>/`

- Each folder holds the three pieces of one square: `left.png`, `middle.png`, `right.png`. Place them at x = 0, 54 and 108.
- `h0`–`h3` are the heights at the left edge, ⅓, ⅔ and the right edge (0 low, 1 mid, 2 high). `c1`–`c3` are the left, middle and right concavity (`concave-up` or `concave-down`).
- There are 328 folders for each of `day_grass`, `day_snow`, `night_grass` and `night_snow`, 1,312 in all. The files are copies of the pieces in `ground/`.
- The **Folder** column on the Ground shapes tab gives each row's folder name.

Example: `ground-squares/day_grass/0_1_2_2_concave-up_concave-down_concave-up/left.png`

## Joins

- Every sky in a look uses the same gradient. The sun, moon and clouds stay inside the square, so any two skies can sit side by side.
- Rain and snow use one shared pattern, spaced the same inside a square and across a join, and run down behind the ground.
- Ground shading only looks straight down, so two pieces that meet at the same height match at the join.

Each PNG has an editable SVG beside it.
