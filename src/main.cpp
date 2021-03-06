/**
 *  The MLX90640 requires some hefty calculations and larger arrays. You will
 * need a microcontroller with 20,000 bytes or more of RAM. This relies on the
 * driver written by Melexis and can be found at:
 *  https://github.com/melexis/mlx90640-library
 */

#include <M5StickCPlus.h>
#include <Wire.h>

#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

#include "Adafruit_TCS34725.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define DELAY_MS 30 * 1000

#define COLOR_SERVICE_UUID "55c29024-939d-11ec-b909-0242ac120002"
#define COLOR_CHARA_UUID "5e7b4422-939d-11ec-b909-0242ac120002"
#define THERMAL_SERVICE_UUID "55c29025-939d-11ec-b909-0242ac120002"
#define THERMAL_CHARA_UUID "5e7b4423-939d-11ec-b909-0242ac120002"

BLEServer *pServer = NULL;
BLECharacteristic *pColorChara;
BLECharacteristic *pThermalChara;

// BLE 接続状態
bool deviceConnected = false;
bool oldDeviceConnected = false;

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer *pServer)
    {
        deviceConnected = false;
    }
};

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

TFT_eSprite img = TFT_eSprite(&M5.Lcd);
TFT_eSprite msg = TFT_eSprite(&M5.Lcd);

// color
#define commonAnode true // set to false if using a common cathode LED.  //如果使用普通阴极LED，则设置为false
byte gammatable[256];    // our RGB -> eye-recognized gamma color
static uint16_t color16(uint16_t r, uint16_t g, uint16_t b)
{
    uint16_t _color;
    _color = (uint16_t)(r & 0xF8) << 8;
    _color |= (uint16_t)(g & 0xFC) << 3;
    _color |= (uint16_t)(b & 0xF8) >> 3;
    return _color;
}

// thermal
#define TA_SHIFT 8 // Default shift for MLX90640 in open air
#define COLS 32
#define ROWS 24
#define COLS_3 (COLS * 3 - 2)
#define ROWS_3 (ROWS * 3 - 2)
#define pixelsArraySize (COLS * ROWS)
#define INTERPOLATED_COLS 32
#define INTERPOLATED_ROWS 32

float pixels[COLS * ROWS];
float reversePixels[COLS * ROWS];
float pixels_3[COLS_3 * ROWS_3];

#define get_pixels(x, y) (pixels[y * COLS + x])
#define get_pixels_3(x, y) (pixels_3[(y)*3 * COLS_3 + x])

byte speed_setting = 2; // High is 1 , Low is 2
bool reverseScreen = false;
const byte MLX90640_address =
    0x33; // Default 7-bit unshifted address of the MLX90640

paramsMLX90640 mlx90640;

// low range of the sensor (this will be blue on the screen)
float mintemp = 24;    // For color mapping
float min_v = 24;      // Value of current min temp
float min_cam_v = -40; // Spec in datasheet

// high range of the sensor (this will be red on the screen)
float maxtemp = 35;    // For color mapping
float max_v = 35;      // Value of current max temp
float max_cam_v = 300; // Spec in datasheet

