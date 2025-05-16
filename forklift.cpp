#include <WiFi.h>

#include <SPIFFS.h>

#include <HTTPClient.h>

#include <CRC32.h>

#include "esp_task_wdt.h"

#include <ArduinoJson.h>

#include <esp_sleep.h>

#include <Preferences.h>

#include <WebServer.h>

#include <PubSubClient.h>

#include <Update.h>  // OTA update

 

 

 

// MQTT Broker Ayarları

const char* mqtt_server = "172.16.5.2";

//const char* mqtt_server = "89.19.13.34";

const int mqtt_port = 1883;

const char* mqtt_topic = "ats/acc";

const char* Version_Control = "Forklix_Esp32v.1.0.4";  //EKLENDİ

 

int OTA_FLAG = 0;

// Bağlantı nesneleri

WiFiClient espClient;

PubSubClient mqttClient(espClient);

 

 

Preferences preferences;

//bool credentialsReceived = false;

 

WebServer server(80);

#define WAKEUP_PIN 5

 

// #define ssid "realme GT Master Edition" //Kontrolmatik

// #define password "qwert123." //Kontrolmatik1*

 

String ssid{};

String password{};

String deveui{};

bool enableAp = true;

//const char *apSSID = "ESP32_Setup";

//const char *apPassword = "12345678";

 

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

String CredentialsToAscii(String& r) {

  String retVal{};

  for (int i = 0; i < r.length(); i += 2) {

    String hexChar = r.substring(i, i + 2);

    char asciiChar = (char)strtol(hexChar.c_str(), NULL, 16);  // Hex to ASCII

    retVal += asciiChar;                                       // Sonuç stringine ekle

  }

 

  return retVal;

}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

 

 

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

// void handleRoot() {

//     server.send(200, "text/html", "<form action='/set' method='POST'><input name='ssid' placeholder='SSID'><input name='pass' placeholder='Password'><input type='submit'></form>");

// }

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

// void handleSet() {

//     if (server.hasArg("ssid") && server.hasArg("pass")) {

//         ssid = server.arg("ssid");

//         password = server.arg("pass");

 

//         preferences.begin("wifi", false);

 

//         preferences.putString("ssid", ssid);

//         preferences.putString("password", password);

//         preferences.putBool("enableAp", false);

//         //enableAp = preferences.getBool("enableAp",true);

 

//         preferences.end();

 

 

 

//         credentialsReceived = true;

//         server.send(200, "text/plain", "WiFi credentials received. Restarting...");

//         delay(2000);

//         WiFi.softAPdisconnect(true);

//     } else {

//         server.send(400, "text/plain", "Missing SSID or Password");

//     }

// }

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

 

String serverUrl{};  // Endpoint URL'si

String authToken{};

uint8_t firmFlag = 0;

uint8_t accdataFlag = 0;

bool acc_start = false;

bool wifiFlag = false;

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

StaticJsonDocument<1024> jsonDoc;

// GitHub API ve RAW URL bilgileri

 

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

 

void checkWiFiConnection() {

  // Eğer WiFi bağlantısı yoksa, bağlantıyı tekrar dene

  if (WiFi.status() != WL_CONNECTED) {

    // Serial.println("WiFi disconnected");

    connectToWiFi();  // Bağlanmayı tekrar dene

  }

}

 

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

 

 

 

struct WiFiInfo {

 

  bool found;

  bool wifiCheckFlag = 0;

  int32_t rssi;

  wifi_auth_mode_t auth_mode;

  String tbToken{};

  void printRSSI() const {

 

    Serial.println(rssi);

  }

 

  String getToken() {

 

    const String& payload = loginToThingsBoard();

    tbToken = payload.substring(payload.indexOf(":") + 2, payload.indexOf(",") - 1);

 

    return tbToken;

  }

 

 

private:

  String loginToThingsBoard() {

 

    checkWiFiConnection();

 

    //Serial.println("WiFi Bağlantısı Kuruldu!");

 

    // HTTP İstemcisini kullanarak POST isteği yap

    HTTPClient http;

    http.begin("http://platform.control-ix.com:80/api/auth/login");  // URL'yi girin

    http.addHeader("Content-Type", "application/json");              // Header ekleyin

 

    // JSON Body verisi

    String postData = "{\"username\": \"tenant@thingsboard.org\", \"password\": \"tenant\"}";

 

    int httpResponseCode = http.POST(postData);

 

    String payload = "";  // Cevap burada tutulacak

 

    if (httpResponseCode > 0) {

      //Serial.print("HTTP Response code: ");

      //Serial.println(httpResponseCode);

      payload = http.getString();  // Sunucudan gelen cevabı al

                                   //Serial.println("Gelen Yanıt: ");

                                   //Serial.println(wifi_info.getToken(payload));

    } else {

      //Serial.print("HTTP Request Hatası: ");

      //Serial.println(httpResponseCode);

    }

 

    http.end();      // Bağlantıyı kapat

    return payload;  // Gelen yanıtı geri döndür

  }

 

} wifi_info;

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

 

 

 

 

void connectToWiFi() {

  if (WiFi.status() != WL_CONNECTED) {

    WiFi.disconnect();                           // Mevcut bağlantıyı kopart

    WiFi.begin(ssid.c_str(), password.c_str());  // Tekrar bağlanmayı dene

                                                 // Serial.print("Connecting to WiFi");

 

    // Belirli bir süre boyunca tekrar dene (örneğin, 10 saniye)

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {

      delay(500);

      //Serial.print(".");

    }

 

    if (WiFi.status() == WL_CONNECTED) {

      Serial.println("\nWiFi connected");

      wifiFlag = true;

      wifi_info.rssi = WiFi.RSSI();

      wifi_info.found = true;

      wifi_info.printRSSI();

 

    } else {

 

      wifi_info.found = false;

    }

  }

}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

 

const char* HOST = "api.github.com";

const char* REPO_PATH = "/repos/Andorlatch/Esp32_OTA_Test/contents/";

const char* RAW_URL_PREFIX = "https://raw.githubusercontent.com/Andorlatch/Esp32_OTA_Test/main/";

String firmwareFileName{};

String myCRCheck{};

String mySTMCRCheck{};

String fileSizeStr{};

CRC32 crc;

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

 

 

 

void setup() {

 

  disableCore0WDT();

  esp_task_wdt_deinit();

  Serial.begin(19200);

  //--------------------------------------------------------------------------------------------------------------------------------------------------------------------

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  esp_deep_sleep_enable_gpio_wakeup(1 << 5, ESP_GPIO_WAKEUP_GPIO_LOW);

  //--------------------------------------------------------------------------------------------------------------------------------------------------------------------

  preferences.begin("wifi", true);

 

  enableAp = preferences.getBool("enableAp", enableAp);

  ssid = preferences.getString("ssid", "RAK7268CV2_DB3F");               // RAK7268CV2_DB3F Bartu iPhone'u

  password = preferences.getString("password", "controlix002.");  // controlix002.

  deveui = preferences.getString("deveui", "");

 

  preferences.end();

  Serial.println();

  Serial.println();

  //Serial.println(Version_Control);

  //Serial.println("Deger SSID = " + ssid);

  //Serial.println("Deger Password = " + password);

  //Serial.println("Deger DEVEUI = "+deveui);

  deveui.trim();

  serverUrl = "http://platform.control-ix.com/api/v1/" + deveui + "/telemetry";

  //Serial.println(serverUrl);

  //--------------------------------------------------------------------------------------------------------------------------------------------------------------------

 

  // if(enableAp)

  // {

  // WiFi.softAP(apSSID, apPassword);

  // Serial.println("Access Point Started");

 

  // server.on("/", handleRoot);

  // server.on("/set", HTTP_POST, handleSet);

  // server.begin();

  // Serial.println("Web Server Started");

  // Serial.println(WiFi.softAPIP());

  // while (!credentialsReceived) {

  //     server.handleClient();

  // }

  // }

  //--------------------------------------------------------------------------------------------------------------------------------------------------------------------

  // while(deveui.isEmpty()){

  //   String nReq = std::move(Serial.readString());

  //        //Serial.println(nReq);

  //        checkReq(nReq);

  // }

  connectToWiFi();

  /*

  if (WiFi.status() == WL_CONNECTED) {

  String otaUrl = "https://bleylek.github.io/esp32-ota/firmware.bin";

  performOTA(otaUrl);

}*/

 

  mqttClient.setServer(mqtt_server, mqtt_port);

 

  //--------------------------------------------------------------------------------------------------------------------------------------------------------------------

  //Serial.println(wifi_info.getToken());

  // SPIFFS'i başlat

  if (!SPIFFS.begin(true)) {

    Serial.println("SPIFFS Mount Failed");

    return;

  }

  //--------------------------------------------------------------------------------------------------------------------------------------------------------------------

  //Serial.println(wifi_info.getToken());

  //downloadAndSaveFirmware();

  //String fileNamep = firmwareFileName.substring(0,firmwareFileName.length()-4);

  //Serial.println(fileNamep);

  //listFiles("/");

  //checkFirmwareChecksum("/" + firmwareFileName);

 

  //YENİ

  mqttClient.setCallback(mqttCallback);

  mqttClient.publish("ats/version", Version_Control);  // EKLENDİ version gönderiyoruz

}

