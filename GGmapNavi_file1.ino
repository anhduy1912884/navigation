/*  Rui Santos & Sara Santos - Random Nerd Tutorials
    THIS EXAMPLE WAS TESTED WITH THE FOLLOWING HARDWARE:
    1) ESP32-2432S028R 2.8 inch 240×320 also known as the Cheap Yellow Display (CYD): https://makeradvisor.com/tools/cyd-cheap-yellow-display-esp32-2432s028r/
      SET UP INSTRUCTIONS: https://RandomNerdTutorials.com/cyd/
    2) REGULAR ESP32 Dev Board + 2.8 inch 240x320 TFT Display: https://makeradvisor.com/tools/2-8-inch-ili9341-tft-240x320/ and https://makeradvisor.com/tools/esp32-dev-board-wi-fi-bluetooth/
      SET UP INSTRUCTIONS: https://RandomNerdTutorials.com/esp32-tft/
    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "ImagesDirections.h"
#include "ImagesLanes.h"
#include "DataConstants.h"
#include "IDisplay.h"



#include <SPI.h>

/*  Install the "TFT_eSPI" library by Bodmer to interface with the TFT Display - https://github.com/Bodmer/TFT_eSPI
    *** IMPORTANT: User_Setup.h available on the internet will probably NOT work with the examples available at Random Nerd Tutorials ***
    *** YOU MUST USE THE User_Setup.h FILE PROVIDED IN THE LINK BELOW IN ORDER TO USE THE EXAMPLES FROM RANDOM NERD TUTORIALS ***
    FULL INSTRUCTIONS AVAILABLE ON HOW CONFIGURE THE LIBRARY: https://RandomNerdTutorials.com/cyd/ or https://RandomNerdTutorials.com/esp32-tft/   */
#include <TFT_eSPI.h>
//#include <Fonts/FreeSans9pt7b.h>

// Install the "XPT2046_Touchscreen" library by Paul Stoffregen to use the Touchscreen - https://github.com/PaulStoffregen/XPT2046_Touchscreen
// Note: this library doesn't require further configuration
//#include <XPT2046_Touchscreen.h>

TFT_eSPI tft = TFT_eSPI();

// Touchscreen pins
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

//SPIClass touchscreenSPI = SPIClass(VSPI);
//XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define FONT_SIZE 2
#define IMG_WIDTH 32
#define IMG_HIGH  64
#define IMG_X     88
#define IMG_Y     125


// Định nghĩa UUID từ bảng của Sygic
#define SERVICE_UUID        "DD3F0AD1-6239-4E1F-81F1-91F6C9F01D86"
#define CHAR_INDICATE_UUID  "DD3F0AD2-6239-4E1F-81F1-91F6C9F01D86"
#define CHAR_WRITE_UUID     "DD3F0AD3-6239-4E1F-81F1-91F6C9F01D86"

#define CLEAR_DISPLAY tft.fillScreen(TFT_BLACK);

BLECharacteristic *pWriteCharacteristic;
bool deviceConnected = false;
uint32_t initialDistance = 0;
uint32_t currentDistance = 0;

// Hàm vẽ icon chữ "H" bằng khối hình học
void drawDestinationIcon() {
  tft.fillRect(50, 10, 8, 30, TFT_WHITE); // Thanh dọc trái
  tft.fillRect(70, 10, 8, 30, TFT_WHITE); // Thanh dọc phải
  tft.fillRect(58, 22, 12, 6, TFT_WHITE); // Thanh ngang ở giữa
}

