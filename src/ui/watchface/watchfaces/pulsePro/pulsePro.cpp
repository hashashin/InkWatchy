#include "pulsePro.h"

#if WATCHFACE_PULSEPRO

#include "rtcMem.h"

// -------------------------
// Fonts
// -------------------------
#define PP_TIME_FONT  getFont("inkfield/JackInput40")
#define PP_SMALL_FONT getFont("dogicapixel4")
#define PP_DATE_FONT  getFont("JackInput17")
#define PP_DAY_FONT   getFont("JackInput17")
#define PP_BAT_FONT   getFont("DisposableDroidBB9")

// -------------------------
// Layout
// -------------------------
static constexpr int16_t PP_TIME_Y = 56;

static constexpr int16_t PP_BAND_TIME_Y0  = 0;
static constexpr int16_t PP_BAND_TIME_Y1  = 66;

static constexpr int16_t PP_BAND_DAY_Y0   = 66;
static constexpr int16_t PP_BAND_DAY_Y1   = 92;

static constexpr int16_t PP_BAND_TOP_Y0   = 92;
static constexpr int16_t PP_BAND_TOP_Y1   = 118;

static constexpr int16_t PP_BAND_WEATH_Y0 = 118;
static constexpr int16_t PP_BAND_WEATH_Y1 = 160;

static constexpr int16_t PP_DATE_X = 10;
static constexpr int16_t PP_DATE_Y = 112;

static constexpr int16_t PP_WIFI_Y = 92;
static constexpr int16_t PP_BATT_Y = 110;

static constexpr int16_t PP_MOON_X = 8;
static constexpr int16_t PP_MOON_Y = 124;

static constexpr int16_t PP_WEATHER_ICON_X = 96;
static constexpr int16_t PP_WEATHER_ICON_Y = 122;

static constexpr int16_t PP_TEMP_X = 138;
static constexpr int16_t PP_TEMP_Y = 148;

static constexpr int16_t PP_MODULE_Y = 160;
static constexpr int16_t PP_MODULE_H = 40;

// -------------------------
static int g_lastWeatherCode = -9999;
static int g_lastMoonDay = -1;
static String g_lastTimeDrawn = "";

// -------------------------
static void clearRect(int16_t x,int16_t y,int16_t w,int16_t h)
{
    dis->fillRect(x,y,w,h,SCWhite);
    dUChange=true;
}

static void clearTimeArea(){clearRect(0,PP_BAND_TIME_Y0,200,PP_BAND_TIME_Y1-PP_BAND_TIME_Y0);}
static void clearDayArea(){clearRect(0,PP_BAND_DAY_Y0,200,PP_BAND_DAY_Y1-PP_BAND_DAY_Y0);}
static void clearTopInfoArea(){clearRect(0,PP_BAND_TOP_Y0,200,PP_BAND_TOP_Y1-PP_BAND_TOP_Y0);}
static void clearTempArea(){clearRect(0,PP_BAND_WEATH_Y0,200,PP_BAND_WEATH_Y1-PP_BAND_WEATH_Y0);}

// -------------------------
static String timeStr(tmElements_t t)
{
#if WATCHFACE_12H
    int h=t.Hour;
    if(h==0)h=12;
    else if(h>12)h-=12;
    String hh=String(h); if(hh.length()==1)hh="0"+hh;
#else
    String hh=String(t.Hour); if(hh.length()==1)hh="0"+hh;
#endif
    String mm=String(t.Minute); if(mm.length()==1)mm="0"+mm;
    return hh+":"+mm;
}

static String dayStr()
{
    String d=getLocalizedDayByIndex(timeRTCLocal.Wday,0);
    d.toUpperCase();
    return d;
}

static String dateStr()
{
    String dd=String(timeRTCLocal.Day); if(dd.length()==1)dd="0"+dd;
    String mon=getLocalizedMonthName(timeRTCLocal.Month);
    mon.toUpperCase();
    return dd+" "+mon;
}

// -------------------------
static int16_t centeredDayBaseline(const String& txt)
{
    setTextSize(1);
    setFont(PP_DAY_FONT);

    String tmp=txt;

    int16_t x1=0,y1=0;
    uint16_t w=0,h=0;
    getTextBounds(tmp,&x1,&y1,&w,&h);

    int bandH=PP_BAND_DAY_Y1-PP_BAND_DAY_Y0;
    int16_t baseline=PP_BAND_DAY_Y0+(bandH+h)/2-2;

    return baseline;
}

