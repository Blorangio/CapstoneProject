#include "LIS3DH.h"
#include "MQTT.h"

#define accelerometerChipSelect D2
#define redLedPin D3
#define greenLedPin D4
#define blueLedPin D5
#define generalPower D6
#define ledPin V0

SYSTEM_THREAD(ENABLED);

void callback(char *topic, byte *payload, unsigned int length);

MQTT client("lab.thewcl.com", 1883, callback);

LIS3DHConfig config;
LIS3DHSample sample;
LIS3DHSPI accel(SPI, accelerometerChipSelect, WKP);

void readyToUpdateFunc();
void previouslyShakedTrue();
void updateAfterShakesCounted();
void publishToMQTT();

bool isOnStand = 0;

Timer readyToUpdateTimer(500, readyToUpdateFunc, true);
Timer ledOFF(500, previouslyShakedTrue, true);
Timer resettimesShaked(1000, updateAfterShakesCounted, true);
Timer publishMQTT(2000, publishToMQTT, false);
bool readyToUpdate = true;

double movementAxis[3];
double previousAxisData[3];

int timesShaked = 0;
bool previouslyShakedBool = false;
double shakingSensitivity = 0.6;

float speedOfArgon[3];

double movement1Offset = 0;

int red = 255;
int green;
int blue;

String strRed = "255";
String strGreen = "000";
String strBlue = "000";
String strBrightness = "255";

String data = "0000000000000";

int colorIndex = 0;
int mappedColorIndex;

bool isLightOn = false;
bool changingBrightness = false;
bool changingColor = false;
int brightness = 64;

bool readyToPublish = false;

void setup()
{
  config.setAccelMode(LIS3DH::RATE_100_HZ);
  accel.setup(config);

  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(blueLedPin, OUTPUT);
  pinMode(generalPower, OUTPUT);

  analogWrite(generalPower, map(brightness, -64, 64, 0, 255));
  digitalWrite(redLedPin, HIGH);
  digitalWrite(greenLedPin, HIGH);
  digitalWrite(blueLedPin, HIGH);

  publishMQTT.start();
  client.subscribe("gestureLamp2");
}

void loop()
{
  Serial.begin(9600);
  if (client.isConnected())
  {
    client.loop();
  }
  else
  {
    client.connect(System.deviceID());
    client.subscribe("gestureLamp2");
  }
  if (isOnStand)
  {
    isOnStandFunc();
  }
}

