#include "BLEDevice.h"
//#include "BLEScan.h"
#include "Arduino.h"
#include <U8g2lib.h>
#include <Wire.h>

U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// #define DEBUG
// The remote service we wish to connect to.
static BLEUUID serviceUUID(BLEUUID((uint16_t)0x1816));
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID(BLEUUID((uint16_t)0x2A5B));
// static BLEUUID service_cadence_UUID(BLEUUID((uint16_t)0x1816));
// static BLEUUID char_cadence_UUID(BLEUUID((uint16_t)0x2A5B));

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

//定义全局变量
uint16_t revlution_counts;
uint16_t last_revlution_counts;
uint16_t revlutions_timestamp;
uint16_t last_revlutions_timestamp;
//此处更改为999，否则开机会闪现00
uint16_t cadence = 999;
char cadence_string[4] = "---";
unsigned long longtime_noupdate_timestamp=0;

static void notifyCallback(
	BLERemoteCharacteristic* pBLERemoteCharacteristic,
	uint8_t* pData,
	size_t length,
	bool isNotify) {
	// Serial.print("Notify callback for characteristic ");
	// Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
	// Serial.print(" of data length ");
	// Serial.println(length);
	// Serial.print("data: ");
	// Serial.write(pData, length);
	// Serial.println();
	//踏频计的信息很奇葩，5个数，头一个没用，第二三个代表圈数，第四五个代表时间
	revlution_counts = pData[2]*256+pData[1];
	revlutions_timestamp = pData[4]*256+pData[3];
	//通过计算其差来计算踏频
	if(revlution_counts != last_revlution_counts){
		uint16_t revlution_delta = revlution_counts - last_revlution_counts;
		uint16_t revlutions_timesdelta = revlutions_timestamp - last_revlutions_timestamp;
		cadence = uint16_t(60.0*revlution_delta/(revlutions_timesdelta/1000.0));
		last_revlution_counts = revlution_counts;
		last_revlutions_timestamp = revlutions_timestamp;
		Serial.print(cadence);
		Serial.println("rpm");
		longtime_noupdate_timestamp = millis();
	}
	#ifdef DEBUG
	Serial.print(revlution_counts);Serial.print("s");
	Serial.print(revlutions_timestamp);Serial.println("ms");
	#endif
}

class MyClientCallback : public BLEClientCallbacks {
	void onConnect(BLEClient* pclient) {
	}

	void onDisconnect(BLEClient* pclient) {
		connected = false;
		#ifdef DEBUG
		Serial.println("onDisconnect");
		#endif
	}
};

bool connectToServer() {
	#ifdef DEBUG
	Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
	#endif
    
    BLEClient*  pClient  = BLEDevice::createClient();
	#ifdef DEBUG
    Serial.println(" - Created client");
	#endif
    pClient->setClientCallbacks(new MyClientCallback());
    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
	#ifdef DEBUG
    Serial.println(" - Connected to server");
	#endif
    pClient->setMTU(517); //set client to request maximum MTU from server (default is 23 otherwise)
  
    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
		#ifdef DEBUG
		Serial.print("Failed to find our service UUID: ");
		Serial.println(serviceUUID.toString().c_str());
		#endif
		pClient->disconnect();
		return false;
    }
	#ifdef DEBUG
    Serial.println(" - Found our service");
	#endif

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
		#ifdef DEBUG
		Serial.print("Failed to find our characteristic UUID: ");
		Serial.println(charUUID.toString().c_str());
		#endif
		pClient->disconnect();
		return false;
    }
	#ifdef DEBUG
    Serial.println(" - Found our characteristic");
	#endif
    // Read the value of the characteristic.
    if(pRemoteCharacteristic->canRead()) {
		std::string value = pRemoteCharacteristic->readValue();
		#ifdef DEBUG
		Serial.print("The characteristic value was: ");
		Serial.println(value.c_str());
		#endif
    }

    if(pRemoteCharacteristic->canNotify())
      pRemoteCharacteristic->registerForNotify(notifyCallback);

    connected = true;
    return true;
}
/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
	#ifdef DEBUG
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
	#endif
    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
		BLEDevice::getScan()->stop();
		myDevice = new BLEAdvertisedDevice(advertisedDevice);
		doConnect = true;
		doScan = true;
    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

