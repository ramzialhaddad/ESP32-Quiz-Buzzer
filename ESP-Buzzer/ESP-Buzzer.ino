#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
//#include <Arduino.h>

// Enums
enum MessageType
{
	PAIRING,
	DATA,
};

enum ActionType
{
	STANDBY,
	FLASH,
	READY_TO_RECEIVE
};

// Structs
typedef struct struct_message
{
	uint8_t msgType; // MessageType enum
	uint8_t id;
	uint8_t macAddr[6];
	uint8_t actionType // ActionType enum
} struct_message;

typedef struct struct_pairing
{
	uint8_t msgType;
	uint8_t id;
	uint8_t macAddr[6];
	uint8_t channel;
} struct_pairing;

// MEMORY ALLOCATIONS \\
MessageType messageType;
uint8_t clientMacAddress[6];
esp_now_peer_info_t peer;
int chan;

// Structs
struct_message incomingReadings;
struct_message outgoingSetpoints;
struct_pairing pairingData;

bool isHost;

// ATTACHED HARDWARE STUFF \\
#define LED_PIN 1
#define IS_HOST_PIN 2
//#define IS_BUZZER_PIN 3
#define BUTTON_PIN 4

// ---------- HOST SPECIFIC ---------- \\
uint8_t PairedMacAddresses[20][6];

enum HostStatus
{
	PAIRING,
	STANDBY,
	RECEIVING_BUZZER_RESPONSES,
	WINNER_SELECTION_FLASHING,
};
HostStatus hostStatus = PAIRING;

void OnHostDataSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status){
	Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Last packet delivered successfully" : "Last packet delivery failed!");
}

void OnHostDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len){
	uint8_t type = incomingData[0]; // first byte of the message is the type of the message
	switch(type){
		case DATA:
			if(hostStatus == PAIRING || hostStatus == STANDBY || hostStatus == WINNER_SELECTION_FLASHING)
				return;

			break;
		case PAIRING:
			if(hostStatus != PAIRING)
				return;

			memcpy(&pairingData, incomingData, sizeof(pairingData));

			clientMacAddress[0] = pairingData.macAddr[0];
			clientMacAddress[1] = pairingData.macAddr[1];
			clientMacAddress[2] = pairingData.macAddr[2];
			clientMacAddress[3] = pairingData.macAddr[3];
			clientMacAddress[4] = pairingData.macAddr[4];
			clientMacAddress[5] = pairingData.macAddr[5];

			break;
	}
}

void initHostESP_NOW(){
	if(esp_now_init() != ESP_OK){
		return;
	}

	esp_now_register_send_cb(esp_now_send_cb_t(OnHostDataSent));
	esp_now_register_send_cb(esp_now_send_cb_t(OnHostDataRecv));
}

// -------- END HOST SPECIFIC -------- \\


// ---------- BUZZER SPECIFIC ---------- \\
enum BuzzerStatus
{
	LOOKING_TO_PAIR,
	STANDBY,
	READY_TO_SEND,
	SELECTED_AS_WINNER,
};
BuzzerStatus buzzerStatus = LOOKING_TO_PAIR;

enum PairingStatus
{
	NOT_PAIRED,
	PAIR_REQUEST,
	PAIR_REQUESTED,
	PAIR_PAIRED,
	PAIR_DENIED,
};
PairingStatus pairingStatus = NOT_PAIRED;

void OnBuzzerDataSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status){
	Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Last packet delivered successfully" : "Last packet delivery failed!");
}

void OnBuzzerDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len){
	uint8_t type = incomingData[0]; // first byte of the message is the type of the message
	switch(type){
		case DATA:
	  		break;
		case PAIRING:
	  		break;
	}
}

void initBuzzerESP_NOW(){
	if(esp_now_init() != ESP_OK){
		return;
	}

	esp_now_register_send_cb(esp_now_send_cb_t(OnBuzzerDataSent));
	esp_now_register_send_cb(esp_now_send_cb_t(OnBuzzerDataRecv));
}

// -------- END BUZZER SPECIFIC -------- \\



void setup() {
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	chan = WiFi.channel();

	pinMode(BUTTON_PIN, INPUT_PULLUP);
	pinMode(LED_PIN, OUTPUT);

	pinMode(IS_HOST_PIN, INPUT_PULLUP);
	isHost = digitalRead(IS_HOST_PIN);

	if(isHost){
		initHostESP_NOW();
	}else{
		initBuzzerESP_NOW();
	}
}

void loop() {
  // put your main code here, to run repeatedly:

}