void isOnStandFunc()
{
  if (readyToPublish)
  {
    data = strRed + strGreen + strBlue + strBrightness + (String)(isLightOn + 2 * (changingBrightness || changingColor));
    client.publish("gestureLamp", data);
    readyToPublish = false;
  }
  if (accel.getSample(sample))
  {
    for (int i = 0; i < 3; i++)
    {
      previousAxisData[i] = movementAxis[i];
    }
    movementAxis[0] = round(sample.x / (32768.0 / 2.0) * 10.0) / 10.0;
    movementAxis[1] = round(sample.y / (32768.0 / 2.0) * 10.0) / 10.0 - movement1Offset;
    movementAxis[2] = round(sample.z / (32768.0 / 2.0) * 10.0) / 10.0;
    if(!changingBrightness&&!changingColor&&movementAxis[0]<=0.2&&movementAxis[0]>=-0.2) {
      movement1Offset += movementAxis[1];
    }
  }
  for (int i = 0; i < 3; i++)
  {
    if ((previousAxisData[i] - shakingSensitivity >= movementAxis[i] || previousAxisData[i] + shakingSensitivity <= movementAxis[i]) && !previouslyShakedBool)
    {
      timesShaked++;
      switch (timesShaked % 4)
      {
      case 1:
        digitalWrite(redLedPin, isLightOn);
        digitalWrite(greenLedPin, HIGH);
        digitalWrite(blueLedPin, HIGH);
        break;
      case 2:
        digitalWrite(greenLedPin, LOW);
        digitalWrite(redLedPin, HIGH);
        digitalWrite(blueLedPin, HIGH);
        break;
      case 3:
        digitalWrite(blueLedPin, LOW);
        digitalWrite(greenLedPin, HIGH);
        digitalWrite(redLedPin, HIGH);
        break;
      }

      previouslyShakedBool = true;

      ledOFF.start();
      resettimesShaked.reset();
      resettimesShaked.start();
    }
  }
  if (readyToUpdate)
  {
    readyToUpdate = false;
    readyToUpdateTimer.start();

    if (changingColor)
    {
      colorIndex += movementAxis[1] * 10;
      mappedColorIndex = map(colorIndex, -128, 128, 0, 7 * 255);
      green = -abs(mappedColorIndex - (255 * 1.5)) + (255 * 1.5);
      blue = -abs(mappedColorIndex - (255 * 2.5)) + (255 * 1.5);
      red = abs(mappedColorIndex - (255 * 2)) - 255;
      if (red > 255)
      {
        red = 255;
      }
      else if (red < 0)
      {
        red = 0;
      }
      if (green > 255)
      {
        green = 255;
      }
      else if (green < 0)
      {
        green = 0;
      }
      if (blue > 255)
      {
        blue = 255;
      }
      else if (blue < 0)

      {
        blue = 0;
      }
      setColor(red, green, blue);
      strRed = (String)red;
      strGreen = (String)green;
      strBlue = (String)blue;
      for (int i = 0; i < 3 - strRed.length(); i += 0)
      {
        strRed = "0" + strRed;
      }
      for (int i = 0; i < 3 - strGreen.length(); i += 0)
      {
        strGreen = "0" + strGreen;
      }
      for (int i = 0; i < 3 - strBlue.length(); i += 0)
      {
        strBlue = "0" + strBlue;
      }
    }

    if (changingBrightness)
    {
      brightness += movementAxis[1] * 10;
      if (brightness < -64)
      {
        brightness = -64;
      }
      else if (brightness > 64)
      {
        brightness = 64;
      }
      strBrightness = (String)map(brightness, -64, 64, 0, 255);
      for (int i = 0; i < 3 - strBrightness.length(); i += 0)
      {
        strBrightness = "0" + strBrightness;
      }
      analogWrite(generalPower, map(brightness, -64, 64, 0, 255));
    }
  }
}

void setColor(int red, int green, int blue)
{
  analogWrite(redLedPin, 255 - red);
  analogWrite(greenLedPin, 255 - green);
  analogWrite(blueLedPin, 255 - blue);
}

void readyToUpdateFunc()
{
  readyToUpdate = true;
}

void previouslyShakedTrue()
{
  previouslyShakedBool = false;
}

void updateAfterShakesCounted()
{
  if (!changingColor)
  {
    switch (timesShaked % 4)
    {
    case 1:
      changingBrightness = false;
      isLightOn = !isLightOn;
      digitalWrite(redLedPin, isLightOn);
      digitalWrite(greenLedPin, HIGH);
      digitalWrite(blueLedPin, HIGH);
      break;
    case 2:
      changingBrightness = false;
      changingColor = true;
      colorIndex = 0;
      speedOfArgon[0] = 0.0;
      speedOfArgon[1] = 0.0;
      speedOfArgon[2] = 0.0;
      break;
    case 3:
    case 0:
      digitalWrite(blueLedPin, LOW);
      digitalWrite(greenLedPin, HIGH);
      digitalWrite(redLedPin, HIGH);
      changingBrightness = true;
      break;
    }
  }
  else
  {
    setColor(red, green, blue);
    changingColor = false;
  }
  timesShaked = 0;
}

void publishToMQTT()
{
  readyToPublish = true;
}

void callback(char *topic, byte *payload, unsigned int length)
{
  char p[length + 1];
  memcpy(p, payload, length);
  p[length] = NULL;

  // To parse an int
  isOnStand = atoi(p);
}