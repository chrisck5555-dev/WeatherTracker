#pragma once

#include <QString>

// RZ9: three width bands, by the window's width in layout px (RZ1). Pages switch between
// one and two columns only at these points; groups, grids and tiles adapt by what fits.
namespace Responsive {

enum class Band { Narrow, Medium, Wide };

Band band(int layoutWidth);         // Narrow under 640, Medium 640-1007, Wide 1008 and up
QString bandName(Band b);           // "Narrow", "Medium", "Wide"

// Two columns of cards or groups only in the Wide band.
inline bool twoColumns(Band b) { return b == Band::Wide; }

} // namespace Responsive
