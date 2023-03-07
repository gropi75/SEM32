

#include <NTPClient.h>
#include <WiFiUdp.h>


#define PI 3.1415926536
int dayofyear=0;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

//Week Days
String weekDays[7]={"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

//Month names
String months[12]={"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};


//________________________________________________________________________________________calculate sunrise and sunset
// die folgenden Formeln wurden der Seite http://lexikon.astronomie.info/zeitgleichung/ entnommen


// subfunction to compute Sonnendeklination
float sonnendeklination(int T) {
  // Deklination der Sonne in Radians
  // Formula 2008 by Arnold(at)Barmettler.com, fit to 20 years of average declinations (2008-2017)
 return 0.409526325277017*sin(0.0169060504029192*(T-80.0856919827619));
}

// subfunction to compute Zeitdifferenz
// Dauer des halben Tagbogens in Stunden: Zeit von Sonnenaufgang (Hoehe h) bis zum hoechsten Stand im Sueden
float zeitdifferenz(float Deklination, float B) {
  
    return 12.0*acos((sin(-(50.0/60.0)*PI/180.0) - sin(B)*sin(Deklination)) / (cos(B)*cos(Deklination)))/PI;
}

// subfunction to compute Zeitgleichung
float zeitgleichung(int T) {
  return -0.170869921174742*sin(0.0336997028793971 * T + 0.465419984181394) - 0.129890681040717*sin(0.0178674832556871*T - 0.167936777524864);
}

// subfunction to compute sunrise
float aufgang(int T, float B) {
 float DK = sonnendeklination(T);
 return 12 - zeitdifferenz(DK, B) - zeitgleichung(T);
}

// subfunction to compute sunset
float untergang(int T, float B) {
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
float computeAufgang(int T, float Laenge, float Breite) {
 
 int   Zone       = 1; // Unterschied zu UTC-Zeit ( = 2 in der Sommerzeit, 1 in der Winterzeit )
 float B = Breite*PI/180.0; // geogr. Breite in Radians
 
 // Berechnung von Sonnenaufgang
 float Aufgang = aufgang(T, B); // Sonnenaufgang bei 0 Grad Laenge

 Aufgang = Aufgang   - Laenge /15.0 + Zone; // Sonnenaufgang bei gesuchter Laenge und Zeitzone in Stunden
 return Aufgang;
}

float computeUntergang(int T, float Laenge, float Breite) {
 
 int   Zone       = 1; // Unterschied zu UTC-Zeit ( = 2 in der Sommerzeit, 1 in der Winterzeit )
 float B = Breite*PI/180.0; // geogr. Breite in Radians
 
 // Berechnung von Sonnenuntergang

 float Untergang = untergang(T, B); // Sonnenuntergang bei 0 Grad Laenge

 Untergang = Untergang - Laenge /15.0 + Zone; // Sonnenuntergang bei gesuchter Laenge und Zeitzone in Stunden
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

int caldayofyear(int day,int mon,int year)
{
    int  days_in_feb = 28;
    int doy;    // day of year

  
    doy = day;

    // check for leap year
    if( (year % 4 == 0 && year % 100 != 0 ) || (year % 400 == 0) )
    {
        days_in_feb = 29;
    }

    switch(mon)
    {
        case 2:
            doy += 31;
            break;
        case 3:
            doy += 31+days_in_feb;
            break;
        case 4:
            doy += 31+days_in_feb+31;
            break;
        case 5:
            doy += 31+days_in_feb+31+30;
            break;
        case 6:
            doy += 31+days_in_feb+31+30+31;
            break;
        case 7:
            doy += 31+days_in_feb+31+30+31+30;
            break;            
        case 8:
            doy += 31+days_in_feb+31+30+31+30+31;
            break;
        case 9:
            doy += 31+days_in_feb+31+30+31+30+31+31;
            break;
        case 10:
            doy += 31+days_in_feb+31+30+31+30+31+31+30;            
            break;            
        case 11:
            doy += 31+days_in_feb+31+30+31+30+31+31+30+31;            
            break;                        
        case 12:
            doy += 31+days_in_feb+31+30+31+30+31+31+30+31+30;            
            break;                                    
    }

  

    return doy; // return day of year 
}
//_________________________________________________________________End calculate day of year

//____________________________________________________________________SetPV flag
// this function calculates start and end of the PV production 
// based on sunrise and sunset and a lag factor

bool setPVstartflag(float lagmorning, float lagevening) 
{
bool pvstartflag=false;
float startpv, stoppv;
float laenge     = 9.0658; // Postition of PV
float breite     = 48.7990; // Position of PV
float currenttime; // this is the time in decimal hours i.e. 8.5


timeClient.update();
 time_t epochTime = timeClient.getEpochTime();
 float currentHour = timeClient.getHours();// get current hour
 float currentMinute = timeClient.getMinutes(); // get current menutes
  struct tm *ptm = gmtime ((time_t *)&epochTime);
     int monthDay = ptm->tm_mday;// get current day of the month
     int currentMonth = ptm->tm_mon+1;//get current month
     int currentYear = ptm->tm_year+1900; // get current year
  
dayofyear=caldayofyear(monthDay,currentMonth,currentYear);// calculate current day of the year
  
  startpv=computeAufgang(dayofyear, laenge, breite) + lagmorning; // statring time of pv 
  stoppv=computeUntergang(dayofyear,laenge,breite)-lagevening; // stop time of PV

currenttime=currentHour + (currentMinute/60); // calculate current ime in decimal hours

// figure out wethere pv can produce power or not
// flag pvstartflag is true at daytime when pv can procuce power
if (currenttime>=startpv & currenttime<=stoppv){
  pvstartflag=true;
}else pvstartflag= false;

//__________________________-Test ausgabe

Serial.print ("Sonnenaufgang: ");
Serial.println(computeAufgang(dayofyear, laenge, breite));
Serial.print("Sonnenuntergang: ");
Serial.println(computeUntergang(dayofyear,laenge,breite));
  

Serial.print("Day of the year: ");
Serial.println (dayofyear);

Serial.print("Current minute: ");
Serial.println (currentMinute);


Serial.print("Current time decimal: ");
Serial.println (currenttime);

Serial.print("start PV: ");
Serial.println (startpv);

Serial.print("stoppv PV: ");
Serial.println (stoppv);

Serial.print("pvstartflag: ");
Serial.println (pvstartflag);

String currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay);
  Serial.print("Current date: ");
  Serial.println(currentDate);

return pvstartflag; // return pv start produce flag
}


//____________________________________________________________________end day of year


//________________________________________________START NTP Client
// Initialize a NTPClient to get time to calculate sunrise and sunset
void setuptimeClient (){
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.begin();  
  timeClient.setTimeOffset(3600);
  
}
//________________________________________________END NTP client