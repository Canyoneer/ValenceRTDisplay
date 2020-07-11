#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <Button2.h>
#include <HardwareSerial.h>

#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN   0x10
#endif

#define TFT_MOSI            19
#define TFT_SCLK            18
#define TFT_CS              5
#define TFT_DC              16
#define TFT_RST             23

#define TFT_BL          4  // Display backlight control pin
#define BUTTON_1        35
#define BUTTON_2        0

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library
Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);

String errorMsg = "";

void espDelay(int ms);
void button_init();
void drawBatteryData();
void requestBatteryData();
void readSerialData();
void readSerialDataTimestamped();
void readSerialData(uint8_t *buffer, size_t length);
void sendMessageToBattery(byte message[], size_t len);
uint16_t ModRTU_CRC(byte buf[], uint16_t len);
void logByteArray(byte data[], size_t len);

HardwareSerial Serial485(2);

byte reqInit[] = {0x00, 0x03, 0x00, 0xfc, 0x00, 0x00};
byte reqLoopInit[] = {0xff, 0x43, 0x06, 0x06};
byte reqLoop[] = {0xff, 0x50, 0x06, 0x00};
byte reqData[] = {0x00, 0x03, 0x00, 0x29, 0x00, 0x1b};
byte reqAssignBatteryId[] = {0xff, 0x41, 0x12, 0x43, 0x42, 0x42, 0x31, 0x31, 0x32, 0x38, 0x31, 0x30, 0x00, 0x00, 0x00, 0x00};

int assignedBatteryIds = 0;
byte batteryData[4][59];

unsigned long firstReadTime = 0;


//! Long time delay, it is recommended to use shallow sleep, which can effectively reduce the current consumption
void espDelay(int ms)
{   
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,ESP_PD_OPTION_ON);
    esp_light_sleep_start();
}

void button_init()
{
    btn1.setLongClickHandler([](Button2 & b) {

    });

    btn1.setClickHandler([](Button2 & b) {
        requestBatteryData();
    });

    btn2.setClickHandler([](Button2 & b) {
        
    });
}

void button_loop()
{
    btn1.loop();
    btn2.loop();
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Start");
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);

    if (TFT_BL > 0) { // TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
         pinMode(TFT_BL, OUTPUT); // Set backlight pin to output mode
         digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on. TFT_BACKLIGHT_ON has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
    }

    tft.setSwapBytes(true);   
    
    button_init();
    
    Serial485.begin(115200, SERIAL_8N1, 37, 32);
    Serial485.setRxBufferSize(1024);

    batteryData[0][0] = 0;
    batteryData[1][0] = 0;
    batteryData[2][0] = 0;
    batteryData[3][0] = 0;

    Serial.println();

}

void loop()
{
    button_loop();

    delay(1);  // don't just use espDelay, because then the watchdog will eventually reset the device because it considers it as not responding
    espDelay(20);    
}

void logByteArray(byte data[], size_t len) 
{
    for (int i = 0; i < len; ++i) {
        Serial.print(data[i]>>4,  HEX);
        Serial.print(data[i]&0x0F, HEX);
    }
}

void requestBatteryData() 
{
    byte response[100];
    assignedBatteryIds = 0;

    sendMessageToBattery(reqInit, sizeof(reqInit) / sizeof(byte));
    sendMessageToBattery(reqInit, sizeof(reqInit) / sizeof(byte));

    for (int i = 0; i < 3; i++) {
        sendMessageToBattery(reqLoopInit, sizeof(reqLoopInit) / sizeof(byte));
        sendMessageToBattery(reqLoopInit, sizeof(reqLoopInit) / sizeof(byte));

        for (int j = 0; j < 6; j++) {
            reqLoop[3] = j;
            sendMessageToBattery(reqLoop, sizeof(reqLoop) / sizeof(byte));
            delay(30);
            readSerialData(response, 20);

            if (response[0] == 0xff) {
                reqAssignBatteryId[12] = response[12];
                reqAssignBatteryId[13] = response[13];
                reqAssignBatteryId[14] = response[14];
                reqAssignBatteryId[15] = (byte) (assignedBatteryIds + 2);

                sendMessageToBattery(reqAssignBatteryId, 16);
                delay(35);

                readSerialData(); // response should be validated

                assignedBatteryIds++;
                memset(response, 0, 20);
            }
        }

    }

    if (assignedBatteryIds > 0) {
            
        for (int i = 0; i < assignedBatteryIds; i++)
        {
            reqData[0] = (byte) (i + 2);
            sendMessageToBattery(reqData, 6);
            delay(8);

            readSerialData(batteryData[i], 59); // TODO: response should be validated
        }            

        drawBatteryData();

    } else {
        Serial.println("no batteries found");
    }

    Serial.flush();
}

