

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
bool setPVstartflag(float lagmorning, float lagevening);