void reconnect() {

  unsigned long startAttemptTime = millis();

  while (!mqttClient.connected() && millis() - startAttemptTime < 10000) {

    delay(500);

    String clientId = "ESP8266Client-" + String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str())) {

      //Serial.println("Mqtt Connected.");

      mqttClient.subscribe("ats/com");

      // YENİ

      mqttClient.subscribe("ats/com");  // MQTT'den komut alabilmek için abone ol

    } else {

      Serial.println();

      //Serial.print("Hata kodu: ");

      Serial.print(mqttClient.state());

      delay(5000);

    }

  }

}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

void loop() {

  if (wifiFlag == false) {

    connectToWiFi();

  }

 

  if (!mqttClient.connected()) {

    reconnect();

  }

 

  mqttClient.loop();

 

  String nReq = "";

 

  if (Serial.available()) {

    nReq = Serial.readStringUntil('\n');

  }

 

  if (nReq.length() > 0) {

    checkReq(nReq);

  }

 

  if (acc_start) {

    if (Serial.available()) {

      String acc_data = Serial.readStringUntil('\n');

      if (acc_data.length() > 0) {

        // mqttClient.publish("ats/mas", acc_data.c_str());

      }

    }

  }

 

  if (OTA_FLAG) {

    String otaUrl = "https://bleylek.github.io/esp32-ota/firmware.bin";

    performOTA(otaUrl);

    OTA_FLAG = 0;  // OTA bir kez denendi, tekrar denememesi için sıfırla

  }

 

 

  delay(1);

  static bool versionSent = false;

  if (mqttClient.connected() && !versionSent) {

    mqttClient.publish("ats/version", Version_Control);

    versionSent = true;

  }

}

 

 

 

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

void downloadAndSaveFirmware() {

  WiFiClientSecure client;

  client.setInsecure();  // Güvenli olmayan bağlantılara izin ver

 

  // GitHub API'sine bağlan

  if (client.connect(HOST, 443)) {

    client.print(String("GET ") + REPO_PATH + " HTTP/1.1\r\n" + "Host: " + HOST + "\r\n" + "User-Agent: ESP32\r\n" + "Connection: close\r\n\r\n");

 

    String responseBody;

 

    // HTTP yanıtını oku

    while (client.connected() || client.available()) {

      responseBody += client.readString();

    }

    client.stop();

 

    // .bin dosyasının ismini bul

    int startIndex = 0;

    while ((startIndex = responseBody.indexOf("\"name\":\"", startIndex)) >= 0) {

      startIndex += 8;  // "name":" kısmından sonrasına geç

      int endIndex = responseBody.indexOf("\"", startIndex);

      firmwareFileName = responseBody.substring(startIndex, endIndex);

 

      // Dosya adının .bin ile bitip bitmediğini kontrol et

      if (firmwareFileName.endsWith(".bin")) {

        //Serial.println("Found firmware file name: " + firmwareFileName);

        break;  // .bin dosyasının ismini aldıktan sonra döngüyü kır

      }

    }

 

    if (!firmwareFileName.isEmpty()) {

      // Dosya boyutunu "_" karakterinden sonrasını al

      int underscoreIndex = firmwareFileName.lastIndexOf('_');

      if (underscoreIndex > 0) {

        fileSizeStr = firmwareFileName.substring(0, firmwareFileName.indexOf('_'));  // .bin kısmını çıkar

        myCRCheck = firmwareFileName.substring(firmwareFileName.indexOf('_') + 1, underscoreIndex);

        mySTMCRCheck = firmwareFileName.substring(underscoreIndex + 1, firmwareFileName.length() - 4);

        int expectedFileSize = fileSizeStr.toInt();  // Dosya boyutunu int olarak al

        //Serial.println("Expected file size: " + String(expectedFileSize));

 

        bool fileDownloadedCorrectly = false;

        while (!fileDownloadedCorrectly) {

          // Dosyayı indir ve boyutunu kontrol et

          String downloadURL = String(RAW_URL_PREFIX) + firmwareFileName;

          //Serial.println("Downloading firmware from: " + downloadURL);

          int downloadedFileSize = saveFileToSPIFFS(downloadURL, firmwareFileName);

 

          // İndirilen dosya boyutunu kontrol et

          if (downloadedFileSize == expectedFileSize) {

            // Serial.println("File downloaded successfully, size matches.");

            fileDownloadedCorrectly = true;

            checkFirmwareChecksum("/" + firmwareFileName);

            firmFlag = 0;

          } else {

            //Serial.println("File size mismatch. Retrying download...");

            delay(1000);  // İndirmenin yeniden başlaması için kısa bir gecikme ekleyelim

          }

        }

      } else {

        //Serial.println("File name format incorrect, no file size found.");

      }

    } else {

      //Serial.println("No .bin firmware file found.");

    }

  } else {

    //Serial.println("Failed to connect to GitHub API");

  }

}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