void setup() {
	Wire.setPins(6, 7);
	Wire.begin();
	u8g2.begin();  

//蓝牙连接是阻塞的，也就是开机屏幕不亮，很慌，挪到这里，先显示点什么。
	u8g2.firstPage();
	do {
		u8g2.drawHLine(0,0,128);
		u8g2.drawHLine(0,63,128);
		u8g2.drawVLine(0,0,64);
		u8g2.drawVLine(127,0,64);
		u8g2.drawHLine(1,1,127);
		u8g2.drawHLine(0,62,128);
		u8g2.drawVLine(1,1,64);
		u8g2.drawVLine(126,1,64);
		u8g2.drawHLine(2,2,127);
		u8g2.drawHLine(2,61,128);
		u8g2.drawVLine(2,2,64);
		u8g2.drawVLine(125,2,64);
		u8g2.setFont(u8g2_font_bytesize_tf);
		u8g2.drawStr(12,24,"Cadence Meter");
		u8g2.setFont(u8g2_font_bytesize_tf);
		u8g2.drawStr(12,48,"Connecting...");
	} while ( u8g2.nextPage() );


	Serial.begin(115200);
	#ifdef DEBUG
	Serial.println("Starting Arduino BLE Client application...");
	#endif
	BLEDevice::init("");

	// Retrieve a Scanner and set the callback we want to use to be informed when we
	// have detected a new device.  Specify that we want active scanning and start the
	// scan to run for 5 seconds.
	BLEScan* pBLEScan = BLEDevice::getScan();
	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
	pBLEScan->setInterval(1349);
	pBLEScan->setWindow(449);
	pBLEScan->setActiveScan(true);
	pBLEScan->start(5, false);
} // End of setup.


// This is the Arduino main loop function.
void loop() {

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
	if (doConnect == true) {
		if (connectToServer()) {
			#ifdef DEBUG
			Serial.println("We are now connected to the BLE Server.");
			#endif
		} else {
			#ifdef DEBUG
			Serial.println("We have failed to connect to the server; there is nothin more we will do.");
			#endif
		}
	doConnect = false;
	}
    if (!connected){
		BLEDevice::getScan()->start(5);  // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
	}

	//转换成3个数字
	cadence_string[2] = 0x30 + cadence%10;
	cadence_string[1] = 0x30 + (cadence/10)%10;
	cadence_string[0] = 0x30 + (cadence/100)%10;
	//如果是2位数，就不显示第一位的0
	if(cadence < 100){
		cadence_string[0] = 0x20 ;
	}
	//踏频是有限的，不可能太大，太大肯定是启停瞬间的bug
	if(cadence > 200){
		cadence_string[2] = 0x2D;
		cadence_string[1] = 0x2D;
		cadence_string[0] = 0x2D;
	}	
	//失去连接用符号显示
	if(!connected){
		cadence_string[2] = 0x2D;
		cadence_string[1] = 0x2D;
		cadence_string[0] = 0x2D;
	}
	//长时间不更新也用符号显示,5S不转一圈，就停止更新
	if((millis()-longtime_noupdate_timestamp)>5000){
		cadence_string[2] = 0x2D;
		cadence_string[1] = 0x2D;
		cadence_string[0] = 0x2D;
	}

	u8g2.firstPage();
	do {
		u8g2.drawHLine(0,0,128);
		u8g2.drawHLine(0,63,128);
		u8g2.drawVLine(0,0,64);
		u8g2.drawVLine(127,0,64);
		u8g2.drawHLine(1,1,127);
		u8g2.drawHLine(0,62,128);
		u8g2.drawVLine(1,1,64);
		u8g2.drawVLine(126,1,64);
		u8g2.drawHLine(2,2,127);
		u8g2.drawHLine(2,61,128);
		u8g2.drawVLine(2,2,64);
		u8g2.drawVLine(125,2,64);
		//字体不全，无法显示“---”
		if(0x2D == cadence_string[0]){
			u8g2.setFont(u8g2_font_fub42_tf);
			u8g2.drawStr(16,48,cadence_string);

		}
		else{
			u8g2.setFont(u8g2_font_7Segments_26x42_mn);
			u8g2.drawStr(3,53,cadence_string);
		}
		u8g2.setFont(u8g2_font_12x6LED_tf);
		u8g2.drawStr(100,53,"rpm");
	} while ( u8g2.nextPage() );

} // End of loop


//ESP32-C3基础测试
// #include <Arduino.h>
// #define LED_PIN 8

// void setup() {
// 	pinMode(LED_PIN,OUTPUT);
// 	Serial.begin(115200);
// }

// void loop(){
// 	Serial.println("hello,esp32-c3");
// 	digitalWrite(LED_PIN, HIGH);
// 	delay(1000);
// 	digitalWrite(LED_PIN, LOW);
// 	delay(1000);   

// }

//ESP32-C3基础测试OLED
// #include <Arduino.h>
// #include <U8g2lib.h>
// #include <Wire.h>

// #define LED_PIN 8

// U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// void setup(void) {
// 	//没有这一句，ESP32C3不工作
// 	Wire.setPins(6, 7);
// 	Wire.begin();
// 	// u8g2.setI2CAddress(0x78);
// 	u8g2.begin();  
// 	pinMode(LED_PIN,OUTPUT);
// 	Serial.begin(115200);
// }

// void loop(void) {
// 	u8g2.firstPage();
// 	do {
// 		u8g2.drawHLine(0,0,10);
// 		u8g2.drawHLine(0,31,10);

// 		u8g2.setFont(u8g2_font_ncenB10_tr);
// 		u8g2.drawStr(0,24,"Hello World!");
// 	} while ( u8g2.nextPage() );
// 		Serial.println("hello,esp32-c3");
// 		digitalWrite(LED_PIN, HIGH);
// 		delay(1000);
// 		digitalWrite(LED_PIN, LOW);
// 		delay(1000);  
// }