// -------------------------
static String getInkfieldWeatherIcon(int code)
{
    if(code==0)return"inkfield/clear_sky";
    if(code>=1&&code<=3)return"inkfield/overcast";
    if(code==45||code==48)return"inkfield/fog";
    if((code>=51&&code<=67)||(code>=80&&code<=82))return"inkfield/rain";
    if((code>=71&&code<=77)||code==85||code==86)return"inkfield/snow";
    if(code>=95)return"inkfield/thunderstorm";
    return"inkfield/no_weather_data";
}

// -------------------------
static double moonPhase01()
{
    const double synodic=29.53058867;
    time_t now=makeTime(timeRTCLocal);
    const time_t ref=947182440;

    double days=(double)(now-ref)/86400.0;
    double frac=fmod(days,synodic);
    if(frac<0)frac+=synodic;
    return frac/synodic;
}

static void drawMoonIcon(int16_t x,int16_t y)
{
    clearRect(x,y,24,24);

    double p=moonPhase01();
    double t=(p<=0.5)?(p/0.5):((p-0.5)/0.5);
    bool waxing=(p<=0.5);

    int cx=x+12;
    int cy=y+12;
    int r=10;

    dis->fillCircle(cx,cy,r,SCBlack);

    int maxShift=r-1;
    int shift=(int)round((1.0-t)*(double)maxShift);
    if(!waxing)shift=-shift;

    dis->fillCircle(cx+shift,cy,r,SCWhite);
    dis->drawCircle(cx,cy,r,SCBlack);

    dUChange=true;
}

// -------------------------
static void drawWifiIcon(int16_t x,int16_t y)
{
    wifiStatusSimple st=wifiStatusWrap();
    clearRect(x,y,22,22);

    if(st==WifiOff)writeImageN(x,y,getImg("wifiOff"));
    else if(st==WifiOn)writeImageN(x,y,getImg("wifiOn"));
    else writeImageN(x,y,getImg("wifiConnected"));

    dUChange=true;
}

// -------------------------
static int16_t drawBatteryPercent()
{
    int pct=constrain((int)rM.batteryPercantageWF,0,100);
    String s=String(pct)+"%";

    setTextSize(1);
    setFont(PP_BAT_FONT);

    int16_t x1=0,y1=0;
    uint16_t w=0,h=0;
    getTextBounds(s,&x1,&y1,&w,&h);

    const int16_t rightMargin=6;
    int16_t x=200-(int16_t)w-rightMargin;

    clearRect(x-2,PP_BAND_TOP_Y0,w+6,PP_BAND_TOP_Y1-PP_BAND_TOP_Y0);
    writeTextReplaceBack(s,x,PP_BATT_Y,SCBlack,SCWhite);
    dUChange=true;

    return x;
}

// -------------------------
static int16_t measureTimeTextW(const String& s)
{
    setTextSize(1);
    setFont(PP_TIME_FONT);

    String tmp=s;

    int16_t x1=0,y1=0;
    uint16_t w=0,h=0;
    getTextBounds(tmp,&x1,&y1,&w,&h);

    return (int16_t)w;
}

static int16_t centeredXForTime(const String& t)
{
    int16_t w=measureTimeTextW(t);
    int16_t x=(200-w)/2;
    if(x<0)x=0;
    return x;
}

// -------------------------
static void drawTimeBeforeApply()
{
    // Night slow refresh: lo gestiona el CORE (NIGHT_SLEEP_*), aquí no hacemos throttle con millis().
    // Solo dibujamos cuando cambia el string de hora/minuto.
    String newT=timeStr(timeRTCLocal);

    if(g_lastTimeDrawn=="")
    {
        clearTimeArea();
        setTextSize(1);
        setFont(PP_TIME_FONT);
        writeTextReplaceBack(newT,centeredXForTime(newT),PP_TIME_Y,SCBlack,SCWhite);
        g_lastTimeDrawn=newT;
        return;
    }

    if(newT==g_lastTimeDrawn)return;

    if(newT.substring(0,2)!=g_lastTimeDrawn.substring(0,2))
    {
        clearTimeArea();
        setTextSize(1);
        setFont(PP_TIME_FONT);
        writeTextReplaceBack(newT,centeredXForTime(newT),PP_TIME_Y,SCBlack,SCWhite);
        g_lastTimeDrawn=newT;
        return;
    }

    setTextSize(1);
    setFont(PP_TIME_FONT);

    int16_t x=centeredXForTime(newT);
    clearRect(x+60,PP_BAND_TIME_Y0,100,PP_BAND_TIME_Y1);
    writeTextReplaceBack(newT,x,PP_TIME_Y,SCBlack,SCWhite);

    g_lastTimeDrawn=newT;
}

// -------------------------
static void drawDay()
{
    String d=dayStr();

    clearDayArea();

    setTextSize(1);
    setFont(PP_DAY_FONT);
    writeTextCenterReplaceBack(d,centeredDayBaseline(d),SCBlack,SCWhite);
}

