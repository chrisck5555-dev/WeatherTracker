#pragma once

class QImage;

// The glass blur (G4, G11): three passes of a box blur in each direction, which looks like
// a Gaussian blur and costs the same at any radius. It works on premultiplied ARGB images,
// usually a quarter-size copy of what passes under the glass.
namespace BoxBlur {

// Blurs `image` in place. Each pass averages 2 * radius + 1 pixels; edges repeat the
// outermost pixel. A radius below 1 leaves the image as it is.
void blur(QImage &image, int radius);

// The per-pass radius that gives a blur of `blurRadius` (G4's 24 px) on an image drawn at
// `scale` of the real size (0.25 for the quarter-size copy). Three passes of radius r spread
// about as far as one Gaussian of radius 2r, so r is half the scaled radius.
int passRadius(int blurRadius, double scale);

} // namespace BoxBlur
