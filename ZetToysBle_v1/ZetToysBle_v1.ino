#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_NAME        "ZetToys001"
// https://www.uuidgenerator.net/
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
//Konfiguracja dla APK.
//joy1[D, X] to joystick po lewej - w przykładzie jest sterowanie serwem dla x i silnikiem DC dla y
//joy2[Y, Z] to joystick po prawej - w przykładzie jest sterowanie dwoma silnikami DC - np. wyjście Y podłączone do obrotnicy drabiny a Z do wysuwania jej
//buttons[A=opis 1,B=opis 2,C=opis 3] to przyciski + nazwy wyświetlane w aplikacji
//Jezeli nie ma w konfigu deklaracji, aplikacja nie wyswietla tych kontrolek
#define CONFIG "joy1[D,X]:joy2[Y,Z]:buttons[A=Klakson,B=Światła]:nazwa=Wóz strażacki"

const int pinA = 2;
const int pinB = 4;
const int pinC = 12;
const int pinD = 13;
const int pinE = 14;
const int pinF = 15;
const int pinX = 16;
const int pinY = 17;
const int pinZ = 18;
const int pinDirX = 19;
const int pinDirY = 21;
const int pinDirZ = 23;

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Tutaj przypisuje wartości do pinów
class GetMessage {
  public:
    static void processValue(const std::string &value) {
      std::string segment;
      size_t start = 0;
      size_t end = value.find(',');
      while (end != std::string::npos) {
        segment = value.substr(start, end - start);
        assignValue(segment);
        start = end + 1;
        end = value.find(',', start);
      }
      assignValue(value.substr(start)); // Ostatni segment po ostatnim przecinku
    }

  private:
    static void assignValue(const std::string &segment) {
      size_t pos = segment.find(':');
      if (pos != std::string::npos) {
        std::string key = segment.substr(0, pos);
        float val = atof(segment.substr(pos + 1).c_str()); // Konwersja string na float
        if (key == "X") {
          digitalWrite(pinDirX, val > 0 ? HIGH : LOW);
          ledcWrite(0, (int)(fabs(val) * 255));
        }
        else if (key == "Y") {
          digitalWrite(pinDirY, val > 0 ? HIGH : LOW);
          ledcWrite(1, (int)(fabs(val) * 255));
        }
        else if (key == "Z") {
          digitalWrite(pinDirZ, val > 0 ? HIGH : LOW);
          ledcWrite(2, (int)(fabs(val) * 255));
        }
        else if (key == "A") digitalWrite(pinA, val > 0 ? HIGH : LOW);
        else if (key == "B") digitalWrite(pinB, val > 0 ? HIGH : LOW);
        else if (key == "C") digitalWrite(pinC, val > 0 ? HIGH : LOW);
        else if (key == "D") ledcWrite(3, (int)((val + 1) * 127.5));
        else if (key == "E") ledcWrite(4, (int)((val + 1) * 127.5));
        else if (key == "F") ledcWrite(5, (int)((val + 1) * 127.5));
      }
    }
};
//Poniej serwer BLE i obsługa połączenia/przyjmowania wiadomości
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Połączono");
      BLEDevice::startAdvertising();
      pCharacteristic->setValue(CONFIG);
      // Wysyłaj konfigurację do podłączonego urządzenia
      pCharacteristic->indicate();
      Serial.println("Config wysłany");
      Serial.println("Start nasłuchu");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Rozłączono");
      ledcWrite(0, 0);
      ledcWrite(1, 0);
      ledcWrite(2, 0);
      ledcWrite(3, 128);
      ledcWrite(4, 128);
      ledcWrite(5, 128);
    }
};
class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic){
    std::string value = pCharacteristic->getValue();
    if(value.length() > 0){
      GetMessage::processValue(value);
      Serial.print("New value: ");
      for(int i=0; i < value.length(); i++){
        Serial.print(value[i]);
      }
      Serial.println();
    }
  }
  void onRead(BLECharacteristic *pCharacteristic) {
    pCharacteristic->setValue(CONFIG);
    pCharacteristic->indicate();
    Serial.println("Wysłano config");
  }
};
void setup() {
  Serial.begin(115200);
  BLEDevice::init(SERVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );
  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  Serial.println("Oczekiwanie na podłączenie aplikacji...");
  // Konfiguracja kanałów PWM
  ledcSetup(0, 5000, 8);
  ledcAttachPin(pinX, 0);
  ledcSetup(1, 5000, 8);
  ledcAttachPin(pinY, 1);
  ledcSetup(2, 5000, 8);
  ledcAttachPin(pinZ, 2);
  ledcSetup(3, 5000, 8);
  ledcAttachPin(pinD, 3);
  ledcSetup(4, 5000, 8);
  ledcAttachPin(pinE, 4);
  ledcSetup(5, 5000, 8);
  ledcAttachPin(pinF, 5);
  // Piny OUT
  pinMode(pinA, OUTPUT);
  pinMode(pinB, OUTPUT);
  pinMode(pinC, OUTPUT);
  pinMode(pinDirX, OUTPUT);
  pinMode(pinDirY, OUTPUT);
  pinMode(pinDirZ, OUTPUT);
  ledcWrite(0, 0);
  ledcWrite(1, 0);
  ledcWrite(2, 0);
  ledcWrite(3, 128);
  ledcWrite(4, 128);
  ledcWrite(5, 128);
}
void loop() {
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        Serial.println("Oczekiwanie na podłączenie aplikacji...");
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
}
