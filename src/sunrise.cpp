// Aufgaben 16.03.2023: in lauffähiges programm bringen, ohne display mit wifi connection.
// testen im Main alle daten ausgeben
// korrektur sunrise tomorow benötigt auch epochtime+1 Tag!!
// https://randomnerdtutorials.com/esp8266-nodemcu-date-time-ntp-client-server-arduino/
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "sunrise.h"

// #define PI 3.1415926536
int dayofyear = 0;
int dayofyearplusone = 0;
float currenttime;                     // this is the time in decimal hours i.e. 8.5
float startpv, stoppv, startpvtomorow; // time in decimal hours when PV starts, stopps
int vdayofweek;                        // day of week
int currentDay;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Week Days
String weekDays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// Month names
String months[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

//________________________________________________________________________________________calculate sunrise and sunset
// die folgenden Formeln wurden der Seite http://lexikon.astronomie.info/zeitgleichung/ entnommen

// subfunction to compute Sonnendeklination
float sonnendeklination(int T)
{
    // Deklination der Sonne in Radians
    // Formula 2008 by Arnold(at)Barmettler.com, fit to 20 years of average declinations (2008-2017)
    return 0.409526325277017 * sin(0.0169060504029192 * (T - 80.0856919827619));
}

// subfunction to compute Zeitdifferenz
// Dauer des halben Tagbogens in Stunden: Zeit von Sonnenaufgang (Hoehe h) bis zum hoechsten Stand im Sueden
float zeitdifferenz(float Deklination, float B)
{

    return 12.0 * acos((sin(-(50.0 / 60.0) * PI / 180.0) - sin(B) * sin(Deklination)) / (cos(B) * cos(Deklination))) / PI;
}

// subfunction to compute Zeitgleichung
float zeitgleichung(int T)
{
    return -0.170869921174742 * sin(0.0336997028793971 * T + 0.465419984181394) - 0.129890681040717 * sin(0.0178674832556871 * T - 0.167936777524864);
}

// subfunction to compute sunrise
float aufgang(int T, float B)
{
    float DK = sonnendeklination(T);
    return 12 - zeitdifferenz(DK, B) - zeitgleichung(T);
}

// subfunction to compute sunset
float untergang(int T, float B)
{
    float DK = sonnendeklination(T);
    return 12 + zeitdifferenz(DK, B) - zeitgleichung(T);
}

// subfunction to compute it all

/*void initsunset(int T) {
 float Laenge     = 10.98;
 float Breite     = 48.25;
 int   Zone       = 2; // Unterschied zu UTC-Zeit ( = 2 in der Sommerzeit, 1 in der Winterzeit )
 float B = Breite*PI/180.0; // geogr. Breite in Radians

 // Berechnung von Sonnenauf- und -Untergang
 float Aufgang = aufgang(T, B); // Sonnenaufgang bei 0 Grad Laenge
 float Untergang = untergang(T, B); // Sonnenuntergang bei 0 Grad Laenge

 Aufgang = Aufgang   - Laenge /15.0 + Zone; // Sonnenaufgang bei gesuchter Laenge und Zeitzone in Stunden
 Untergang = Untergang - Laenge /15.0 + Zone; // Sonnenuntergang bei gesuchter Laenge und Zeitzone in Stunden
 Serial.print("Tag: ");Serial.print(T);
 Serial.print(" Aufgang: ");Serial.print(Aufgang);Serial.print(" Untergang: ");Serial.println(Untergang);
}
*/
float computeAufgang(int T, float Laenge, float Breite)
{

    int Zone = 1;                  // Unterschied zu UTC-Zeit ( = 2 in der Sommerzeit, 1 in der Winterzeit )
    float B = Breite * PI / 180.0; // geogr. Breite in Radians

    // Berechnung von Sonnenaufgang
    float Aufgang = aufgang(T, B); // Sonnenaufgang bei 0 Grad Laenge

    Aufgang = Aufgang - Laenge / 15.0 + Zone; // Sonnenaufgang bei gesuchter Laenge und Zeitzone in Stunden
    return Aufgang;
}

float computeUntergang(int T, float Laenge, float Breite)
{

    int Zone = 1;                  // Unterschied zu UTC-Zeit ( = 2 in der Sommerzeit, 1 in der Winterzeit )
    float B = Breite * PI / 180.0; // geogr. Breite in Radians

    // Berechnung von Sonnenuntergang

    float Untergang = untergang(T, B); // Sonnenuntergang bei 0 Grad Laenge

    Untergang = Untergang - Laenge / 15.0 + Zone; // Sonnenuntergang bei gesuchter Laenge und Zeitzone in Stunden
    return Untergang;
}

//___________________________________________________________end calculate sunrise and sunset

//_____________________________________________________________Calculate Day of year
/*************************************************
 Program to calculate day of year from the date
 *
 * Enter date (MM/DD/YYYY): 12/30/2006
 * Day of year: 364
 *
 ************************************************/

int caldayofyear(int day, int mon, int year)
{
    int days_in_feb = 28;
    int doy; // day of year

    doy = day;

    // check for leap year
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
    {
        days_in_feb = 29;
    }

    switch (mon)
    {
    case 2:
        doy += 31;
        break;
    case 3:
        doy += 31 + days_in_feb;
        break;
    case 4:
        doy += 31 + days_in_feb + 31;
        break;
    case 5:
        doy += 31 + days_in_feb + 31 + 30;
        break;
    case 6:
        doy += 31 + days_in_feb + 31 + 30 + 31;
        break;
    case 7:
        doy += 31 + days_in_feb + 31 + 30 + 31 + 30;
        break;
    case 8:
        doy += 31 + days_in_feb + 31 + 30 + 31 + 30 + 31;
        break;
    case 9:
        doy += 31 + days_in_feb + 31 + 30 + 31 + 30 + 31 + 31;
        break;
    case 10:
        doy += 31 + days_in_feb + 31 + 30 + 31 + 30 + 31 + 31 + 30;
        break;
    case 11:
        doy += 31 + days_in_feb + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31;
        break;
    case 12:
        doy += 31 + days_in_feb + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30;
        break;
    }

    return doy; // return day of year
}
//_________________________________________________________________End calculate day of year
//____________________________________________________________________SetPV flag

// this function calculates start and end of the PV production
// based on sunrise and sunset and a lag factor
bool setPVstartflag(float lagmorning, float lagevening, float laenge, float breite)
{
    bool pvstartflag = false;
    timeClient.update();
    time_t epochTime = timeClient.getEpochTime();
    float currentHour = timeClient.getHours();     // get current hour
    float currentMinute = timeClient.getMinutes(); // get current menutes
    currentDay = timeClient.getDay();              // get current Day 0 Sunday
    long day;
    struct tm *ptm = gmtime((time_t *)&epochTime);
    int monthDay = ptm->tm_mday;           // get current day of the month
    int currentMonth = ptm->tm_mon + 1;    // get current month
    int currentYear = ptm->tm_year + 1900; // get current year

    dayofyear = caldayofyear(monthDay, currentMonth, currentYear);     // calculate current day of the year
    startpv = computeAufgang(dayofyear, laenge, breite) + lagmorning;  // statring time of pv
    stoppv = computeUntergang(dayofyear, laenge, breite) - lagevening; // stop time of PV
    currenttime = currentHour + (currentMinute / 60);                  // calculate current time in decimal hours

    // figure out wethere pv can produce power or not
    // flag pvstartflag is true at daytime when pv can procuce power
    if (currenttime >= startpv & currenttime <= stoppv)
    {
        pvstartflag = true;
    }
    else
        pvstartflag = false;

    return pvstartflag; // return pv start produce flag
}

// calculate UTC offset for EU DST Daylight saving time
int set_UTC_offset() {

    time_t epochTime = timeClient.getEpochTime();
   
    struct tm *ptm = gmtime((time_t *)&epochTime);
    int monthDay = ptm->tm_mday;           // get current day of the month
    int currentMonth = ptm->tm_mon + 1;    // get current month
    int currentYear = ptm->tm_year + 1900; // get current year
  
  bool isDST = false;
  //int currentMonth = ptm->tm_mon + 1;    // get current month
  int currentDay = timeClient.getDay();
  if (currentMonth > 3 && currentMonth < 10) {
    // DST is in effect from the last Sunday of March to the last Sunday of October
    int lastSunday = ((31 - (5 * currentMonth / 4 + 4)) % 7) + 1;
    if (currentMonth == 10) {
      if (currentDay < lastSunday) {
        isDST = true;
      }
    } else if (currentMonth == 3) {
      if (currentDay >= lastSunday) {
        isDST = true;
      }
    } else {
      isDST = true;
    }
  }
  if (isDST) {
    return 7200; // UTC+2 timezone (EU Central Summer Time)
  } else {
    return 3600; // UTC+1 timezone (EU Central Time)
  }
}





//________________________________________________START NTP Client
// Initialize a NTPClient to get time to calculate sunrise and sunset
void setuptimeClient()
{
    // Set offset time in seconds to adjust for your timezone, for example:
    timeClient.update();
    delay(200);
    timeClient.begin();
    delay(200);
    timeClient.setTimeOffset(set_UTC_offset());
}
//________________________________________________END NTP client

float factualtimeinhours()
{ // current time in decimal hours
    float currenthour;
    timeClient.update();
    // time_t epochTime = timeClient.getEpochTime();
    float currentHour = timeClient.getHours();        // get current hour
    float currentMinute = timeClient.getMinutes();    // get current menutes
    currenthour = currentHour + (currentMinute / 60); // calculate current time in decimal hours
    return currenthour;
}

void TimeData(struct TimeStruct *s, float lagmorning, float lagevening, float laenge, float breite)
{
    char TempCharMonth[3];
    char TempCharDay[3];
    timeClient.update();
    delay(100);
    time_t epochTime = timeClient.getEpochTime();
    time_t epochTimetomorow = timeClient.getEpochTime() + 86400;

    struct tm *ptm = gmtime((time_t *)&epochTime);
    int monthDay = ptm->tm_mday;                                      // get current day of the month
    int currentMonth = ptm->tm_mon + 1;                               // get current month
    int currentYear = ptm->tm_year + 1900;                            // get current year
    dayofyear = caldayofyear(monthDay, currentMonth, currentYear);    // calculate current day of the year
    startpv = computeAufgang(dayofyear, laenge, breite) + lagmorning; // statring time of pv
    s->sunrise_today = startpv;
    // date today
    sprintf(TempCharMonth, "%02d", currentMonth);
    sprintf(TempCharDay, "%02d", monthDay);
    String currentDate = String(currentYear) + TempCharMonth + TempCharDay;

    char *charArray = (char *)malloc((currentDate.length() + 1) * sizeof(char));
    strcpy(charArray, currentDate.c_str());
    s->date_today = charArray;

    // Sunset today with lag
    stoppv = computeUntergang(dayofyear, laenge, breite) - lagevening; // stop time of PV
    s->sunset_today = stoppv;

    // sunrise tomorow with lag
    // time_t epochTimetomorow = timeClient.getEpochTime()+86400;// epoch time 1 day later
    struct tm *ptmt = gmtime((time_t *)&epochTimetomorow);
    int monthDaytomorow = ptmt->tm_mday;                                                       // get current day of the month
    int currentMonthtomorow = ptmt->tm_mon + 1;                                                // get current month
    int currentYeartomorow = ptmt->tm_year + 1900;                                             // get current year
    dayofyearplusone = caldayofyear(monthDaytomorow, currentMonthtomorow, currentYeartomorow); // calculate current day of the year +1 day
    startpvtomorow = startpv = computeAufgang(dayofyearplusone, laenge, breite) + lagmorning;  // statring time of pv next day
    s->sunrise_tomorrow = startpvtomorow;

    // Date tomorow
    // time_t epochTime = timeClient.getEpochTime()+86400;

    sprintf(TempCharMonth, "%02d", currentMonthtomorow);
    sprintf(TempCharDay, "%02d", monthDaytomorow);

    String currentDatetomorow = String(currentYeartomorow) + TempCharMonth + TempCharDay;
    char *charArraytomorow = (char *)malloc((currentDatetomorow.length() + 1) * sizeof(char));
    strcpy(charArraytomorow, currentDatetomorow.c_str());
    s->date_tomorrow = charArraytomorow;

    // day of week as int
    s->day_of_week = timeClient.getDay();
}