int saveFileToSPIFFS(String downloadURL, String fileName) {

  WiFiClientSecure client;

  client.setInsecure();  // Güvenli olmayan bağlantılara izin ver

 

  if (client.connect("raw.githubusercontent.com", 443)) {  // Sunucuya bağlan

    // HTTP GET isteğini gönder

    client.print("GET " + downloadURL + " HTTP/1.1\r\n");

    client.print("Host: raw.githubusercontent.com\r\n");

    client.println("Connection: close\r\n");  // Yanıt sonrası bağlantıyı kapat

    client.println();                         // İstek başlıklarının sonunu belirt

 

    // Yanıtı oku ve SPIFFS'te dosyaya yaz

    File file = SPIFFS.open("/" + fileName, FILE_WRITE);

    if (!file) {

      //Serial.println("Failed to open file for writing");

      return -1;

    }

 

    // HTTP yanıt başlıklarını okuma

    String headers = "";

    bool endOfHeaders = false;

 

    while (client.connected() && !endOfHeaders) {

      if (client.available()) {

        char c = client.read();

        headers += c;

        if (headers.endsWith("\r\n\r\n")) {  // Başlıkların sonunu kontrol et

          endOfHeaders = true;

        }

      }

    }

 

    //Serial.println("HTTP response headers received.");

 

    // Ham verileri okuyup dosyaya yazma

    const size_t bufferSize = 1024;  // Tampon boyutu

    uint8_t buffer[bufferSize];

    int totalBytesRead = 0;

 

    while (client.connected()) {

      if (client.available()) {

        size_t bytesRead = client.readBytes(buffer, bufferSize);

        totalBytesRead += bytesRead;  // İndirilen toplam byte sayısı

        file.write(buffer, bytesRead);

      }

    }

    file.close();

    client.stop();

    //Serial.println("File downloaded and saved to SPIFFS as: " + fileName);

    //Serial.println("Downloaded file size: " + String(totalBytesRead));

    return totalBytesRead;  // İndirilen dosyanın toplam boyutunu geri döndür

  } else {

    //Serial.println("Failed to connect to raw.githubusercontent.com");

    return -1;

  }

}

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