static void drawMonth()
{
    String ds=dateStr();

    clearTopInfoArea();

    setTextSize(1);
    setFont(PP_DATE_FONT);
    writeTextReplaceBack(ds,PP_DATE_X,PP_DATE_Y,SCBlack,SCWhite);

    int16_t battX=drawBatteryPercent();

    const int16_t wifiW=20;
    const int16_t gap=6;
    int16_t wifiX=battX-wifiW-gap;
    if(wifiX<110)wifiX=110;

    drawWifiIcon(wifiX,PP_WIFI_Y);
}

static void drawBattery(){(void)drawBatteryPercent();}

// -------------------------
static void drawTimeAfterApply(bool forceDraw)
{
#if WEATHER_INFO
    OM_OneHourWeather w=weatherGetDataHourly(WEATHER_WATCHFACE_HOUR_OFFSET);

    int today=(int)timeRTCLocal.Day;
    int wcode=(int)w.weather_code;

    bool need=
        forceDraw||
        (w.fine&&rM.pulsepro.lastTemp!=(int)round(w.temp))||
        (w.fine&&g_lastWeatherCode!=wcode)||
        (g_lastMoonDay!=today)||
        (!w.fine&&g_lastWeatherCode!=-9999);

    if(!need)return;

    clearTempArea();

    drawMoonIcon(PP_MOON_X,PP_MOON_Y);
    g_lastMoonDay=today;

    if(w.fine){
        writeImageN(PP_WEATHER_ICON_X,PP_WEATHER_ICON_Y,getImg(getInkfieldWeatherIcon(w.weather_code)));
        g_lastWeatherCode=wcode;

        setTextSize(1);
        setFont(PP_DATE_FONT);
        writeTextReplaceBack(formatTemperature(w.temp),PP_TEMP_X,PP_TEMP_Y,SCBlack,SCWhite);

        rM.pulsepro.lastTemp=(int)round(w.temp);
    }else{
        writeImageN(PP_WEATHER_ICON_X,PP_WEATHER_ICON_Y,getImg("inkfield/no_weather_data"));
        g_lastWeatherCode=-9999;

        setTextSize(1);
        setFont(PP_DATE_FONT);
        String dash="--";
        writeTextReplaceBack(dash,PP_TEMP_X,PP_TEMP_Y,SCBlack,SCWhite);

        rM.pulsepro.lastTemp=-9999;
    }
#else
    (void)forceDraw;
#endif
}

// -------------------------
static void showTimeFull()
{
    clearRect(0,0,200,PP_MODULE_Y);

    setTextSize(1);
    setFont(PP_TIME_FONT);
    String t=timeStr(timeRTCLocal);
    writeTextReplaceBack(t,centeredXForTime(t),PP_TIME_Y,SCBlack,SCWhite);

    drawDay();
    drawMonth();
    drawTimeAfterApply(true);
}

// -------------------------
static void initWatchface()
{
    rM.pulsepro.lastTemp=-9999;
    g_lastWeatherCode=-9999;
    g_lastMoonDay=-1;
    g_lastTimeDrawn="";
}

// -------------------------
static void manageInput(buttonState bt)
{
    if(bt!=None)resetSleepDelay(SLEEP_EVERY_MS);

    if(bt==Up){wfModuleSwitch(Left);return;}
    if(bt==Down){wfModuleSwitch(Right);return;}

    if(bt==Menu){
        generalSwitch(UiPlace::mainMenu);
        return;
    }

#if LONG_BACK_FULL_REFRESH
    if(bt==LongBack){
        updateDisplay(true);
        return;
    }
#endif
}

static void lpCoreScreenPrepareCustom(){}

static const squareInfo kModuleArea={
    .size{.w=200,.h=PP_MODULE_H},
    .cord{.x=0,.y=PP_MODULE_Y}
};

const watchfaceDefOne pulseProDef={
    .drawTimeBeforeApply=drawTimeBeforeApply,
    .drawTimeAfterApply =drawTimeAfterApply,
    .drawDay            =drawDay,
    .drawMonth          =drawMonth,
    .showTimeFull       =showTimeFull,
    .initWatchface      =initWatchface,
    .drawBattery        =drawBattery,
    .manageInput        =manageInput,

    .watchfaceModules   =true,
    .watchfaceModSquare =kModuleArea,
    .someDrawingSquare  = {.size{.w=0,.h=0},.cord{.x=0,.y=0}},
    .isModuleEngaged    =isModuleEngaged,
    .lpCoreScreenPrepareCustom=lpCoreScreenPrepareCustom
};

#endif