// the colors we will be using
const uint16_t camColors[] = {
    0x480F,
    0x400F,
    0x400F,
    0x400F,
    0x4010,
    0x3810,
    0x3810,
    0x3810,
    0x3810,
    0x3010,
    0x3010,
    0x3010,
    0x2810,
    0x2810,
    0x2810,
    0x2810,
    0x2010,
    0x2010,
    0x2010,
    0x1810,
    0x1810,
    0x1811,
    0x1811,
    0x1011,
    0x1011,
    0x1011,
    0x0811,
    0x0811,
    0x0811,
    0x0011,
    0x0011,
    0x0011,
    0x0011,
    0x0011,
    0x0031,
    0x0031,
    0x0051,
    0x0072,
    0x0072,
    0x0092,
    0x00B2,
    0x00B2,
    0x00D2,
    0x00F2,
    0x00F2,
    0x0112,
    0x0132,
    0x0152,
    0x0152,
    0x0172,
    0x0192,
    0x0192,
    0x01B2,
    0x01D2,
    0x01F3,
    0x01F3,
    0x0213,
    0x0233,
    0x0253,
    0x0253,
    0x0273,
    0x0293,
    0x02B3,
    0x02D3,
    0x02D3,
    0x02F3,
    0x0313,
    0x0333,
    0x0333,
    0x0353,
    0x0373,
    0x0394,
    0x03B4,
    0x03D4,
    0x03D4,
    0x03F4,
    0x0414,
    0x0434,
    0x0454,
    0x0474,
    0x0474,
    0x0494,
    0x04B4,
    0x04D4,
    0x04F4,
    0x0514,
    0x0534,
    0x0534,
    0x0554,
    0x0554,
    0x0574,
    0x0574,
    0x0573,
    0x0573,
    0x0573,
    0x0572,
    0x0572,
    0x0572,
    0x0571,
    0x0591,
    0x0591,
    0x0590,
    0x0590,
    0x058F,
    0x058F,
    0x058F,
    0x058E,
    0x05AE,
    0x05AE,
    0x05AD,
    0x05AD,
    0x05AD,
    0x05AC,
    0x05AC,
    0x05AB,
    0x05CB,
    0x05CB,
    0x05CA,
    0x05CA,
    0x05CA,
    0x05C9,
    0x05C9,
    0x05C8,
    0x05E8,
    0x05E8,
    0x05E7,
    0x05E7,
    0x05E6,
    0x05E6,
    0x05E6,
    0x05E5,
    0x05E5,
    0x0604,
    0x0604,
    0x0604,
    0x0603,
    0x0603,
    0x0602,
    0x0602,
    0x0601,
    0x0621,
    0x0621,
    0x0620,
    0x0620,
    0x0620,
    0x0620,
    0x0E20,
    0x0E20,
    0x0E40,
    0x1640,
    0x1640,
    0x1E40,
    0x1E40,
    0x2640,
    0x2640,
    0x2E40,
    0x2E60,
    0x3660,
    0x3660,
    0x3E60,
    0x3E60,
    0x3E60,
    0x4660,
    0x4660,
    0x4E60,
    0x4E80,
    0x5680,
    0x5680,
    0x5E80,
    0x5E80,
    0x6680,
    0x6680,
    0x6E80,
    0x6EA0,
    0x76A0,
    0x76A0,
    0x7EA0,
    0x7EA0,
    0x86A0,
    0x86A0,
    0x8EA0,
    0x8EC0,
    0x96C0,
    0x96C0,
    0x9EC0,
    0x9EC0,
    0xA6C0,
    0xAEC0,
    0xAEC0,
    0xB6E0,
    0xB6E0,
    0xBEE0,
    0xBEE0,
    0xC6E0,
    0xC6E0,
    0xCEE0,
    0xCEE0,
    0xD6E0,
    0xD700,
    0xDF00,
    0xDEE0,
    0xDEC0,
    0xDEA0,
    0xDE80,
    0xDE80,
    0xE660,
    0xE640,
    0xE620,
    0xE600,
    0xE5E0,
    0xE5C0,
    0xE5A0,
    0xE580,
    0xE560,
    0xE540,
    0xE520,
    0xE500,
    0xE4E0,
    0xE4C0,
    0xE4A0,
    0xE480,
    0xE460,
    0xEC40,
    0xEC20,
    0xEC00,
    0xEBE0,
    0xEBC0,
    0xEBA0,
    0xEB80,
    0xEB60,
    0xEB40,
    0xEB20,
    0xEB00,
    0xEAE0,
    0xEAC0,
    0xEAA0,
    0xEA80,
    0xEA60,
    0xEA40,
    0xF220,
    0xF200,
    0xF1E0,
    0xF1C0,
    0xF1A0,
    0xF180,
    0xF160,
    0xF140,
    0xF100,
    0xF0E0,
    0xF0C0,
    0xF0A0,
    0xF080,
    0xF060,
    0xF040,
    0xF020,
    0xF800,
};

float get_point(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y);

long loopTime, startTime, endTime, fps;

// cover 1 --> 3, 32 * 24 --> 94 * 70
void cover3()
{
    uint8_t x, y;
    uint16_t pos = 0;
    float x_step = 0.0;
    float y_step = 0.0;
    float pixel_value = 0.0;
    float max_step = 0;

    for (y = 0; y < ROWS - 1; y++)
    {
        for (x = 0; x < COLS - 1; x++)
        {
            pixel_value = get_pixels(x, y);
            x_step = (get_pixels(x + 1, y) - pixel_value) / 3.0;
            pos = 3 * x + COLS_3 * (y * 3);
            pixels_3[pos] = pixel_value + x_step;
            pixels_3[pos + 1] = pixels_3[pos] + x_step;
            pixels_3[pos + 2] = pixels_3[pos + 1] + x_step;
        }
    }

    for (y = 0; y < ROWS - 1; y++)
    {
        for (x = 0; x < COLS_3; x++)
        {
            pixel_value = get_pixels_3(x, y);
            y_step = (get_pixels_3(x, y + 1) - pixel_value) / 3.0;
            pixels_3[(3 * y + 1) * COLS_3 + x] = pixel_value + y_step;
            pixels_3[(3 * y + 2) * COLS_3 + x] = pixel_value + 2 * y_step;
        }
    }
}