// Hàm vẽ thanh tỷ lệ khoảng cách - Thanh sát dưới, chữ ở trên thanh
void drawDistanceBar(uint32_t distanceTravelled, uint32_t maxDistance) {
  const int barX = 10;
  const int barY = SCREEN_HEIGHT - 12; // Thanh sát mép dưới (64 - 12 = 52)
  const int barWidth = 108;
  const int barHeight = 12;

  tft.drawRect(barX, barY, barWidth, barHeight, TFT_WHITE);
  int fillWidth = map(distanceTravelled, 0, maxDistance, 0, barWidth - 2);
  if (fillWidth < 0) fillWidth = 0;
  if (fillWidth > barWidth - 2) fillWidth = barWidth - 2;

  tft.fillRect(barX + 1, barY + 1, fillWidth, barHeight - 2, TFT_WHITE);

  // Tính toán giá trị khoảng cách còn lại
  uint32_t remainingDistance = maxDistance - distanceTravelled;
  tft.setTextSize(3);

  // Tính chiều rộng thực tế của chuỗi (mỗi ký tự rộng ~16 pixel với textSize=2)
  int digits = remainingDistance == 0 ? 1 : (int)log10(remainingDistance) + 1; // Số chữ số
  int textWidth = digits * 16 + 16; // Chiều rộng của số + "m"
  int maxX = SCREEN_WIDTH - 5; // Giới hạn bên phải màn hình (margin 5 pixel)
  int cursorX = maxX - textWidth; // Dịch sang trái nếu vượt quá giới hạn
  if (cursorX < barX + barWidth - 60) cursorX = barX + barWidth - 60; // Vị trí mặc định

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(80, barY - 15); // Chữ ở trên thanh (52 - 15 = 37)
  tft.print(remainingDistance);
  tft.print("m");

  Serial.print("Distance Travelled: ");
  Serial.print(distanceTravelled);
  Serial.print(" | Max Distance: ");
  Serial.print(maxDistance);
  Serial.print(" | Remaining: ");
  Serial.print(remainingDistance);
  Serial.print(" | cursorX: ");
  Serial.print(cursorX);
  Serial.print(" | textWidth: ");
  Serial.println(textWidth);
}

// Callback khi có thiết bị kết nối/ngắt kết nối
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Thiết bị đã kết nối");      
      CLEAR_DISPLAY;
      tft.setTextSize(1);
      tft.setCursor(20, 20);
      tft.println("Connected");
      };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Thiết bị đã ngắt kết nối");
      CLEAR_DISPLAY;
      tft.setTextSize(1);
      tft.setCursor(20, 20);
      tft.println("Disconnected");
      BLEDevice::startAdvertising();
    }
};

// Callback khi nhận dữ liệu từ characteristic
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      Serial.print("Dữ liệu mới nhận được, độ dài = ");
      Serial.print(value.length());
      Serial.print(": ");
      
      if (value.length() > 0) {
        for (int i = 0; i < value.length(); i++) {
          char tmp[4];
          sprintf(tmp, "%02X ", value[i]);
          Serial.print(tmp);
        }
        Serial.println();
        tft.setTextColor(TFT_WHITE);
        CLEAR_DISPLAY;
        
        if (value[0] == 0x01 && value.length() >= 3) {
          uint8_t speed = value[1];
          tft.setTextSize(1);
          int textWidth = speed < 10 ? 12 : 24;
          int circleX = 70;
          int circleY = 90;
          int xPos = circleX - (textWidth / 2);
          int yPos = circleY - 8;
          
         
          tft.fillCircle(circleX, circleY, 34, TFT_RED);
          tft.fillCircle(circleX, circleY, 30, TFT_WHITE);

          tft.setFreeFont(&FreeMonoBold12pt7b);
          tft.setTextColor(TFT_BLACK);
          tft.setCursor(xPos, yPos);
          tft.print(speed);

          uint8_t direction = value[2];
          Serial.print("Direction: 0x");
          Serial.println(direction, HEX);

          if (value.length() > 3) {
            uint32_t distance = 0;
            if (value.length() == 11 && value.substr(3) == "No route") {
              Serial.println("Đã đến đích (No route)");
              drawDestinationIcon(); // Hiển thị chữ "H" khi đến đích
            } 
            else if (value.length() == 6) { // Xử lý "100m"
              std::string distanceStr = value.substr(3, 2);
              distance = atoi(distanceStr.c_str());
              if (distance > 0) {
                if (initialDistance == 0 || distance > initialDistance) {
                  initialDistance = distance;
                  currentDistance = distance;
                } else {
                  currentDistance = distance;
                }
                uint32_t distanceTravelled = initialDistance - currentDistance;
                drawDistanceBar(distanceTravelled, initialDistance);

                if (currentDistance == 0) {
                  Serial.println("Đã đến đích (distance = 0)");
                  drawDestinationIcon(); // Hiển thị chữ "H" khi khoảng cách = 0
                } else {
                  drawDirection(direction);                              
                }
              } else {
                Serial.println("Khoảng cách không hợp lệ");
              }
            } 
            else if (value.length() == 7) { // Xử lý "240m"
              std::string distanceStr = value.substr(3, 3);
              distance = atoi(distanceStr.c_str());
              if (distance > 0) {
                if (initialDistance == 0 || distance > initialDistance) {
                  initialDistance = distance;
                  currentDistance = distance;
                } else {
                  currentDistance = distance;
                }
                uint32_t distanceTravelled = initialDistance - currentDistance;
                drawDistanceBar(distanceTravelled, initialDistance);

                if (currentDistance == 0) {
                  Serial.println("Đã đến đích (distance = 0)");
                  drawDestinationIcon(); // Hiển thị chữ "H" khi khoảng cách = 0
                } else {
                  drawDirection(direction);            
                  
                }
              } else {
                Serial.println("Khoảng cách không hợp lệ");
              }
            } 
            else if (value.length() == 8) { // Xử lý "1.7km"
              std::string distanceStr = value.substr(3, 3);
              float distanceKm = atof(distanceStr.c_str());
              distance = (uint32_t)(distanceKm * 1000);
              if (distance > 0) {
                if (initialDistance == 0 || distance > initialDistance) {
                  initialDistance = distance;
                  currentDistance = distance;
                } else {
                  currentDistance = distance;
                }
                uint32_t distanceTravelled = initialDistance - currentDistance;
                drawDistanceBar(distanceTravelled, initialDistance);

                if (currentDistance == 0) {
                  Serial.println("Đã đến đích (distance = 0)");
                  drawDestinationIcon(); // Hiển thị chữ "H" khi khoảng cách = 0
                } else {
                  drawDirection(direction);                              
                }
              } else {
                Serial.println("Khoảng cách không hợp lệ");
              }
            } 
            else {
              Serial.println("Dữ liệu khoảng cách không đủ dài hoặc không hỗ trợ");
            }
          } else {
            Serial.println("Không có dữ liệu khoảng cách");
            drawDirection(direction);            
          }
        }                
      } else {
        Serial.println("Không có dữ liệu");
      }
    }
};

