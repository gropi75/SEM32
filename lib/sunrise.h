#ifndef sunrise_H
#define sunrise_H

struct TimeStruct {
    float sunrise_today; // sunrise today
    float sunset_today; // sunset today
    float sunrise_tomorrow; // sunrise tomorrow
    int day_of_week; // shows which day is today, Monday.... Sunday
    char*  date_today; // date of today with format yyyymmdd (20230312)
    char* date_tomorrow; // date for tomorro with format yyyymmdd (20230312)
};
void TimeData(struct TimeStruct *s,float lagmorning, float lagevening, float laenge, float breite);

// Function to setup the time client
void setuptimeClient ();


// setPVstartflag is a function that calculates start and end of the PV production 
// based on sunrise and sunset and a lag factor
// the lag factor has to be determined by calculating the time from theoretical sunrise to first PV production 

//Variables:
//float lagmorning=1.5;// time in decimal hours between real sunset and start of PV
// float lagevening=2;  // time in decimal hours between end of PV power production and real sunset

// This function figures out wethere PV can produce power or not
// flag pvstartflag is true at daytime when pv can procuce power and false when PV can not produce power
bool setPVstartflag(float lagmorning, float lagevening, float laenge, float breite);

// current time in decimal hours
float factualtimeinhours(); 

// add on  Issue in Github Provide time information #16 
//float sunrisetoday(); // sunrise of current day in decimal hours including lag
//float sunsettoday(); // sunset of current Day including lag
//float sunrisetomorow(); // sunrise today of next day including lag
//float actualtimeinhours(); // current time in decimal hours
//int dayofweek(); //current day of week as integer 1 monday ... 7 sunday
//char date_today(); // current datedate of today with format yyyymmdd (20230312)
//char date_tomorow();// date of tomorow with format yyyymmdd (20230312)

//float fstartpv(float lagmorning, float lagevening, float laenge, float breite); // time when PV starts producing= sunrise + lagmorning
//float fstoppv(float lagmorning, float lagevening,float laenge, float breite); // time when PV stopps producing= sunset - lagevening
//float fstartpvtomorow(float lagmorning, float lagevening,float laenge, float breite); // Time when PV starts producing next day= sunrise + lagmorning

//int fweekday();  //function returns the current week day as a char
//char* fdate_today();  // current date of today with format yyyymmdd (20230312)
//char* fdate_tomorow(); // date of tomorow with format yyyymmdd (20230312)

#endif //sunrise_H