#include <M5Stack.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>

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

#define SD_CONFIG SdSpiConfig(TFCARD_CS_PIN, SHARED_SPI,  SD_SCK_MHZ(25))

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

#define DBG_OUTPUT_PORT Serial

// アクセスポイント設定
const char ssid_AP[] = "M5Collector_12345678";  //SSID
const char pass_AP[] = "12345678";  //パスワード(8文字以上)

long fileid = 1;
char filename[32];

WiFiServer server(80);

//-------------------------------------------------
// 初期設定
//-------------------------------------------------
void setup() {
  M5.begin();
  M5.Power.begin();
  Serial.begin(115200);             // シリアル通信設定
  if (!sd.begin(SD_CONFIG)) {
    Serial.println("SD Initialization Failed.");
    return;
  }

  // アクセスポイント設定
  WiFi.softAP(ssid_AP, pass_AP);  //ソフトAP設定

  server.begin();                     //Webサーバー開始

  // シリアル出力
  Serial.printf("\nSSID:%s\n", ssid_AP);//SSID
  Serial.printf("PASS:%s\n", pass_AP);  //PASSWORD
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());      //IPアドレス（配列）
}
//-------------------------------------------------
// メイン
//-------------------------------------------------
void loop() {
  WiFiClient client = server.available();
  String buf = "";
  
  if (client) {
    sprintf(filename, "/gps_log_%05ld.csv", fileid);
    Serial.println(filename);
    file.open(filename, O_WRONLY | O_CREAT | O_TRUNC);
    
    while (client.connected()) {
      if (client.available() > 0) {
        buf = client.readStringUntil('\r');
        if (buf.endsWith("--END--")) {
          break;
        }
        int len = file.print(buf);
      }
    }
    client.print("OK\r\n");
    client.stop();

    file.close();
    fileid++;
    
    Serial.println("DATA END.");
  }
}
