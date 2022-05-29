#include "M5Atom.h"
#include "TinyGPS++.h"
#include <time.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include <SdFat.h>

// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 3

/*
  Change the value of SD_CS_PIN if you are using SPI and
  your hardware does not use the default value, SS.
  Common values are:
  Arduino Ethernet shield: pin 4
  Sparkfun SD shield: pin 8
  Adafruit SD shields and modules: pin 10
*/

// SDCARD_SS_PIN is defined for the built-in SD on some boards.
#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SS;
#else  // SDCARD_SS_PIN
// Assume built-in SD is used.
const uint8_t SD_CS_PIN = SDCARD_SS_PIN;
#endif  // SDCARD_SS_PIN


// Try to select the best SD card configuration.
#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI,  SD_SCK_MHZ(16))
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI)
#endif  // HAS_SDIO_CLASS

#if SD_FAT_TYPE == 0
SdFat sd;
File file;
#elif SD_FAT_TYPE == 1
SdFat32 sd;
File32 file;
#elif SD_FAT_TYPE == 2
SdExFat sd;
ExFile file;
#elif SD_FAT_TYPE == 3
SdFs sd;
FsFile file;
#else  // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE

#define DATA_SEND_SUCCESS 0
#define DATA_SEND_FAILURE 1
#define SEND_BUF_SIZE  4096

// 子機ID
#define DONGLE_ID "DONGLE_001"

HardwareSerial GPSRaw(2);
TinyGPSPlus gps;
char write_str[128];

// 親機のWi-Fi SSIDおよびパスワード
// このSSIDとパスワードで公開されている親機に接続してデータを送信する。
const char* ssid     = "M5Collector_12345678";
const char* password = "12345678";

struct tm gps_time;
struct tm *gps_time_local;
time_t gps_time_tt;

// 待機間隔 (ms)
long measureInterval = 1000;

// ファイルへの記録間隔 (s) (このタイミングで得たデータのみ記録)
long writeToSDInterval = 5;
long writeIntervalCount = 10;  // 1000/measureInterval * writeToSDInterval の値

// ファイルに記録するか
bool fileWriteFlag = true;

// 保存するファイル名
const char *fname = "/gps_log.csv";

void writeData(char *str) {
  // SDカードへの書き込み処理（ファイル追加モード）
  if (!sd.exists(fname)) {
    file.open(fname, O_RDWR | O_CREAT | O_TRUNC);
    file.println("dongle_id,timestamp,latitude,longitude,speed_kmph,course,altitude");
  } else {
    file.open(fname, FILE_WRITE);
  }
  
  file.println(str);
  file.close();
}

static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do
  {
    while (GPSRaw.available())
      gps.encode(GPSRaw.read());
  } while (millis() - start < ms);
}

int sendFileToCollector() {
  // データ回収地点でデータファイルを送信する
  WiFiClient client;
  const char *host = "192.168.4.1";
  const int port = 80;
  int retryCount = 5;
  
  File f;
  unsigned char *databuf;
  databuf = (unsigned char *)malloc(sizeof(unsigned char) * SEND_BUF_SIZE);

  // データ回収地点サーバに接続 (5回接続失敗したら打ち切り)
  while (!client.connect(host, port)) {
    retryCount--;
    if (retryCount < 0) {
      return DATA_SEND_FAILURE;
    }
    delay(1000);
  }

  // LED橙点灯
  M5.dis.drawpix(0, 0xFF9900);
  M5.dis.setBrightness(5);

  // ファイルを読み取り送信
  f.open(fname, FILE_READ);
  long dataLength = f.size();
  long readLength = 0;
  
  // データ  
  while (dataLength - readLength > 0) {
    int len = f.read(databuf, SEND_BUF_SIZE);
    client.write(databuf, len);
    readLength += len;
  }
  client.print("\r\n--END--\r\n");
  f.close();
  
  int retcode = DATA_SEND_FAILURE;
  
  while (client.connected()) {
    String line = client.readStringUntil('\r');
    if (line == "OK") {
      retcode = DATA_SEND_SUCCESS;
    } else {
      Serial.println(line);
    }
  }

  client.stop();
  free(databuf);
  return retcode;
}

void setup() {
  M5.begin(true, false, true);
  Serial.begin(115200);

  // Initialize SD library
  SPI.begin(23, 33, 19, -1);
  while (!sd.begin(SD_CONFIG)) {
    Serial.println(F("Failed to initialize SD library"));
    delay(1000);
  }

  // Initialize SD library
  GPSRaw.begin(9600, SERIAL_8N1, 22, -1);

  // ログ記録カウンタ
  writeIntervalCount = 5;

  // データ回収地点のWiFiを探す
  WiFi.begin(ssid, password);
}

void loop() {
  // データ回収地点のWiFiに接続、かつボタンが押された時
  M5.update();

  if (WiFi.status() == WL_CONNECTED) {
    M5.dis.drawpix(0, 0x0000FF);
    M5.dis.setBrightness(3);
  }
  
  if ((WiFi.status() == WL_CONNECTED) && M5.Btn.isPressed()) {    
    if (sendFileToCollector() == DATA_SEND_SUCCESS) {
      // データ送信成功時はLEDを緑点灯
      M5.dis.drawpix(0, 0x32CD32);
      M5.dis.setBrightness(3);
    } else {
      // データ送信失敗時はLEDを赤点灯
      M5.dis.drawpix(0, 0xFF0000);
      M5.dis.setBrightness(3);
    }
  }

  if (gps.location.lat() != 0.0 && gps.location.lng() != 0.0) {
    // 時刻を求める(UTC -> JST)
    gps_time.tm_year = gps.date.year() - 1900;
    gps_time.tm_mon  = gps.date.month() - 1;
    gps_time.tm_mday = gps.date.day();
    gps_time.tm_hour = gps.time.hour() + 9;
    gps_time.tm_min  = gps.time.minute();
    gps_time.tm_sec  = gps.time.second();
      
    gps_time_tt      = mktime(&gps_time);
    gps_time_local   = localtime(&gps_time_tt);
    
    // 出力文字列を作成
    sprintf(write_str, "%s,%04d-%02d-%02dT%02d:%02d:%02d+09:00,%2.6f,%3.6f,%.2f,%.2f,%.2f",
                  DONGLE_ID,
                  gps_time_local->tm_year + 1900,
                  gps_time_local->tm_mon  + 1,
                  gps_time_local->tm_mday,
                  gps_time_local->tm_hour,
                  gps_time_local->tm_min,
                  gps_time_local->tm_sec,
                  gps.location.lat(), gps.location.lng(),
                  gps.speed.kmph(),gps.course.deg(),
                  gps.altitude.meters());
  
    Serial.println(write_str);
  
    // SDカードに出力
    if (fileWriteFlag) {
      writeIntervalCount--;
      if (writeIntervalCount < 0) {
        writeData(write_str);
        // ログ書き込み用カウンタリセット
        writeIntervalCount = 5;
      }
    }
  }
  smartDelay(measureInterval);
  M5.dis.clear();
}
