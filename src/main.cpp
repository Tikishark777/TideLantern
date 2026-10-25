/*
 * TideLantern — independent ESP32 dual-display tide and marine weather preview.
 * Project by John Christensen; implementation and documentation developed with AI assistance.
 * Pin-compatible with the 30-pin ESP-WROOM-32 / dual-GC9A01A SkyOrb hardware.
 * Tidal HIGH/LOW EVENTS: NOAA CO-OPS predictions. Graph is schematic interpolation.
 * Marine forecast: Open-Meteo. Place search: Nominatim (user initiated, rate limited).
 * Not for navigation or marine safety decisions.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>
#include "web_ui.h"

// EXACT same physical wiring as SkyOrb Community Edition.
constexpr int PIN_SCK=18, PIN_MOSI=23;
Adafruit_GC9A01A screenTide(5,2,4);    // RIGHT: CS/DC/RESET
Adafruit_GC9A01A screenClock(16,17,21); // LEFT: CS/DC/RESET
constexpr uint16_t NAVY=0x002B, BLACK=0x0000, BLUE=0x0B71,
                   CYAN=0x75BC, WHITE=0xFFFF, AMBER=0xFE69,
                   MUTED=0x7C91, GREEN=0x7E56;

WebServer server(80);
DNSServer dns;
Preferences prefs;
bool apMode=false, restartRequested=false;
uint32_t restartAt=0;
// On an established home Wi-Fi connection, show the dashboard IP for 30
// seconds after boot. Keep the setup hotspot address visible until setup ends.
uint32_t dashboardConnectedAt=0;
constexpr uint32_t DASHBOARD_INTRO_MS=30000UL;

struct Config {
  String site="Seattle, WA";
  String ssid="",password="",station="9447130";
  double lat=47.6026,lon=-122.3393;
  uint8_t view=2,units=0,dateStyle=0,tzStyle=0;
  bool clock24=false,seconds=false,leadingZero=false;
} cfg;
struct TideEvent {time_t t=0;float height=0;char kind='?';};
TideEvent tides[32];
int tideCount=0;
String tideError="Waiting for NOAA predictions", weatherError="Waiting for marine forecasts";
String tzName="Etc/UTC",tzAbbreviation="UTC";
int tzOffset=0;
float windKn=NAN,gustKn=NAN,windDegrees=NAN,waveHeight=NAN,seaTemp=NAN,airTemp=NAN;
uint32_t lastTideSuccess=0,lastWeatherSuccess=0,lastTideAttempt=0,lastWeatherAttempt=0;
bool tidePending=true,weatherPending=true;
String lastSearchQuery="",lastSearchResult="";
uint32_t lastSearchMillis=0;

int clamp(int value,int lo,int hi){return value<lo?lo:value>hi?hi:value;}
String encodeURIComponent(const String &s){
  String out;out.reserve(s.length()*3);
  const char *hex="0123456789ABCDEF";
  for(size_t i=0;i<s.length();i++){
    uint8_t c=(uint8_t)s[i];
    if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~')out+=(char)c;
    else{out+='%';out+=hex[c>>4];out+=hex[c&15];}
  }
  return out;
}
String jsonError(const String &message){JsonDocument d;d["error"]=message;String s;serializeJson(d,s);return s;}
void sendError(int code,const String &message){server.send(code,"application/json",jsonError(message));}
void drawCenter(Adafruit_GFX &d,const String &s,int y,int font,uint16_t color){
  d.setTextSize(font);d.setTextColor(color);int16_t x1,y1;uint16_t w,h;
  d.getTextBounds(s,0,0,&x1,&y1,&w,&h);
  d.setCursor(max(0,(240-(int)w)/2),y);d.print(s);
}
String compact(const String &s,size_t length){return s.length()<=length?s:s.substring(0,length);}

void loadConfig(){
  prefs.begin("tidelantern",false);
  cfg.site=prefs.getString("site",cfg.site);
  cfg.ssid=prefs.getString("ssid","");
  cfg.password=prefs.getString("password","");
  cfg.station=prefs.getString("station",cfg.station);
  cfg.lat=prefs.getDouble("lat",cfg.lat);
  cfg.lon=prefs.getDouble("lon",cfg.lon);
  cfg.view=clamp(prefs.getUChar("view",2),0,2);
  cfg.units=clamp(prefs.getUChar("units",0),0,1);
  cfg.dateStyle=clamp(prefs.getUChar("date",0),0,3);
  cfg.tzStyle=clamp(prefs.getUChar("tzstyle",0),0,2);
  cfg.clock24=prefs.getBool("clock24",false);
  cfg.seconds=prefs.getBool("seconds",false);
  cfg.leadingZero=prefs.getBool("leadzero",false);
}
void setupNetwork(){
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  if(cfg.ssid.length()){
    WiFi.mode(WIFI_STA);WiFi.begin(cfg.ssid.c_str(),cfg.password.c_str());
    Serial.print("Connecting to Wi-Fi");
    uint32_t start=millis();
    while(WiFi.status()!=WL_CONNECTED&&millis()-start<17000){delay(250);Serial.print('.');}
    Serial.println();
  }
  if(WiFi.status()==WL_CONNECTED){
    apMode=false;
    dashboardConnectedAt=millis();
    Serial.print("Dashboard: http://");Serial.println(WiFi.localIP());
    configTime(0,0,"pool.ntp.org","time.google.com","time.cloudflare.com");
    if(MDNS.begin("tidelantern")){
      MDNS.addService("http","tcp",80);
      Serial.println("Dashboard shortcut: http://tidelantern.local/");
    }
  }else{
    WiFi.mode(WIFI_AP_STA);
    apMode=true;
    WiFi.softAP("TideLantern-Setup","Tides2026!");
    delay(150);
    dns.start(53,"*",WiFi.softAPIP());
    Serial.println("Setup hotspot: TideLantern-Setup / Tides2026!");
    Serial.print("Setup dashboard: http://");Serial.println(WiFi.softAPIP());
  }
}
// Remote data is public, read-only and carries no credentials. TLS trust is not
// pinned on this preview firmware; do not use data for navigation or safety.
bool getPublicJson(const String &url,JsonDocument &doc,String &error){
  if(WiFi.status()!=WL_CONNECTED){error="Wi-Fi not connected";return false;}
  WiFiClientSecure client;client.setInsecure();
  HTTPClient http;http.setTimeout(12000);
  if(!http.begin(client,url)){error="Could not start HTTPS request";return false;}
  http.addHeader("User-Agent","TideLantern/0.1 (+https://github.com/Tikishark777/TideLantern)");
  int code=http.GET();
  if(code!=200){error="HTTP "+String(code);http.end();return false;}
  String payload=http.getString();http.end();
  if(payload.length()>45000){error="Unexpectedly large response";return false;}
  DeserializationError e=deserializeJson(doc,payload);
  if(e){error=String("Invalid JSON: ")+e.c_str();return false;}
  return true;
}
time_t readUTC(const char *str){
  if(!str||strlen(str)<16)return 0;
  int y=0,mo=0,d=0,h=0,mi=0;
  if(sscanf(str,"%d-%d-%d %d:%d",&y,&mo,&d,&h,&mi)!=5)return 0;
  if(y<2024||mo<1||mo>12||d<1||d>31||h>23||mi>59)return 0;
  // Convert a UTC calendar date to Unix time without non-portable timegm().
  // Civil-date arithmetic handles leap years and does not depend on the
  // device's configured local timezone or daylight-saving settings.
  const int year=y-(mo<=2);
  const int era=(year>=0?year:year-399)/400;
  const unsigned yearOfEra=(unsigned)(year-era*400);
  const unsigned monthFromMarch=(unsigned)(mo+(mo>2?-3:9));
  const unsigned dayOfYear=(153*monthFromMarch+2)/5+(unsigned)d-1;
  const unsigned dayOfEra=yearOfEra*365+yearOfEra/4-yearOfEra/100+dayOfYear;
  const int64_t days=(int64_t)era*146097+(int64_t)dayOfEra-719468;
  return (time_t)(days*86400LL+(int64_t)h*3600LL+(int64_t)mi*60LL);
}
String dateUTC(time_t utc){char b[16];struct tm t{};gmtime_r(&utc,&t);strftime(b,sizeof(b),"%Y%m%d",&t);return String(b);}
String timeLocal(time_t utc,bool withDate=false){
  struct tm t{};time_t local=utc+tzOffset;gmtime_r(&local,&t);
  char b[32];if(withDate)strftime(b,sizeof(b),"%m/%d %H:%M",&t);
  else if(cfg.clock24)strftime(b,sizeof(b),"%H:%M",&t);
  else{int hour=t.tm_hour%12;if(hour==0)hour=12;snprintf(b,sizeof(b),"%d:%02d %s",hour,t.tm_min,t.tm_hour>=12?"PM":"AM");}
  return String(b);
}
void refreshTides(){
  lastTideAttempt=millis();tidePending=false;
  if(!cfg.station.length()){tideCount=0;tideError="No NOAA tide station selected";return;}
  if(cfg.station.length()>10){tideCount=0;tideError="Invalid NOAA station ID";return;}
  time_t now=time(nullptr);if(now<1700000000){tideError="Waiting for clock sync";return;}
  String start=dateUTC(now-86400),end=dateUTC(now+4*86400);
  String url="https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?product=predictions"
             "&application=TideLantern&format=json&datum=MLLW&time_zone=gmt&interval=hilo";
  url+="&station="+cfg.station+"&begin_date="+start+"&end_date="+end;
  url+=(cfg.units==1)?"&units=metric":"&units=english";
  JsonDocument doc;String error;
  if(!getPublicJson(url,doc,error)){tideError=error;return;}
  JsonArray items=doc["predictions"].as<JsonArray>();
  if(items.isNull()){
    tideCount=0;
    tideError=doc["error"]["message"].as<String>();
    if(!tideError.length())tideError="No predictions for this NOAA station";
    return;
  }
  TideEvent fresh[32];int total=0;
  for(JsonObject x:items){
    if(total>=32)break;
    time_t utc=readUTC(x["t"]|"");
    if(!utc)continue;
    const char *type=x["type"]|"";
    float h=atof(x["v"] | "0"); // NOAA returns prediction heights as JSON strings
    fresh[total].t=utc;fresh[total].height=h;fresh[total].kind=type[0];total++;
  }
  if(total<2){tideError="Not enough NOAA high/low events";tideCount=0;return;}
  tideCount=total;memcpy(tides,fresh,total*sizeof(TideEvent));
  tideError="";lastTideSuccess=millis();
  Serial.printf("NOAA tides: %d events for %s\n",tideCount,cfg.station.c_str());
}
void updateTZ(const JsonDocument &doc){
  String timezone=doc["timezone"].as<String>();
  if(timezone.length())tzName=timezone;
  tzOffset=doc["utc_offset_seconds"].as<int>();
  String zone=doc["timezone_abbreviation"].as<String>();
  tzAbbreviation=zone;
  if(tzName=="America/Los_Angeles")tzAbbreviation=tzOffset==-25200?"PDT":"PST";
  else if(tzName=="America/Denver")tzAbbreviation=tzOffset==-21600?"MDT":"MST";
  else if(tzName=="America/Chicago")tzAbbreviation=tzOffset==-18000?"CDT":"CST";
  else if(tzName=="America/New_York")tzAbbreviation=tzOffset==-14400?"EDT":"EST";
  else if(tzName=="Europe/London")tzAbbreviation=tzOffset==3600?"BST":"GMT";
  else if(tzName=="Europe/Paris"||tzName=="Europe/Berlin"||tzName=="Europe/Rome")tzAbbreviation=tzOffset==7200?"CEST":"CET";
  else if(tzName=="Asia/Tokyo")tzAbbreviation="JST";
  else if(tzName=="Asia/Kolkata")tzAbbreviation="IST";
  else if(tzName=="Australia/Sydney")tzAbbreviation=tzOffset==39600?"AEDT":"AEST";
  else if(tzName=="Pacific/Auckland")tzAbbreviation=tzOffset==46800?"NZDT":"NZST";
  else if(tzName=="Etc/UTC")tzAbbreviation="UTC";
  else tzAbbreviation=""; // Use explicit UTC offset for ambiguous / unfamiliar zones.
}
void refreshWeather(){
  lastWeatherAttempt=millis();weatherPending=false;
  const String lat=String(cfg.lat,5),lon=String(cfg.lon,5);
  String weather="https://api.open-meteo.com/v1/forecast?latitude="+lat+"&longitude="+lon+
    "&current=wind_speed_10m,wind_direction_10m,wind_gusts_10m,temperature_2m"
    "&wind_speed_unit=kn&timezone=auto&temperature_unit="+(cfg.units==1?"celsius":"fahrenheit");
  JsonDocument doc;String error;
  bool weatherOkay=false,marineOkay=false;
  if(getPublicJson(weather,doc,error)){
    JsonObject current=doc["current"].as<JsonObject>();
    if(!current.isNull()){
      windKn=current["wind_speed_10m"].isNull()?NAN:current["wind_speed_10m"].as<float>();
      gustKn=current["wind_gusts_10m"].isNull()?NAN:current["wind_gusts_10m"].as<float>();
      windDegrees=current["wind_direction_10m"].isNull()?NAN:current["wind_direction_10m"].as<float>();
      airTemp=current["temperature_2m"].isNull()?NAN:current["temperature_2m"].as<float>();
      updateTZ(doc);weatherOkay=true;
    }
  }
  doc.clear();
  String marine="https://marine-api.open-meteo.com/v1/marine?latitude="+lat+"&longitude="+lon+
    "&current=wave_height,sea_surface_temperature&cell_selection=sea&timezone=auto"
    "&length_unit="+(cfg.units==1?"metric":"imperial")+
    "&temperature_unit="+(cfg.units==1?"celsius":"fahrenheit");
  if(getPublicJson(marine,doc,error)){
    JsonObject current=doc["current"].as<JsonObject>();
    if(!current.isNull()){
      waveHeight=current["wave_height"].isNull()?NAN:current["wave_height"].as<float>();
      seaTemp=current["sea_surface_temperature"].isNull()?NAN:current["sea_surface_temperature"].as<float>();
      if(!weatherOkay)updateTZ(doc);
      marineOkay=true;
    }
  }
  if(weatherOkay||marineOkay){lastWeatherSuccess=millis();weatherError=String(!marineOkay?"Marine model unavailable; ":"")+(!weatherOkay?"Wind forecast unavailable":"");}
  else weatherError=error.length()?error:"Forecast unavailable";
  Serial.printf("Forecast: wind %.1f kn, wave %.1f\n",windKn,waveHeight);
}
String tzOffsetLabel(){
  int absolute=abs(tzOffset),hh=absolute/3600,mm=(absolute%3600)/60;
  char b[16];snprintf(b,sizeof(b),"UTC%c%02d:%02d",tzOffset<0?'-':'+',hh,mm);
  return String(b);
}
String displayZone(){
  if(cfg.tzStyle==2)return "";
  if(cfg.tzStyle==0&&tzAbbreviation.length()&&tzAbbreviation.length()<=5)return tzAbbreviation;
  return tzOffsetLabel();
}
void renderClock(bool force=false){
  // Full-screen redraw happens only on first render or when settings change.
  // With optional seconds enabled, refresh just the main time rectangle.
  static String lastTime="",lastDate="",lastConfig="";
  time_t now=time(nullptr);
  // Hide the address after the boot intro on an already connected unit.
  // Always show it in hotspot setup mode so first-time users can connect.
  // Unsigned subtraction remains correct if millis() wraps around.
  const bool showDashboardAddress=apMode ||
    (millis()-dashboardConnectedAt < DASHBOARD_INTRO_MS);
  String configKey=String(cfg.clock24)+"/"+String(cfg.leadingZero)+"/"+
    String(cfg.dateStyle)+"/"+String(cfg.tzStyle)+"/"+
    String(tzOffset)+"/"+displayZone()+"/"+String(apMode)+"/"+String(showDashboardAddress);
  bool full=force||configKey!=lastConfig;
  if(full){
    lastConfig=configKey;lastTime="";lastDate="";
    screenClock.fillScreen(BLACK);
    screenClock.drawCircle(120,120,116,BLUE);
    drawCenter(screenClock,"TIDELANTERN",38,1,CYAN);
    // Boot-only help on home Wi-Fi. Keep the setup address until configured.
    if(showDashboardAddress){
      drawCenter(screenClock,apMode?"WIFI SETUP":"SETTINGS PAGE",205,1,CYAN);
      String ip=apMode?"192.168.4.1":WiFi.localIP().toString();
      drawCenter(screenClock,ip,219,1,WHITE);
    }
  }
  if(now<1700000000){
    if(full)drawCenter(screenClock,"SYNCING TIME",102,2,WHITE);
    return;
  }
  time_t local=now+tzOffset;struct tm t{};gmtime_r(&local,&t);
  int hour=t.tm_hour;
  if(!cfg.clock24){hour%=12;if(hour==0)hour=12;}
  char buf[36];
  if(cfg.clock24||cfg.leadingZero)snprintf(buf,sizeof(buf),cfg.seconds?"%02d:%02d:%02d":"%02d:%02d",hour,t.tm_min,t.tm_sec);
  else snprintf(buf,sizeof(buf),cfg.seconds?"%d:%02d:%02d":"%d:%02d",hour,t.tm_min,t.tm_sec);
  String timeText(buf);
  if(timeText!=lastTime){
    screenClock.fillRect(15,71,210,39,BLACK);
    drawCenter(screenClock,timeText,74,timeText.length()>6?3:4,WHITE);
    lastTime=timeText;
  }
  String zone=displayZone();
  String period=cfg.clock24?"":t.tm_hour>=12?"PM":"AM";
  String subtitle=period+(zone.length()?(period.length()?"  ":"")+zone:"");
  String dateKey=String(t.tm_yday)+"/"+String(t.tm_year)+"/"+subtitle;
  if(dateKey!=lastDate){
    screenClock.fillRect(12,114,216,86,BLACK);
    drawCenter(screenClock,subtitle,119,subtitle.length()>13?1:2,WHITE);
    static const char* days[]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    static const char* months[]={"January","February","March","April","May","June","July","August","September","October","November","December"};
    drawCenter(screenClock,days[t.tm_wday],155,2,AMBER);
    if(cfg.dateStyle==0)snprintf(buf,sizeof(buf),"%s %d, %04d",months[t.tm_mon],t.tm_mday,t.tm_year+1900);
    else if(cfg.dateStyle==1)snprintf(buf,sizeof(buf),"%02d/%02d/%04d",t.tm_mon+1,t.tm_mday,t.tm_year+1900);
    else if(cfg.dateStyle==2)snprintf(buf,sizeof(buf),"%02d/%02d/%04d",t.tm_mday,t.tm_mon+1,t.tm_year+1900);
    else snprintf(buf,sizeof(buf),"%04d-%02d-%02d",t.tm_year+1900,t.tm_mon+1,t.tm_mday);
    drawCenter(screenClock,buf,181,strlen(buf)>18?1:2,AMBER);
    lastDate=dateKey;
  }
}
float tideHeightAt(time_t target){
  for(int i=0;i<tideCount-1;i++){
    if(tides[i].t<=target&&target<=tides[i+1].t){
      float f=float(target-tides[i].t)/float(tides[i+1].t-tides[i].t);
      return tides[i].height+(tides[i+1].height-tides[i].height)*(1.0f-cosf(f*PI))*0.5f;
    }
  }
  return NAN;
}
int nextTideIndex(time_t now){for(int i=0;i<tideCount;i++)if(tides[i].t>now)return i;return -1;}
void drawTides(){
  Adafruit_GC9A01A &d=screenTide;
  d.fillScreen(BLACK);d.drawCircle(120,120,115,BLUE);
  drawCenter(d,"TIDE PREDICTIONS",25,1,CYAN);
  drawCenter(d,compact(cfg.site,20),42,1,WHITE);
  time_t now=time(nullptr);
  if(tideCount<2){
    if(WiFi.status()!=WL_CONNECTED){
      drawCenter(d,"WIFI SETUP",90,2,AMBER);
      drawCenter(d,"TideLantern-Setup",124,1,WHITE);
      drawCenter(d,"192.168.4.1",160,2,CYAN);
    }else{
      drawCenter(d,"NO NOAA TIDES",97,2,AMBER);
      drawCenter(d,"AVAILABLE",121,2,AMBER);
      drawCenter(d,"Marine view may work",165,1,CYAN);
    }
    return;
  }
  float low=100000,high=-100000;
  for(int i=0;i<tideCount;i++){low=min(low,tides[i].height);high=max(high,tides[i].height);}
  if(high-low<0.5f)high=low+0.5f;
  const int x0=23,x1=217,y0=75,y1=150;
  time_t start=now-6*3600,end=now+18*3600;
  auto mapY=[&](float v)->int{return y1-int((v-low)/(high-low)*float(y1-y0));};
  d.drawFastHLine(x0,111,x1-x0,BLUE);
  bool pen=false;int previousX=0,previousY=0;
  for(int x=x0;x<=x1;x++){
    time_t sample=start+(time_t)((double)(end-start)*(x-x0)/(x1-x0));
    float level=tideHeightAt(sample);
    if(!isnan(level)){
      int y=clamp(mapY(level),y0,y1);
      if(pen)d.drawLine(previousX,previousY,x,y,CYAN);
      previousX=x;previousY=y;pen=true;
    }else pen=false;
  }
  const int nowX=x0+(x1-x0)/4;
  for(int y=y0;y<=y1;y+=6)d.drawPixel(nowX,y,AMBER);
  drawCenter(d,"-6h       NOW       +18h",160,1,MUTED);
  int ix=nextTideIndex(now);
  if(ix>=0){
    String s=String(tides[ix].kind=='H'?"HIGH ":"LOW ")+timeLocal(tides[ix].t);
    drawCenter(d,s,180,s.length()>18?1:2,WHITE);
    drawCenter(d,String(tides[ix].height,1)+(cfg.units==1?" m":" ft")+"  MLLW",205,1,AMBER);
  }else drawCenter(d,"Next event pending",189,1,AMBER);
}
void drawMarine(){
  Adafruit_GC9A01A &d=screenTide;
  d.fillScreen(BLACK);d.drawCircle(120,120,115,BLUE);
  drawCenter(d,"MARINE FORECAST",24,1,CYAN);
  drawCenter(d,compact(cfg.site,20),40,1,WHITE);
  drawCenter(d,"WIND",64,1,MUTED);
  if(isnan(windKn))drawCenter(d,"-- kn",83,3,WHITE);
  else drawCenter(d,String(windKn,0)+" kn",81,3,WHITE);
  String dir=isnan(windDegrees)?"--":String(windDegrees,0)+" deg";
  if(!isnan(gustKn))dir+="  G "+String(gustKn,0);
  drawCenter(d,dir,117,1,AMBER);
  d.drawFastHLine(40,135,160,BLUE);
  String wave=isnan(waveHeight)?"WAVES --":String("WAVES ")+String(waveHeight,1)+(cfg.units==1?" m":" ft");
  drawCenter(d,wave,148,wave.length()>13?1:2,CYAN);
  String water=isnan(seaTemp)?"SEA --":String("SEA ")+String(seaTemp,0)+(cfg.units==1?" C":" F");
  drawCenter(d,water,176,2,WHITE);
}
void renderRight(bool force=false){
  static uint32_t lastDraw=0;
  static int lastMode=-1;
  int mode=cfg.view==2?int((millis()/20000UL)%2):int(cfg.view);
  if(!force&&lastMode==mode&&millis()-lastDraw<12000UL)return;
  lastMode=mode;lastDraw=millis();
  if(mode==0)drawTides();else drawMarine();
}
void respondState(){
  JsonDocument d;
  d["siteName"]=cfg.site;d["lat"]=cfg.lat;d["lon"]=cfg.lon;
  d["stationId"]=cfg.station;d["view"]=cfg.view;d["units"]=cfg.units;
  d["clock24"]=cfg.clock24;d["dateStyle"]=cfg.dateStyle;d["tzStyle"]=cfg.tzStyle;
  d["seconds"]=cfg.seconds;d["leadingZero"]=cfg.leadingZero;
  d["timezone"]=tzName;d["zoneLabel"]=displayZone();
  d["windKn"]=nullptr;
  if(!isnan(windKn))d["windKn"]=windKn;
  d["wave"]=nullptr;if(!isnan(waveHeight))d["wave"]=waveHeight;
  d["water"]=nullptr;if(!isnan(seaTemp))d["water"]=seaTemp;
  time_t now=time(nullptr);
  int next=nextTideIndex(now);
  d["nextEvent"]=next<0?String("—"):String(tides[next].kind=='H'?"High ":"Low ")+timeLocal(tides[next].t);
  String s="";
  if(WiFi.status()!=WL_CONNECTED)s+="Not connected to Wi-Fi. ";
  if(tideError.length())s+="Tides: "+tideError+". ";
  else if(lastTideSuccess)s+="NOAA high/low predictions available. ";
  if(weatherError.length())s+="Forecast: "+weatherError+". ";
  else if(lastWeatherSuccess)s+="Marine forecast available (modeled). ";
  if(lastTideSuccess)s+="Tides checked "+String((millis()-lastTideSuccess)/60000)+" min ago. ";
  if(lastWeatherSuccess)s+="Weather checked "+String((millis()-lastWeatherSuccess)/60000)+" min ago.";
  d["status"]=s;
  JsonArray arr=d["events"].to<JsonArray>();
  for(int i=0;i<tideCount;i++){
    JsonObject e=arr.add<JsonObject>();e["epoch"]=(int64_t)tides[i].t;
    e["height"]=tides[i].height;e["type"]=String(tides[i].kind);
  }
  String output;serializeJson(d,output);server.send(200,"application/json",output);
}
void applySettings(){
  bool placeChanged=false,marineChanged=false;
  if(server.hasArg("lat")||server.hasArg("lon")){
    if(!server.hasArg("lat")||!server.hasArg("lon"))return sendError(400,"Both latitude and longitude required");
    double lat=server.arg("lat").toDouble(),lon=server.arg("lon").toDouble();
    if(!isfinite(lat)||!isfinite(lon)||lat < -90||lat>90||lon < -180||lon>180)return sendError(400,"Coordinates outside valid range");
    if(lat!=cfg.lat||lon!=cfg.lon){cfg.lat=lat;cfg.lon=lon;prefs.putDouble("lat",lat);prefs.putDouble("lon",lon);placeChanged=true;}
  }
  if(server.hasArg("siteName")){
    String s=server.arg("siteName");s.trim();if(!s.length())return sendError(400,"Location name cannot be blank");
    cfg.site=compact(s,64);prefs.putString("site",cfg.site);
  }
  if(server.hasArg("station")){
    String id=server.arg("station");id.trim();
    if(id.length()>10)return sendError(400,"NOAA station ID too long");
    for(size_t i=0;i<id.length();i++)if(!isalnum((uint8_t)id[i]))return sendError(400,"Invalid NOAA station ID");
    if(cfg.station!=id){cfg.station=id;prefs.putString("station",id);placeChanged=true;}
  }
  if(server.hasArg("view")){cfg.view=clamp(server.arg("view").toInt(),0,2);prefs.putUChar("view",cfg.view);}
  if(server.hasArg("units")){
    uint8_t unit=clamp(server.arg("units").toInt(),0,1);
    if(unit!=cfg.units){cfg.units=unit;prefs.putUChar("units",unit);marineChanged=true;}
  }
  if(server.hasArg("clock24")){cfg.clock24=server.arg("clock24")=="1";prefs.putBool("clock24",cfg.clock24);}
  if(server.hasArg("dateStyle")){cfg.dateStyle=clamp(server.arg("dateStyle").toInt(),0,3);prefs.putUChar("date",cfg.dateStyle);}
  if(server.hasArg("tzStyle")){cfg.tzStyle=clamp(server.arg("tzStyle").toInt(),0,2);prefs.putUChar("tzstyle",cfg.tzStyle);}
  if(server.hasArg("seconds")){cfg.seconds=server.arg("seconds")=="1";prefs.putBool("seconds",cfg.seconds);}
  if(server.hasArg("leadingZero")){cfg.leadingZero=server.arg("leadingZero")=="1";prefs.putBool("leadzero",cfg.leadingZero);}
  if(placeChanged){
    tidePending=true;weatherPending=true;
    tideCount=0;windKn=NAN;waveHeight=NAN;seaTemp=NAN;
    lastTideAttempt=0;lastWeatherAttempt=0;
    tideError="Awaiting new location predictions";
  }else if(marineChanged){tidePending=true;weatherPending=true;tideCount=0;lastTideAttempt=0;lastWeatherAttempt=0;}
  renderClock(true);renderRight(true);
  server.send(200,"application/json","{\"message\":\"Saved! Displays update immediately; data refreshes shortly.\"}");
}
void handlePlaceSearch(){
  if(WiFi.status()!=WL_CONNECTED)return sendError(503,"Connect TideLantern to the internet first");
  String q=server.arg("q");q.trim();
  if(q.length()<3||q.length()>100)return sendError(400,"Search length must be 3–100 characters");
  uint32_t now=millis();
  if(q==lastSearchQuery&&lastSearchResult.length()&&now-lastSearchMillis<300000UL){server.send(200,"application/json",lastSearchResult);return;}
  if(lastSearchMillis&&now-lastSearchMillis<1500UL)return sendError(429,"Please wait before searching again");
  lastSearchMillis=now;
  String url="https://nominatim.openstreetmap.org/search?format=jsonv2&addressdetails=0&limit=5&q="+encodeURIComponent(q);
  WiFiClientSecure client;client.setInsecure();HTTPClient http;http.setTimeout(12000);
  if(!http.begin(client,url))return sendError(502,"Search connection failed");
  http.addHeader("User-Agent","TideLantern/0.1 (GitHub: Tikishark777/TideLantern; interactive place search)");
  http.addHeader("Accept","application/json");
  int status=http.GET();
  if(status!=200){http.end();return sendError(502,"Place search failed (HTTP "+String(status)+")");}
  String body=http.getString();http.end();
  if(body.length()>28000)return sendError(502,"Place search returned too much data");
  JsonDocument document;if(deserializeJson(document,body)||!document.is<JsonArray>())return sendError(502,"Place search response was invalid");
  lastSearchQuery=q;lastSearchResult=body;
  server.send(200,"application/json",body);
}
// Return nearby Wi-Fi networks for a friendlier phone-based setup portal.
// The ESP32 supports scanning in AP+STA mode while the setup hotspot stays on.
void handleWifiScan(){
  int found=WiFi.scanNetworks(false,false);
  if(found<0)return sendError(503,"Wi-Fi scan failed. Try Scan again or type the name manually.");
  JsonDocument d;
  JsonArray networks=d["networks"].to<JsonArray>();
  for(int i=0;i<found&&networks.size()<40;i++){
    String name=WiFi.SSID(i);
    if(!name.length())continue;
    bool duplicate=false;
    for(JsonObject item:networks){if(item["ssid"].as<String>()==name){duplicate=true;break;}}
    if(duplicate)continue;
    JsonObject item=networks.add<JsonObject>();
    item["ssid"]=name;
    item["rssi"]=WiFi.RSSI(i);
    item["secure"]=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  String response;serializeJson(d,response);
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json",response);
}
void handleNetworkStatus(){
  JsonDocument d;
  bool connected=WiFi.status()==WL_CONNECTED;
  d["connected"]=connected;
  d["setupMode"]=apMode;
  d["ip"]=connected?WiFi.localIP().toString():"192.168.4.1";
  d["ssid"]=connected?WiFi.SSID():"";
  d["hostname"]="tidelantern.local";
  String response;serializeJson(d,response);
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json",response);
}
void handleWifi(){
  String ssid=server.arg("ssid"),pass=server.arg("password");
  if(!ssid.length()||ssid.length()>32||
     (pass.length()&& (pass.length()<8||pass.length()>64)))
    return sendError(400,"Enter a Wi-Fi name and a valid password (8–64 characters if secured)");
  prefs.putString("ssid",ssid);prefs.putString("password",pass);
  server.send(200,"application/json","{\"message\":\"Wi-Fi saved. Reconnect your phone to home Wi-Fi, then use the IP shown on TideLantern's clock.\"}");
  // Leave time for the phone to show the instructions before rebooting.
  restartRequested=true;restartAt=millis()+8500;
}
void registerRoutes(){
  server.on("/",HTTP_GET,[](){server.send_P(200,"text/html; charset=utf-8",WEB_UI);});
  server.on("/setup",HTTP_GET,[](){server.sendHeader("Location","/",true);server.send(302,"text/plain","");});
  server.on("/api/state",HTTP_GET,respondState);
  server.on("/api/settings",HTTP_POST,applySettings);
  server.on("/api/search",HTTP_GET,handlePlaceSearch);
  server.on("/api/wifi",HTTP_POST,handleWifi);
  server.on("/api/wifi/scan",HTTP_GET,handleWifiScan);
  server.on("/api/network",HTTP_GET,handleNetworkStatus);
  server.on("/generate_204",HTTP_GET,[](){server.sendHeader("Location","/",true);server.send(302,"text/plain","");});
  server.on("/hotspot-detect.html",HTTP_GET,[](){server.sendHeader("Location","/",true);server.send(302,"text/plain","");});
  server.onNotFound([](){if(apMode){server.sendHeader("Location","/",true);server.send(302,"text/plain","");}else server.send(404,"text/plain","Not found");});
  server.begin();
}
void setup(){
  Serial.begin(115200);delay(300);Serial.println("\nTideLantern v0.1 preview");
  SPI.begin(PIN_SCK,-1,PIN_MOSI,-1);
  screenTide.begin();screenClock.begin();
  screenTide.fillScreen(BLACK);screenClock.fillScreen(BLACK);
  drawCenter(screenTide,"TIDELANTERN",96,2,CYAN);
  drawCenter(screenClock,"STARTING...",107,2,WHITE);
  loadConfig();setupNetwork();registerRoutes();
  renderClock(true);renderRight(true);
}
void loop(){
  server.handleClient();if(apMode)dns.processNextRequest();
  if(restartRequested&&millis()>restartAt){delay(150);ESP.restart();}
  renderClock();renderRight();
  if(WiFi.status()!=WL_CONNECTED){delay(15);return;}
  time_t now=time(nullptr);
  if(now>1700000000&&(tidePending||millis()-lastTideAttempt>21600000UL)){
    // Retry transient NOAA failures once every 15 minutes; success every 6 hours.
    if(!lastTideAttempt||millis()-lastTideAttempt>(tideError.length()?900000UL:21600000UL)){
      refreshTides();renderRight(true);return;
    }
  }
  if(weatherPending||!lastWeatherAttempt||millis()-lastWeatherAttempt>(weatherError.length()?900000UL:1200000UL)){
    if(!lastWeatherAttempt||millis()-lastWeatherAttempt>30000UL){refreshWeather();renderClock(true);renderRight(true);return;}
  }
  delay(25);
}
