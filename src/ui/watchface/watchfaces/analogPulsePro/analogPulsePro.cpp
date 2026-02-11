#include "analogPulsePro.h"

#if WATCHFACE_ANALOG_PULSEPRO

#include "rtcMem.h"
#include <math.h>

// Dial geometry
static constexpr int16_t CX = 100;
static constexpr int16_t CY = 100;
static constexpr int16_t R  = 90;

// Font for hour numbers
#define F_NUM getFont("UbuntuMono10")

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static inline void clearAll()
{
    dis->fillScreen(SCWhite);
    dUChange = true;
}

static inline void setF(const GFXfont* f)
{
    setTextSize(1);
    setFont(f);
}

static void drawTextCentered(int16_t x, int16_t yBaseline, const String& s)
{
    int16_t x1=0,y1=0;
    uint16_t tw=0,th=0;
    String tmp=s;
    getTextBounds(tmp,&x1,&y1,&tw,&th);

    int16_t x0 = x - (int16_t)tw/2;
    writeTextReplaceBack(s,x0,yBaseline,SCBlack,SCWhite);
}

static void linePolar(float deg,int16_t r0,int16_t r1,int16_t thick)
{
    float rad=(90.0f-deg)*(PI/180.0f);
    float cs=cosf(rad);
    float sn=sinf(rad);

    int16_t x0=(int16_t)(CX+cs*(float)r0);
    int16_t y0=(int16_t)(CY-sn*(float)r0);
    int16_t x1=(int16_t)(CX+cs*(float)r1);
    int16_t y1=(int16_t)(CY-sn*(float)r1);

    for(int i=-thick;i<=thick;++i){
        dis->drawLine(x0+i,y0,x1+i,y1,SCBlack);
        dis->drawLine(x0,y0+i,x1,y1+i,SCBlack);
    }
    dUChange=true;
}

static void drawHand(float deg,int16_t len,int16_t tail,int16_t thick)
{
    float rad=(90.0f-deg)*(PI/180.0f);
    float cs=cosf(rad);
    float sn=sinf(rad);

    int16_t x1=(int16_t)(CX+cs*(float)len);
    int16_t y1=(int16_t)(CY-sn*(float)len);

    int16_t x0=(int16_t)(CX-cs*(float)tail);
    int16_t y0=(int16_t)(CY+sn*(float)tail);

    for(int i=-thick;i<=thick;++i){
        dis->drawLine(x0+i,y0,x1+i,y1,SCBlack);
        dis->drawLine(x0,y0+i,x1,y1+i,SCBlack);
    }
    dUChange=true;
}

// ------------------------------------------------------------
// Dial
// ------------------------------------------------------------
static void drawDialStatic()
{
    // Outer ring
    dis->drawCircle(CX,CY,R,SCBlack);
    dis->drawCircle(CX,CY,R-1,SCBlack);

    // Minute + hour ticks
    for(int i=0;i<60;i++){
        float deg=(float)i*6.0f;
        bool major=(i%5)==0;

        int16_t r0=R-(major?14:8);
        int16_t r1=R-2;
        linePolar(deg,r0,r1,major?1:0);
    }

    // Hour numerals
    setF(F_NUM);
    for(int h=1;h<=12;++h){
        float deg=(float)h*30.0f;
        float rad=(90.0f-deg)*(PI/180.0f);

        int16_t nr=R-22;
        int16_t nx=(int16_t)(CX+cosf(rad)*(float)nr);
        int16_t ny=(int16_t)(CY-sinf(rad)*(float)nr);

        drawTextCentered(nx,ny+6,String(h));
    }

    dUChange=true;
}

static void drawHands()
{
    int hh=(int)timeRTCLocal.Hour;
    int mm=(int)timeRTCLocal.Minute;

    float mDeg=(float)mm*6.0f;
    float hDeg=(float)(hh%12)*30.0f+(float)mm*0.5f;

    // hour
    drawHand(hDeg,(int16_t)(R*0.55f),(int16_t)(R*0.08f),2);

    // minute
    drawHand(mDeg,(int16_t)(R*0.82f),(int16_t)(R*0.10f),1);

    // center cap
    dis->fillCircle(CX,CY,4,SCBlack);
    dis->fillCircle(CX,CY,2,SCWhite);
    dis->fillCircle(CX,CY,1,SCBlack);

    dUChange=true;
}

// ------------------------------------------------------------
// watchface hooks
// ------------------------------------------------------------
static void renderFull()
{
    clearAll();
    drawDialStatic();
    drawHands();
}

static void drawTimeBeforeApply(){ renderFull(); }
static void drawTimeAfterApply(bool forceDraw){ (void)forceDraw; }
static void drawDay(){ renderFull(); }
static void drawMonth(){ renderFull(); }
static void showTimeFull(){ renderFull(); }
static void initWatchface(){ renderFull(); }
static void drawBattery(){}

static void manageInput(buttonState bt)
{
    if(bt!=None) resetSleepDelay(SLEEP_EVERY_MS);

    if(bt==Menu){
        generalSwitch(UiPlace::mainMenu);
        return;
    }

#if LONG_BACK_FULL_REFRESH
    if(bt==LongBack){
        dUChange=true;
        rM.updateCounter=FULL_DISPLAY_UPDATE_QUEUE;
        return;
    }
#endif
}

static void lpCoreScreenPrepareCustom(){}

const watchfaceDefOne analogPulseProDef={
    .drawTimeBeforeApply=drawTimeBeforeApply,
    .drawTimeAfterApply=drawTimeAfterApply,
    .drawDay=drawDay,
    .drawMonth=drawMonth,
    .showTimeFull=showTimeFull,
    .initWatchface=initWatchface,
    .drawBattery=drawBattery,
    .manageInput=manageInput,

    .watchfaceModules=false,
    .watchfaceModSquare={.size{.w=0,.h=0},.cord{.x=0,.y=0}},
    .someDrawingSquare={.size{.w=0,.h=0},.cord{.x=0,.y=0}},
    .isModuleEngaged=isModuleEngaged,
    .lpCoreScreenPrepareCustom=lpCoreScreenPrepareCustom
};

#endif