void listFiles(String dir) {

  File root = SPIFFS.open(dir);

  if (!root) {

    //Serial.println("Failed to open directory");

    return;

  }

 

  if (!root.isDirectory()) {

    //Serial.println("Not a directory");

    return;

  }

 

  File file = root.openNextFile();

  while (file) {

    //Serial.print("File Name: ");

    //Serial.println(file.name());

    //Serial.print("File Size: ");

    //Serial.println(file.size());

    const size_t bufferSize = 4096;                  // Buffer size reduced to 1024 bytes

    uint8_t* buffer = (uint8_t*)malloc(bufferSize);  // Allocate buffer on heap

 

    if (buffer == nullptr) {

      //Serial.println("Memory allocation failed");

      return;

    }

 

    // Dosyanın içeriğini yazdır

    //Serial.println("File Content:");

    static uint8_t packageSize = 1;

    while (file.available()) {

      size_t bytesRead = file.read(buffer, bufferSize);  // Buffer'a veri oku

                                                         // checkReq();

                                                         //Serial.println("FT24");

 

      Serial.write(buffer, bytesRead);  // Sadece okunan kadar veriyi gönder

      if (bytesRead != 4096) {

        //Serial.println("FT24");/* --------------------------------------------------------------------------------------------------- */

        firmFlag = 1;

        break;

      } else {

        //Serial.println("FT24");/* --------------------------------------------------------------------------------------------------- */

      }

 

      //PacketOk(); // direkt göndermesini istiyorsan bunu yoruma al.

    }

    //Serial.println("Firmware update finished.");

    file.close();

 

    free(buffer);  // Free allocated memory

  }

}

 

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

void checkFirmwareChecksum(String firmwareFilePath) {

  // Dosyayı SPIFFS'ten aç

  //Serial.println("bakliyor : " + firmwareFilePath);

  File firmwareFile = SPIFFS.open(firmwareFilePath, "r");

  if (!firmwareFile) {

    //Serial.println("Firmware file could not be opened!");

    return;

  }

 

  // CRC32 hesaplayıcıyı başlat (CRC-32/MPEG-2 için)

  CRC32 crc;

 

  const size_t bufferSize = 4096;  // Blok boyutu

  uint8_t buffer[bufferSize];

  size_t bytesRead;

 

  // Dosya tamamıyla okunana kadar döngüye gir

  while ((bytesRead = firmwareFile.read(buffer, bufferSize)) > 0) {

    // CRC32 hesaplayıcıya verileri besle

    crc.update((uint32_t*)buffer, bytesRead / 4);

  }

 

  // Dosya okuma işlemi bitti, CRC32 checksumını al

  uint32_t calculatedChecksum = crc.finalize();  // finalize ile CRC-32 checksum'u elde edilir

 

  // Beklenen checksum ile karşılaştırma (örneğin, bir sabit değer ile)

  uint32_t expectedChecksum = strtoul(myCRCheck.c_str(), NULL, 16);  // Bu değeri firmware'in doğru checksum'ı ile değiştirin

 

  //Serial.print("Calculated Checksum: ");

  //Serial.println(calculatedChecksum, HEX);

  //  Serial.print("myCRCcheck : ");

  //Serial.println(expectedChecksum, HEX);

 

  if (calculatedChecksum == expectedChecksum) {

    //Serial.println("Checksum matches!");

  } else {

    //Serial.println("Checksum does NOT match!");

  }

 

  // Dosyayı kapat

  firmwareFile.close();

}

 

 

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

 

 

void formatFileSystem() {

 

  if (SPIFFS.format()) {

    //Serial.println("SPIFFS formatted successfully.");

    firmFlag = 0;

 

  } else {

    //Serial.println("SPIFFS format failed.");

  }

}

 

 

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

 

