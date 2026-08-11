#pragma once

#include "defines.h"

#define FIRST_PLACE watchface
#define PLACE_TREE_MAX_DEPTH 15

typedef enum
{
    watchface,
    mainMenu,
    debugMenu,
    settingsMenu,
    utilitiesMenu,
    generalDebug,
    clockDebug,
    batteryDebug,
    motorDebug,
    wifiDebug,
    gitDebug,
    accDebug,
    weatherMenu,             // Weather main menu
    weatherDateMenu,         // Weather submenu (formally weatherMenu)
    weatherConditionMenu,    // Weather date selection
    airQualityDateMenu,      // Air quality submenu
    airQualityConditionMenu, // Air quality date selection
    alarmSelectorMenu,
    alarmEditMenu,
    alarmRing,
    alarmQuick,
    alarmSetChooser,
    setTimePlace,
    setDatePlace,
    alarmEditDays,
    pomodoroMenu,
    powerMenu,
    book,
    bookSelector,
    vault,
    vaultImage, // because of exit function
    vaultMenu,  // because of exit function
    apple,
    apple2,
    calendarDateMenu,
    calendarEventMenu,
    videoPlayer,
    videoMenu,
    pong,
    gotchi,
    paint,
    maze,
    tetris,
    jumperGame,
    snake,
    diceMenu,
    diceApp,
    gamesMenu,
    credits,
    conwayApp,
    partyApp,
    healthMenu,
    heartMonitor,
    qrApp,
    rssReader,
    haControl,
    chessApp,
    fsUpload,
    presenceBeaconCfg,
    stopwatch,
    moonSun,
    subnetCalc,
    worldClock,
    pinoutWallet,
    rtcDriftLab,
    issPasses,
    baikyApp,
    notesManager,
    notesApp,
    bluetoothHostScanner,
    blePeripheral,
    blePeripheralMenu,
    bleTagScanner,
    fontPreview,
    fontPreviewMenu,
    watchfaceSelector,
    setTimeWatchPlace,
    setTimezoneUtcWatchPlace,
    setTimezoneContinentWatchPlace,
    setTimezoneCityWatchPlace,
    setDateWatchPlace,
    setClockMenu,
    setTimezoneMenu,
    dailyStepsChartMenu,
    // General places now:
    inputPinPlace,
    generalMenuPlace,
    imagePlace,
    textDialog,
    errorDialog,
    ChartPlace,
    NoPlace,
} UiPlace;

extern int currentPlaceIndex;
extern UiPlace placeTree[PLACE_TREE_MAX_DEPTH];
extern UiPlace currentPlace;
extern void (*exitFunc)();
extern void (*exitFuncGlob)(); // Executed when a place is exiting and it was requested

extern bool disabledBacking;

void executeExitFunc();

void initManager();
void loopManager();

// Manager functions
void initMainMenu();
#if DEBUG_MENUS
void initDebugMenu();
#endif
void initSettingsMenu();
void initUtilitiesMenu();
void initGamesMenu();
#if HEALTH_MENU
void initHealthMenu();
#endif
#if WEATHER_INFO
void initWeatherMenu();
void initWeatherDateMenu();
void initWeatherConditionMenu();
void initAirQualityDateMenu();
void initAirQualityConditionMenu();
#endif
void initpowerMenu();
#if FONT_MENU_ENABLED
void initFontMenu();
void switchFontsPreviewMenu();
#endif

// Manager switches
void generalSwitch(UiPlace place);
void overwriteSwitch(UiPlace place);

void switchDebugMenu();
void switchSettingsMenu();
void switchUtilitiesMenu();
void switchGamesMenu();
void switchGeneralDebug();
void switchClockDebug();
void switchBatteryDebug();
void switchWifiDebug();
void switchGitDebug();
void switchAccDebug();
void switchMotorDebug();
#if HEALTH_MENU
void switchHealthMenu();
#endif
#if QR_APP
void switchQrApp();
#endif
#if RSS_READER
void switchRssReader();
#endif
#if HA_CONTROL
void switchHaControl();
#endif
#if CHESS
void switchChessApp();
#endif
#if FS_UPLOAD
void switchFsUpload();
#endif
#if PRESENCE_BEACON
void switchPresenceBeaconCfg();
#endif
#if STOPWATCH
void switchStopwatch();
#endif
#if PRECISE_STEP_COUNTING
void switchDailyStepsChartMenu();
#endif
void switchWeatherMenu();
void switchMoonSun();
void switchSubnetCalc();
#if WORLD_CLOCK_APP
void switchWorldClock();
#endif
#if PINOUT_WALLET_APP
void switchPinoutWallet();
#endif
#if RTC_DRIFT_LAB_APP
void switchRtcDriftLab();
#endif
#if ISS_PASSES_APP
void switchIssPasses();
#endif
void switchWeatherDateMenu();
void switchWeatherConditionMenu();
void switchAirQualityDateMenu();
void switchAirQualityConditionMenu();
#if INK_ALARMS
void switchAlarmSelectorMenu();
void switchAlarmEditMenu();
void switchAlarmEditDays();
void switchAlarmRing();
void switchAlarmSetChooser();
void switchAlarmQuick();
void switchPomodoroMenu();
#endif
#if SET_TIME_GUI
void switchSetTime();
#endif
#if SET_DATE_GUI
void switchSetDate();
#endif
void switchPowerMenu();
void switchWatchfaceSelectorMenu();

void switchBook();
void switchBookSelector();
void switchVault();
void switchBack();
#if VIDEO_PLAYER
void switchVideoPlayer();
void switchVideoMenu();
#endif
void switchApple();
void switchApple2();
#if PONG
void switchPong();
#endif
#if GOTCHI
void switchGotchi();
#endif
#if PAINT
void switchPaint();
#endif
#if MAZE
void switchMaze();
#endif
#if TETRIS
void switchTetris();
#endif
#if JUMPER
void switchJumper();
#endif
#if SNAKE
void switchSnake();
#endif
#if DICE
void switchDiceMenu();
void switchDiceApp();
#endif
#if CREDITS
void switchCredits();
#endif
#if CONWAY
void switchConway();
#endif
#if RGB_DIODE
void switchParty();
#endif
#if HEART_MONITOR
void switchHeartMonitor();
#endif
#if BAIKY
void switchBaiky();
#endif
#if NOTES_APP
void switchNotes();
void switchNotesManager();
#endif
#if BLE_HOST_ENABLED
void switchBluetoothHostScanner();
#endif
#if BLE_PERIPHERAL
void switchBlePeripheral();
void switchBlePeripheralMenu();
#endif
#if BLE_TAG_SCANNER
void switchBleTagScanner();
#endif
#if FONT_MENU_ENABLED
void switchFontsPreview();
#endif
void showTextDialog(String str, bool center, String title = "");
#if SET_CLOCK_GUI
void switchSetTimeWatch();
void switchSetTimezoneUtcWatch();
void switchSetTimezoneContinentWatchPlace();
void switchSetTimezoneCityWatchPlace();
void switchSetDateWatch();
#endif