void drawpixels(float *p, uint8_t rows, uint8_t cols)
{
    int colorTemp;
    Serial.printf("%f, %f\r\n", mintemp, maxtemp);

    for (int y = 0; y < rows; y++)
    {
        for (int x = 0; x < cols; x++)
        {
            float val = get_point(p, rows, cols, x, y);

            if (val >= maxtemp)
            {
                colorTemp = maxtemp;
            }
            else if (val <= mintemp)
            {
                colorTemp = mintemp;
            }
            else
            {
                colorTemp = val;
            }

            uint8_t colorIndex = map(colorTemp, mintemp, maxtemp, 0, 255);
            colorIndex = constrain(colorIndex, 0, 255); // 0 ~ 255
            // draw the pixels!
            img.drawPixel(x, y, camColors[colorIndex]);
        }
    }

    img.drawCircle(COLS_3 / 2, ROWS_3 / 2, 5,
                   TFT_WHITE); // update center spot icon
    img.drawLine(COLS_3 / 2 - 8, ROWS_3 / 2, COLS_3 / 2 + 8, ROWS_3 / 2,
                 TFT_WHITE); // vertical line
    img.drawLine(COLS_3 / 2, ROWS_3 / 2 - 8, COLS_3 / 2, ROWS_3 / 2 + 8,
                 TFT_WHITE); // horizontal line
    img.setCursor(COLS_3 / 2 + 6, ROWS_3 / 2 - 12);
    img.setTextColor(TFT_WHITE);
    img.printf("%.2fC", get_point(p, rows, cols, cols / 2, rows / 2));
    img.pushSprite(0, 0);

    msg.fillScreen(TFT_BLACK);
    msg.setTextColor(TFT_YELLOW);
    msg.setCursor(11, 3);
    msg.print("min tmp");
    msg.setCursor(13, 15);
    msg.printf("%.2fC", min_v);
    msg.setCursor(11, 27);
    msg.print("max tmp");
    msg.setCursor(13, 39);
    msg.printf("%.2fC", max_v);

    msg.pushSprite(COLS_3, 10);
}

