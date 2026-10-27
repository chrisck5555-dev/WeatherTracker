cmake_minimum_required(VERSION 3.21)
project(WeatherTracker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)   # compiles resources/resources.qrc (the images) into the program

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Network Test)

# Core: GUI-free logic shared by the app and the tests
add_library(WeatherCore STATIC
    src/Period.h
    src/PeriodMath.cpp
    src/PeriodMath.h
    src/WeatherDay.h
    src/Repository.cpp
    src/Repository.h
    src/Format.cpp
    src/Format.h
    src/EventTypes.h
    src/WeatherInput.cpp
    src/WeatherInput.h
    src/Units.cpp
    src/Units.h
    src/TextSize.cpp
    src/TextSize.h
    src/Stats.cpp
    src/Stats.h
    src/StatsRows.cpp
    src/StatsRows.h
    src/History.cpp
    src/History.h
    src/ChartMath.cpp
    src/ChartMath.h
    src/AppLog.cpp
    src/AppLog.h
    src/ProblemReport.cpp
    src/ProblemReport.h
    src/Theme.cpp
    src/Theme.h
    src/Records.cpp
    src/Records.h
    src/Backups.cpp
    src/Backups.h
    src/UpdateCheck.cpp
    src/UpdateCheck.h
)
target_include_directories(WeatherCore PUBLIC src)
target_link_libraries(WeatherCore PUBLIC Qt6::Core Qt6::Gui Qt6::Sql)

# The application
add_executable(WeatherTracker WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/AppState.cpp
    src/AppState.h
    src/PeriodSelector.cpp
    src/PeriodSelector.h
    src/MonthGridWidget.cpp
    src/MonthGridWidget.h
    src/Markers.cpp
    src/Markers.h
    src/DayView.cpp
    src/DayView.h
    src/WeekView.cpp
    src/WeekView.h
    src/YearView.cpp
    src/YearView.h
    src/NumberField.cpp
    src/NumberField.h
    src/EditorView.cpp
    src/EditorView.h
    src/Confirm.cpp
    src/Confirm.h
    src/Display.cpp
    src/Display.h
    src/SettingsView.cpp
    src/SettingsView.h
    src/StatsView.cpp
    src/StatsView.h
    src/TempChart.cpp
    src/TempChart.h
    src/ChartsView.cpp
    src/ChartsView.h
    src/BarChart.cpp
    src/BarChart.h
    src/LineChart.cpp
    src/LineChart.h
    src/RecordsView.cpp
    src/RecordsView.h
    src/PanelHost.cpp
    src/PanelHost.h
    src/SearchPanel.cpp
    src/SearchPanel.h
    src/RestoreDialog.cpp
    src/RestoreDialog.h
    src/UpdateChecker.cpp
    src/UpdateChecker.h
    src/TourOverlay.cpp
    src/TourOverlay.h
    src/GuideDialog.cpp
    src/GuideDialog.h
    src/GoToDatePanel.cpp
    src/GoToDatePanel.h
    src/CrashGuard.cpp
    src/CrashGuard.h
    src/ThemeManager.cpp
    src/ThemeManager.h
    src/ClayPaint.cpp
    src/ClayPaint.h
    src/ClayStyle.cpp
    src/ClayStyle.h
    src/Artwork.cpp
    src/Artwork.h
    src/WeatherIcons.cpp
    src/WeatherIcons.h
    resources/app-icon/app.rc
    resources/resources.qrc
)

target_link_libraries(WeatherTracker PRIVATE Qt6::Widgets Qt6::Network WeatherCore)

# Tests
enable_testing()
add_executable(tst_periodmath tests/tst_periodmath.cpp)
target_link_libraries(tst_periodmath PRIVATE WeatherCore Qt6::Test)
add_test(NAME periodmath COMMAND tst_periodmath)

add_executable(tst_repository tests/tst_repository.cpp)
target_link_libraries(tst_repository PRIVATE WeatherCore Qt6::Test)
add_test(NAME repository COMMAND tst_repository)

add_executable(tst_format tests/tst_format.cpp)
target_link_libraries(tst_format PRIVATE WeatherCore Qt6::Test)
add_test(NAME format COMMAND tst_format)

add_executable(tst_weatherinput tests/tst_weatherinput.cpp)
target_link_libraries(tst_weatherinput PRIVATE WeatherCore Qt6::Test)
add_test(NAME weatherinput COMMAND tst_weatherinput)

add_executable(tst_units tests/tst_units.cpp)
target_link_libraries(tst_units PRIVATE WeatherCore Qt6::Test)
add_test(NAME units COMMAND tst_units)

add_executable(tst_stats tests/tst_stats.cpp)
target_link_libraries(tst_stats PRIVATE WeatherCore Qt6::Test)
add_test(NAME stats COMMAND tst_stats)

add_executable(tst_textsize tests/tst_textsize.cpp)
target_link_libraries(tst_textsize PRIVATE WeatherCore Qt6::Test)
add_test(NAME textsize COMMAND tst_textsize)

add_executable(tst_statsrows tests/tst_statsrows.cpp)
target_link_libraries(tst_statsrows PRIVATE WeatherCore Qt6::Test)
add_test(NAME statsrows COMMAND tst_statsrows)

add_executable(tst_history tests/tst_history.cpp)
target_link_libraries(tst_history PRIVATE WeatherCore Qt6::Test)
add_test(NAME history COMMAND tst_history)

add_executable(tst_chartmath tests/tst_chartmath.cpp)
target_link_libraries(tst_chartmath PRIVATE WeatherCore Qt6::Test)
add_test(NAME chartmath COMMAND tst_chartmath)

add_executable(tst_theme tests/tst_theme.cpp)
target_link_libraries(tst_theme PRIVATE WeatherCore Qt6::Test)
add_test(NAME theme COMMAND tst_theme)

add_executable(tst_records tests/tst_records.cpp)
target_link_libraries(tst_records PRIVATE WeatherCore Qt6::Test)
add_test(NAME records COMMAND tst_records)

add_executable(tst_dateparse tests/tst_dateparse.cpp)
target_link_libraries(tst_dateparse PRIVATE WeatherCore Qt6::Test)
add_test(NAME dateparse COMMAND tst_dateparse)

add_executable(tst_backups tests/tst_backups.cpp)
target_link_libraries(tst_backups PRIVATE WeatherCore Qt6::Test)
add_test(NAME backups COMMAND tst_backups)

add_executable(tst_updatecheck tests/tst_updatecheck.cpp)
target_link_libraries(tst_updatecheck PRIVATE WeatherCore Qt6::Test)
add_test(NAME updatecheck COMMAND tst_updatecheck)

add_executable(tst_filecheck tests/tst_filecheck.cpp)
target_link_libraries(tst_filecheck PRIVATE WeatherCore Qt6::Test)
add_test(NAME filecheck COMMAND tst_filecheck)

# Tools
add_executable(bench_cells tools/bench_cells.cpp)
target_link_libraries(bench_cells PRIVATE Qt6::Widgets)

add_executable(make_demo_data tools/make_demo_data.cpp)
target_link_libraries(make_demo_data PRIVATE WeatherCore)