void sendMessageToBattery(byte message[], size_t len) 
{
    uint16_t crc = ModRTU_CRC(message, len);
    Serial485.write(message, len);
    Serial485.write(highByte(crc));
    Serial485.write(lowByte(crc));
    Serial485.flush();
    logByteArray(message, len);
    Serial.print(crc,  HEX);
    Serial.println();
    delay(1);
}

void readSerialData(uint8_t *buffer, size_t length) 
{
    size_t count = 0;
    if (Serial485.available()) {
        Serial.print("<= ");
        
        while (Serial485.available() > 0 && count < length) {
            buffer[count] = Serial485.read();
            
            Serial.print(buffer[count]>>4,  HEX);
            Serial.print(buffer[count]&0x0F, HEX);

            count++;
        }    
        Serial.println();
    }
}

void readSerialData() {
    if (Serial485.available() > 0) {
        Serial.print("<= ");
        
        while (Serial485.available() > 0) {
            int received = Serial485.read();
            
            Serial.print(received>>4,  HEX);
            Serial.print(received&0x0F, HEX);

        }    
        Serial.println("<= "); 
    }
}

void readSerialDataTimestamped() 
{

    if (Serial485.available() > 0) {
        if (firstReadTime == 0) {
            firstReadTime = millis();
        }

        Serial.printf("%6lu ", (millis() - firstReadTime));        // - firstReadTime
        // Serial.print(millis() - firstReadTime);

        while (Serial485.available() > 0) {
            int received = Serial485.read();
            
            Serial.print(received>>4,  HEX);
            Serial.print(received&0x0F, HEX);

        }    
        Serial.println(); 
    }
}

void drawBatteryData() 
{
    tft.fillScreen(TFT_BLACK);

    tft.setCursor(0, 0);
    tft.print("SOC:");    
    for (uint8_t i = 0; i < assignedBatteryIds; i++)
    {
        float soc = (float)batteryData[i][43] / 255 * 100;
        tft.printf(" %3.1f", soc);
    }

    tft.setCursor(0, 18);
    tft.print("I:");    
    for (uint8_t i = 0; i < assignedBatteryIds; i++)
    {
        int16_t current =  (batteryData[i][24] << 8) | (batteryData[i][23]);
        if (current > 32767) {
            current = current - 65536;// / 1000;
        }
        tft.printf(" %4.2f", ((double_t)current / 1000));
    }

    tft.setCursor(0, 36);
    tft.print("U:");    
    for (uint8_t i = 0; i < assignedBatteryIds; i++)
    {
        double_t voltage = (double_t)(batteryData[i][36] * 256 +  batteryData[i][35]) / 1000;
        tft.printf(" %4.2f", voltage);
    }

    tft.setCursor(0, 54);
    tft.print("P:");    
    for (uint8_t i = 0; i < assignedBatteryIds; i++)
    {
        int16_t current =  (batteryData[i][24] << 8) | (batteryData[i][23]);
        if (current > 32767) {
            current = current - 65536;// / 1000;
        }
        double_t power = ((double_t)current / 1000) * (((double_t)batteryData[i][36] * 256 +  (double_t)batteryData[i][35]) / 1000);
        tft.printf(" %3.1f", power);
    }

    tft.setCursor(0, 72);
    tft.print("T:");    
    for (uint8_t i = 0; i < assignedBatteryIds; i++)
    {
        tft.printf(" %2d", batteryData[i][3]);
    }

}


/* MODBUS CRC implementation from https://ctlsys.com/support/how_to_compute_the_modbus_rtu_message_crc/ could be replaced by LibCRC  */
uint16_t ModRTU_CRC(byte buf[], uint16_t len)
{
  uint16_t crc = 0xFFFF;
  
  for (uint16_t pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)buf[pos];
  
    for (uint16_t i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      }
      else
        crc >>= 1;
    }
  }
  // swap low and high byte
  uint16_t swap = (crc & 0xff)  << 8 | (crc & 0xff00) >> 8;
  return swap;
}