void setup()
{
    M5.begin();

    // Increase I2C clock speed to 450kHz
    Wire.begin(0, 26, 400000UL); // HAT
    M5.Lcd.setRotation(1);
    Wire1.begin(32, 33); // grove

    while (!tcs.begin((uint8_t)41U, &Wire1))
    { //如果color unit未能初始化
        M5.Lcd.println("No TCS34725 found ... check your connections");
        M5.Lcd.drawString("No Found sensor.", 50, 100, 4);
        delay(1000);
    }
    tcs.setIntegrationTime(TCS34725_INTEGRATIONTIME_154MS); // Sets the integration time for the TC34725.  设置TC34725的集成时间
    tcs.setGain(TCS34725_GAIN_4X);                          // Adjusts the gain on the TCS34725.  调整TCS34725上的增益

    // use show tmp bitmap
    img.createSprite(COLS_3, ROWS_3);
    // use show tmp data
    msg.createSprite(160 - COLS_3, ROWS_3 - 10);

    Serial.println("M5StickC MLX90640 IR Camera");

    // Get device parameters - We only have to do this once
    int status;
    uint16_t eeMLX90640[832]; // 32 * 24 = 768

    status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
    if (status != 0)
        Serial.println("Failed to load system parameters");

    status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
    if (status != 0)
        Serial.println("Parameter extraction failed");

    // Setting MLX90640 device at slave address 0x33 to work with 16Hz refresh
    MLX90640_SetRefreshRate(0x33, 0x05);

    MLX90640_SetResolution(0x33, 0x03);

    // Display bottom side colorList and info
    M5.Lcd.fillScreen(TFT_BLACK);

    for (int icol = 0; icol <= 127; icol++)
    {
        M5.Lcd.drawRect(icol, 72, 1, 8, camColors[icol * 2]);
    }

    BLEDevice::init("M5ColorAndThermal");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pColorService = pServer->createService(COLOR_SERVICE_UUID);
    pColorChara = pColorService->createCharacteristic(COLOR_CHARA_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    BLEService *pThermalService = pServer->createService(THERMAL_SERVICE_UUID);
    pThermalChara = pThermalService->createCharacteristic(THERMAL_CHARA_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
    // Create a BLE Descriptor
    pColorChara->addDescriptor(new BLE2902());
    pThermalChara->addDescriptor(new BLE2902());
    pColorService->start();
    pThermalService->start();
    Serial.println("BLE setup");

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(COLOR_SERVICE_UUID);
    pAdvertising->addServiceUUID(THERMAL_SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x0); // set value to 0x00 to not advertise this parameter
    BLEDevice::startAdvertising();
    Serial.println("Waiting a client connection to notify...");
}

void loop()
{
    loopTime = millis();
    startTime = loopTime;

    M5.update();
    if (M5.Axp.GetBtnPress() == 0x02)
    {
        esp_restart();
    }

    // Reset settings
    if (M5.BtnA.pressedFor(1000))
    {
        mintemp = min_v - 1;
        maxtemp = max_v + 1;
    }

    // Set Min Value - SortPress //
    if (M5.BtnA.wasPressed())
    {
        if (mintemp <= 0)
        {
            mintemp = maxtemp - 1;
        }
        else
        {
            mintemp--;
        }
    }

    if (M5.BtnB.wasPressed())
    {
        maxtemp = maxtemp + 1;
    }

    uint16_t mlx90640Frame[834];

    uint16_t clear, red, green, blue;
    tcs.getRawData(&red, &green, &blue, &clear); // Reads the raw red, green, blue and clear channel values.  读取原始的红、绿、蓝和清晰的通道值

    // Figure out some basic hex code for visualization.  生成对应的十六进制代码
    uint32_t sum = clear;
    float r, g, b;
    r = red;
    r /= sum;
    g = green;
    g /= sum;
    b = blue;
    b /= sum;
    r *= 256;
    g *= 256;
    b *= 256;
    // uint16_t _color = color16((int)r, (int)g, (int)b);

    M5.lcd.setCursor(160, 0);               // Place the cursor at (0,0).  将光标固定在(0,0)
    M5.lcd.fillRect(160, 0, 80, 80, BLACK); // Fill the screen with a black rectangle.  将屏幕填充黑色矩形
    Serial.println(clear);
    M5.Lcd.print("C:");
    M5.Lcd.println(clear);
    M5.lcd.setCursor(160, 10);
    Serial.println(red);
    M5.Lcd.print("R:");
    M5.Lcd.println(red);
    M5.lcd.setCursor(160, 20);
    Serial.println(green);
    M5.Lcd.print("G:");
    M5.Lcd.println(green);
    M5.lcd.setCursor(160, 30);
    Serial.println(blue);
    M5.Lcd.print("B:");
    M5.Lcd.println(blue);
    M5.lcd.setCursor(160, 40);
    M5.Lcd.print("0x");
    M5.Lcd.print((int)r, HEX);
    M5.Lcd.print((int)g, HEX);
    M5.Lcd.print((int)b, HEX);
    M5.lcd.setCursor(0, 0); // Place the cursor at (0,0).  将光标固定在(0,0)

    // those fun get tmp array, 32*24, 5fps
    for (byte x = 0; x < speed_setting; x++)
    {
        int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
        if (status < 0)
        {
            Serial.print("GetFrame Error: ");
            Serial.println(status);
        }

        float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640);
        float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);
        float tr = Ta - TA_SHIFT; // Reflected temperature based on the sensor
                                  // ambient temperature
        float emissivity = 0.95;
        MLX90640_CalculateTo(
            mlx90640Frame, &mlx90640, emissivity, tr,
            reversePixels); // save pixels temp to array (pixels)
        int mode = MLX90640_GetCurMode(MLX90640_address);
        MLX90640_BadPixelsCorrection(mlx90640.brokenPixels, reversePixels, mode,
                                     &mlx90640);
    }

    // if want to send tmp array to other device, you can write fun in here:
    // xxx.send(reversePixels, 32*24);
    char str[20];
    for (int i = 0; i < 768; i++)
    {
        if (deviceConnected)
        {
            sprintf(str, "%d, %g", i, reversePixels[i]);
            pThermalChara->setValue(str);
            pThermalChara->notify();
            delay(10);
        }
        Serial.printf("[px%d]%g\n", i, reversePixels[i]);
    }
    if (deviceConnected)
    {
        sprintf(str, "%3.2f,%3.2f,%3.2f", r, g, b);
        Serial.printf("[RGB]: %s\n", str);
        pColorChara->setValue(str);
        pColorChara->notify();
    }
    Serial.printf("[RGB]: %f,%f,%f\n", r, g, b);

    // disconnecting
    if (!deviceConnected && oldDeviceConnected)
    {
        delay(500);                  // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected)
    {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }

    // Reverse image (order of Integer array)
    for (int x = 0; x < pixelsArraySize; x++)
    {
        if (x % COLS == 0)
        {
            for (int j = 0 + x, k = (COLS - 1) + x; j < COLS + x; j++, k--)
            {
                pixels[j] = reversePixels[k];
            }
        }
    }

    max_v = mintemp;
    min_v = maxtemp;

    for (int itemp = 0; itemp < sizeof(pixels) / sizeof(pixels[0]); itemp++)
    {
        if (pixels[itemp] > max_v)
        {
            max_v = pixels[itemp];
        }
        if (pixels[itemp] < min_v)
        {
            min_v = pixels[itemp];
        }
    }

    // cover pixels to pixels_3
    cover3();

    // show tmp image
    drawpixels(pixels_3, ROWS_3, COLS_3);

    loopTime = millis();
    endTime = loopTime;
    fps = 1000 / (endTime - startTime);

    delay(DELAY_MS);
}