#include <Arduino.h>
#include <app_access_point.h>

void setup()
{
  // put your setup code here, to run once:
  AccessPoint::Init();
}

void loop()
{
  // put your main code here, to run repeatedly:
  AccessPoint::Loop();
}