void checkReq(String& n_request) {

  if (n_request.indexOf("{") != -1) {

    mqttClient.publish(mqtt_topic, n_request.c_str());

  } else if (n_request.indexOf("AT+FIRM?") != -1) {

    Serial.println("OK");

    formatFileSystem();

    downloadAndSaveFirmware();

    //String fileNamep = firmwareFileName.substring(0,firmwareFileName.length()-4);

    firmFlag = 0;

    delay(1000);

    Serial.println("FIRMINFO:Size:" + fileSizeStr + "/CRC-E:" + myCRCheck + "/CRC-S:" + mySTMCRCheck + "/Version:001");

 

  } else if (n_request.indexOf("AT+ACC_ON") != -1) {

    Serial.println("ACC_ON");

    acc_start = true;

  } else if (n_request.indexOf("AT+ACC_OFF") != -1) {

    Serial.println("ACC_OFF");

    acc_start = false;

  } else if (n_request.indexOf("AT+GETFIRMINFO") != -1) {

    Serial.println("FIRMINFO:Size:" + fileSizeStr + "/CRC-E:" + myCRCheck + "/CRC-S:" + mySTMCRCheck + "/Version:001");

  } else if (n_request.indexOf("AT+SLEEP") != -1) {

    Serial.println("Sleep mode on.");

    esp_deep_sleep_start();

  } else if (n_request.indexOf("AT+RSTSPIFFS") != -1) {

    Serial.println("Reset Firm. from SPIFFS");

    formatFileSystem();

 

  } else if (n_request.indexOf("FIRMINFO+OK") != -1) {

    Serial.println("OK");

    //listFiles("/");

  } else if (n_request.indexOf("SEND+FIRMPACKET") != -1) {

    //Serial.println("PREPARING TO UPDATE");

    if (firmFlag) {

      //Serial.println("FIRM-DONE");

 

    } else listFiles("/");

  } else if (n_request.indexOf("EXT_FLASH:ACC_DATA") != -1) {

    deveui = n_request.substring(19, n_request.length());

    deveui.trim();

    serverUrl = "http://platform.control-ix.com/api/v1/" + deveui + "/telemetry";

    //Serial.println("ACCDATA DAN GELEN DEVEUI *"+deveui+"*ServerUrl -> "+serverUrl);

    preferences.begin("wifi", false);

 

    preferences.putString("deveui", deveui);

 

    preferences.end();

 

    String acc_data{};

    accdataFlag = 1;

    delay(100);

    Serial.println("ACC_DATA_READY");

 

    JsonArray dataArray = jsonDoc.createNestedArray("durations");

    while (Serial.available() > 1 || accdataFlag != 0) {

      acc_data = Serial.readString();

 

      if (acc_data.indexOf("ACC_DATA_Done") != -1) {

        accdataFlag = 1;

        //Serial.println("Write done.");

 

        break;

      } else {

        if (acc_data.isEmpty() != 1) {

          //Serial.print("Data yazildi test: ");

          //Serial.print(acc_data);

          dataArray.add(acc_data);

        }

      }

    }

    //serializeJson(jsonDoc, Serial);

    //Serial.println("Json gonderildi."); /*********************/ JSON DATA

    sendJsonData(jsonDoc);

  } else if (n_request.indexOf("WIFIRESET") != -1) {

 

    preferences.begin("wifi", false);

 

 

    preferences.putBool("enableAp", true);

    preferences.end();

    Serial.println("Wifi reseted to AP mode.");

    ESP.restart();

 

 

  } else if (n_request.indexOf("AT+CREDINFO") != -1) {

 

    delay(100);

    Serial.println("CRED OK");

    String credinfo{ n_request.substring(n_request.indexOf("=") + 5, n_request.length()) };

 

 

    //Serial.println(credinfo);

 

    credinfo = CredentialsToAscii(credinfo);

    // credinfo = credinfo.substring(credinfo.indexOf("00af"),credinfo.length());

    //credinfo.trim();

    //Serial.println(credinfo);

    ssid = credinfo.substring(credinfo.indexOf("\"") + 1, credinfo.indexOf(":") - 1);

    password = credinfo.substring(credinfo.indexOf(":") + 2, credinfo.length() - 1);

    preferences.begin("wifi", false);

 

    preferences.putString("ssid", ssid);

    preferences.putString("password", password);

    preferences.putBool("enableAp", false);

    //enableAp = preferences.getBool("enableAp",true);

 

    preferences.end();

 

    Serial.println("ssid: " + ssid + " password: " + password + " enableAp: " + enableAp);

 

    ESP.restart();

 

  } else if (n_request.indexOf("AT+DEVEUI") != -1) {

 

    deveui = n_request.substring(n_request.indexOf("=") + 1, n_request.length());

    Serial.println("Deveui geldi : " + deveui);

    preferences.begin("wifi", false);

 

    preferences.putString("deveui", deveui);

 

    preferences.end();

 

 

 

  } else {

    //Serial.println("Command not found");

  }

}

 

 

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

 

 

 