void setup() {
  Serial.begin(115200);
  // Start the tft display
  tft.init();
  // Set the TFT display rotation in landscape mode
  tft.setRotation(0);

  // Clear the screen before writing to it
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  BLEDevice::init("ESP32_Sygic_HUD");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  BLECharacteristic *pIndicateCharacteristic = pService->createCharacteristic(
                      CHAR_INDICATE_UUID,
                      BLECharacteristic::PROPERTY_INDICATE
                    );
  pIndicateCharacteristic->addDescriptor(new BLE2902());
  pIndicateCharacteristic->setValue("");

  pWriteCharacteristic = pService->createCharacteristic(
                      CHAR_WRITE_UUID,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pWriteCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Đang chờ kết nối từ Sygic...");
}

void loop() {  
    //tft.pushImage(120, 3, 64, 64, duyTurnRight);
    //drawDirection(DirectionStraight); 
    
    //tft.pushImage(IMG_X, IMG_Y, 64, 64, duyTurnLeft , 1, NULL);
    delay(100);
}

void drawDirection(uint8_t dir)
{
  const uint8_t* imageProgmemChar = ImageFromDirectionChar(dir);
  const unsigned short * imageProgmemShort = ImageFromDirectionShort(dir);
  if (imageProgmemShort)
  {
    tft.pushImage(IMG_X, IMG_Y, 64, 64, imageProgmemShort);
  }
  else if (imageProgmemChar)
  {
      tft.pushImage(IMG_X, IMG_Y, IMG_WIDTH, IMG_HIGH, imageProgmemChar , 1, NULL);
  }
}

const uint8_t* ImageFromDirectionChar(uint8_t direction)
{
    switch (direction)
    {
        case DirectionNone: return nullptr;
        case DirectionStart: return IMG_directionWaypoint;
        case DirectionEasyLeft: return IMG_directionEasyLeft;
        case DirectionEasyRight: return IMG_directionEasyRight;
        case DirectionEnd: return IMG_directionWaypoint;
        case DirectionVia: return IMG_directionWaypoint;
        case DirectionKeepLeft: return IMG_directionKeepLeft;
        case DirectionKeepRight: return IMG_directionKeepRight;
        case DirectionLeft: return IMG_directionLeft;
        case DirectionOutOfRoute: return IMG_directionOutOfRoute;
        case DirectionRight: return IMG_directionRight;
        case DirectionSharpLeft: return IMG_directionSharpLeft;
        case DirectionSharpRight: return IMG_directionSharpRight;
        case DirectionStraight: return IMG_directionStraight;
        case DirectionUTurnLeft: return IMG_directionUTurnLeft;
        case DirectionUTurnRight: return IMG_directionUTurnRight;
        case DirectionFerry: return IMG_directionFerry;
        case DirectionStateBoundary: return IMG_directionStateBoundary;
        case DirectionFollow: return IMG_directionFollow;
        case DirectionMotorway: return IMG_directionMotorway;
        case DirectionTunnel: return IMG_directionTunnel;
        case DirectionExitLeft: return IMG_directionExitLeft;
        case DirectionExitRight: return IMG_directionExitRight;
        case DirectionRoundaboutRSE: return IMG_directionRoundaboutRSE;
        case DirectionRoundaboutRE: return IMG_directionRoundaboutRE;
        case DirectionRoundaboutRNE: return IMG_directionRoundaboutRNE;
        case DirectionRoundaboutRN: return IMG_directionRoundaboutRN;
        case DirectionRoundaboutRNW: return IMG_directionRoundaboutRNW;
        case DirectionRoundaboutRW: return IMG_directionRoundaboutRW;
        case DirectionRoundaboutRSW: return IMG_directionRoundaboutRSW;
        case DirectionRoundaboutRS: return IMG_directionRoundaboutRS;
        case DirectionRoundaboutLSE: return IMG_directionRoundaboutLSE;
        case DirectionRoundaboutLE: return IMG_directionRoundaboutLE;
        case DirectionRoundaboutLNE: return IMG_directionRoundaboutLNE;
        case DirectionRoundaboutLN: return IMG_directionRoundaboutLN;
        case DirectionRoundaboutLNW: return IMG_directionRoundaboutLNW;
        case DirectionRoundaboutLW: return IMG_directionRoundaboutLW;
        case DirectionRoundaboutLSW: return IMG_directionRoundaboutLSW;
        case DirectionRoundaboutLS: return IMG_directionRoundaboutLS;
        default: Serial.println("No arrow drawn"); return NULL;
    }
    return IMG_directionError;
}

const unsigned short  * ImageFromDirectionShort(uint8_t direction){
  switch (direction)
    {
        case DirectionNone: return NULL;
        case DirectionStart: return NULL;
        case DirectionEasyLeft: return NULL;
        case DirectionEasyRight: return NULL;
        case DirectionEnd: return NULL;
        case DirectionVia: return NULL;
        case DirectionKeepLeft: return NULL;
        case DirectionKeepRight: return NULL;
        case DirectionLeft: return duyTurnLeft;
        case DirectionOutOfRoute: return NULL;
        case DirectionRight: return duyTurnRight;
        case DirectionSharpLeft: return NULL;
        case DirectionSharpRight: return NULL;
        case DirectionStraight: return duyStraight;
        case DirectionUTurnLeft: return NULL;
        case DirectionUTurnRight: return NULL;
        case DirectionFerry: return NULL;
        case DirectionStateBoundary: return NULL;
        case DirectionFollow: return NULL;
        case DirectionMotorway: return NULL;
        case DirectionTunnel: return NULL;
        case DirectionExitLeft: return NULL;
        case DirectionExitRight: return NULL;
        case DirectionRoundaboutRSE: return NULL;
        case DirectionRoundaboutRE: return NULL;
        case DirectionRoundaboutRNE: return NULL;
        case DirectionRoundaboutRN: return NULL;
        case DirectionRoundaboutRNW: return NULL;
        case DirectionRoundaboutRW: return NULL;
        case DirectionRoundaboutRSW: return NULL;
        case DirectionRoundaboutRS: return NULL;
        case DirectionRoundaboutLSE: return NULL;
        case DirectionRoundaboutLE: return NULL;
        case DirectionRoundaboutLNE: return NULL;
        case DirectionRoundaboutLN: return NULL;
        case DirectionRoundaboutLNW: return NULL;
        case DirectionRoundaboutLW: return NULL;
        case DirectionRoundaboutLSW: return NULL;
        case DirectionRoundaboutLS: return NULL;
        default: Serial.println("No arrow drawn"); return NULL;
    }
    return NULL;
}