#include "BoxBlur.h"

#include <QImage>
#include <QVector>
#include <algorithm>
#include <cmath>

namespace BoxBlur {

namespace {

// One pass along a line of `n` pixels, `stride` pixels apart, starting at `first`.
// `line` is scratch space for a copy of the line.
void passLine(quint32 *first, int n, int stride, int r, QVector<quint32> &line)
{
    for (int i = 0; i < n; ++i)
        line[i] = first[i * stride];
    auto px = [&](int i) { return line[std::clamp(i, 0, n - 1)]; };
    int s[4] = {0, 0, 0, 0};
    for (int i = -r; i <= r; ++i) {
        const quint32 c = px(i);
        for (int k = 0; k < 4; ++k)
            s[k] += (c >> (8 * k)) & 0xff;
    }
    const int count = 2 * r + 1;
    for (int i = 0; i < n; ++i) {
        quint32 out = 0;
        for (int k = 0; k < 4; ++k)
            out |= quint32((s[k] + count / 2) / count) << (8 * k);
        first[i * stride] = out;
        const quint32 in = px(i + r + 1), gone = px(i - r);
        for (int k = 0; k < 4; ++k)
            s[k] += int((in >> (8 * k)) & 0xff) - int((gone >> (8 * k)) & 0xff);
    }
}

} // namespace

void blur(QImage &image, int radius)
{
    if (radius < 1 || image.isNull())
        return;
    if (image.format() != QImage::Format_ARGB32_Premultiplied)
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = image.width(), h = image.height();
    const int stride = int(image.bytesPerLine() / 4);
    auto *bits = reinterpret_cast<quint32 *>(image.bits());
    QVector<quint32> line(std::max(w, h));
    for (int pass = 0; pass < 3; ++pass)
        for (int y = 0; y < h; ++y)
            passLine(bits + y * stride, w, 1, radius, line);
    for (int pass = 0; pass < 3; ++pass)
        for (int x = 0; x < w; ++x)
            passLine(bits + x, h, stride, radius, line);
}

int passRadius(int blurRadius, double scale)
{
    return std::max(1, int(std::lround(blurRadius * scale / 2.0)));
}

} // namespace BoxBlur