void PacketOk() {

  String pReq{};

  while (Serial.available()) {

    pReq = Serial.readString();

    Serial.println(pReq);

  }

 

  while (pReq.indexOf("SEND+FIRMPACKET") == -1) {

    pReq = Serial.readString();

    if (firmFlag)

      Serial.println("FIRM-DONE");

    //Serial.println(pReq);

  }

  // if(pReq.indexOf("SEND+FIRMPACKET") != -1)

  // Serial.println(pReq + String{"31GO31ALIVE"});

}

 

 

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

 

 

void sendJsonData(JsonDocument& jsonDoc) {

  connectToWiFi();  // Wi-Fi bağlantısını kontrol et

 

  WiFiClientSecure client;

  client.setInsecure();  // Sertifika doğrulamasını kapatıyoruz

 

  HTTPClient http;

  http.begin(client, serverUrl);

 

 

  // Eğer yetkilendirme gerekiyorsa

 

 

  // JSON verisini dizeye dönüştür

  String jsonString;

  serializeJson(jsonDoc, jsonString);

 

  // HTTP POST isteğini gönder

  int httpResponseCode = http.POST(jsonString);

 

  if (httpResponseCode > 0) {

    Serial.print("HTTP Response code: ");

    Serial.println(httpResponseCode);

    //String response = http.getString();

    //Serial.println("Server Response: ");

    //Serial.println(response);

  } else {

    Serial.print("Error in HTTP request: ");

    Serial.println(http.errorToString(httpResponseCode));

  }

 

  http.end();  // Bağlantıyı kapat

}

 

 

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------

 

// YENİ

void mqttCallback(char* topic, byte* payload, unsigned int length) {

  String message;

  for (unsigned int i = 0; i < length; i++) {

    message += (char)payload[i];

  }

  message.trim();  // ← bu satır AT+OTA gibi komutları düzgün tanımak için şart

 

 // Serial.print("MQTT mesajı geldi [");

 // Serial.print(topic);

  //Serial.print("]: ");

  //Serial.println(message);

 

  if (String(topic) == "ats/com" && message == "AT+ACC_OFF") {

    Serial.println("ACC_OFF");

    acc_start = false;

  } else if (String(topic) == "ats/com" && message == "AT+ACC_ON") {

    Serial.println("ACC_ON");

    acc_start = true;

  } else if (String(topic) == "ats/com" && message == "AT+OTA") {

    //Serial.println("MQTT ile OTA komutu alındı.");

    OTA_FLAG = 1;

  }

}

 

 

 

// -------------------------------------------------------------------------------------

 

void performOTA(const String& binUrl) {

  if (!mqttClient.connected()) {

    reconnect();

  }

 

  mqttClient.publish("ats/log", "🔁 OTA işlemi başlatılıyor...");

 

  WiFiClientSecure client;

  client.setInsecure();  // Sertifika doğrulamasını devre dışı bırak

 

  HTTPClient http;

  if (!http.begin(client, binUrl)) {

    mqttClient.publish("ats/log", "❌ HTTPClient başlatılamadı.");

    return;

  }

 

  int httpCode = http.GET();

  mqttClient.publish("ats/log", ("🔎 HTTP response code: " + String(httpCode)).c_str());

 

  if (httpCode == HTTP_CODE_OK) {

    int len = http.getSize();

    mqttClient.publish("ats/log", ("📦 Firmware size: " + String(len) + " bytes").c_str());

    WiFiClient* stream = http.getStreamPtr();

 

    if (Update.begin(len)) {

      mqttClient.publish("ats/log", "🛠️ Update.begin başarılı.");

      size_t written = Update.writeStream(*stream);

      mqttClient.publish("ats/log", ("✍️ Yazılan byte: " + String(written)).c_str());

 

      if (written == len && Update.end() && Update.isFinished()) {

        mqttClient.publish("ats/log", "✅ OTA tamamlandı. Cihaz yeniden başlatılıyor...");

        mqttClient.publish("ats/version", Version_Control);

        delay(1000);  // Mesajların gönderilmesi için

        ESP.restart();

      } else {

        mqttClient.publish("ats/log", "❌ OTA başarısız. Yazma veya bitirme hatası.");

      }

    } else {

      mqttClient.publish("ats/log", "❌ Update.begin başarısız.");

    }

  } else {

    mqttClient.publish("ats/log", ("❌ HTTP error: " + String(httpCode)).c_str());

  }

 

  http.end();

}

