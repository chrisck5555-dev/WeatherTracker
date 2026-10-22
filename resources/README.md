# Weather Tracker – image assets

## app-icon/
- `app.ico`: exe and installer icon (16–256 px). Sizes 16–32 use the simplified `app-icon-small.svg`.
- `app.rc`: embeds the icon in the exe. Add it to your sources:
  `qt_add_executable(WeatherTracker main.cpp ... app-icon/app.rc)`
- `png/`: window/taskbar icon, e.g. `app.setWindowIcon(QIcon(":/icons/app-icon-256.png"));`

## weather-icons/<theme>/
Themes: `clay-light`, `clay-dark`, `contrast-light`, `contrast-dark`.
- `<size>/<name>.png` at 24, 32, 48, 64, 128 and 256 px
- `svg/<name>.svg`: editable source files

The clay SVGs use filters that Qt's SVG renderer may not draw, so use the clay PNGs in the app. The contrast SVGs are plain shapes and work directly with `QIcon`.

Icons: sunny, partly-cloudy, cloudy, fog, windy, drizzle, rain, thunderstorm, snow, sleet, freezing-rain, hail, and wind-from-n / ne / e / se / s / sw / w / nw.

The wind arrows use the weather convention: the name is where the wind comes from, and the arrow points where it blows. A north wind (`wind-from-n`) points down.

## installer/ (Inno Setup 6.6+)
```
[Setup]
SetupIconFile=app-icon\app.ico
WizardStyle=modern dynamic
WizardImageFile=installer\wizard-light-202x386.png,installer\wizard-light-269x515.png,installer\wizard-light-336x643.png,installer\wizard-light-403x772.png,installer\wizard-light-430x824.png,installer\wizard-light-498x953.png,installer\wizard-light-534x1022.png
WizardImageFileDynamicDark=installer\wizard-dark-202x386.png,installer\wizard-dark-269x515.png,installer\wizard-dark-336x643.png,installer\wizard-dark-403x772.png,installer\wizard-dark-430x824.png,installer\wizard-dark-498x953.png,installer\wizard-dark-534x1022.png
WizardSmallImageFile=installer\wizard-small-58.png,installer\wizard-small-77.png,installer\wizard-small-97.png,installer\wizard-small-116.png,installer\wizard-small-124.png,installer\wizard-small-143.png,installer\wizard-small-159.png
```
The small images have a transparent background, so the same set works in both light and dark mode.
