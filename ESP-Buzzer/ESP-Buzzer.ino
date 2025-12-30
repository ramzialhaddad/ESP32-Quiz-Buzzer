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
	READY_TO_RECEIVE,
	YOU_ARE_WINNER,
	CAN_I_BE_WINNER
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
volatile struct_message incomingReadings;
volatile struct_message outgoingSetpoints;
volatile struct_pairing pairingData;

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
volatile HostStatus hostStatus = PAIRING;
volatile bool FoundWinner = false;

void OnHostDataSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status){
	Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Last packet delivered successfully" : "Last packet delivery failed!");
}

void OnHostDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len){
	uint8_t type = incomingData[0]; // first byte of the message is the type of the message
	switch(type){
		case DATA:
			if(hostStatus == PAIRING || hostStatus == STANDBY || hostStatus == WINNER_SELECTION_FLASHING)
				return;
			if(hostStatus == RECEIVING_BUZZER_RESPONSES){
				memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));

				if(incomingReadings.id == 0)
					return;

				if(!FoundWinner){
					FoundWinner == true;
					memcpy(winnerMacAddress, incomingReadings.macAddr, sizeof(winnerMacAddress));

					outgoingSetpoints.msgType = DATA;
					outgoingSetpoints.id = 0;
					outgoingSetpoints.actionType = YOU_ARE_WINNER;

					memcpy(outgoingSetpoints.macAddr, winnerMacAddress, sizeof(winnerMacAddress));

					esp_now_send(NULL, (uint8_t *)&outgoingSetpoints, sizeof(outgoingSetpoints));
				}
			}
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

			if(pairingData.id > 0){
				if(pairingData.msgType == PAIRING){
					pairingData.id = 0;
					esp_wifi_get_mac(WIFI_IF_STA, pairingData.macAddr);
					pairingData.channel = chan;
					esp_err_t result = esp_now_send(clientMacAddress, (uint8_t *)&pairingData, sizeof(pairingData));
					addPeer(clientMacAddress);
					Serial.println("Added peer!");
				}
			}
			break;
	}
}

void initHost(){
	if(esp_now_init() != ESP_OK){
		return;
	}

	esp_now_register_send_cb(esp_now_send_cb_t(OnHostDataSent));
	esp_now_register_send_cb(esp_now_send_cb_t(OnHostDataRecv));

	attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), HostButtonHandler, FALLING);

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
			if(buzzerStatus == LOOKING_TO_PAIR) // We aren't paired with the host, anything here isn't for us
				return;

			memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
			bool isUs = true;
			for (int x = 0; x < 6; x++)
			{
				if (inData.macAddr[x] != clientMacAddress[x]){
					isUs = false;
				}
			}

			switch(incomingReadings.actionType){
				case YOU_ARE_WINNER:
					if(isUs)
						buzzerStatus = SELECTED_AS_WINNER;
					else
						buzzerStatus = STANDBY;
					break;
				case STANDBY:
					if(isUs)
						buzzerStatus = STANDBY;
					break;
				case READY_TO_RECEIVE:
					if(isUs)
						buzzerStatus = READY_TO_SEND;
					break;
				case FLASH:
					if(isUs)
						buzzerStatus = STANDBY; // TODO: Flashing logic
					break;
			}

			break;
		case PAIRING:
			memcpy(&pairingData, incomingData, sizeof(pairingData));
			if(pairingData.id == 0){
				addPeer(pairingData.macAddr, pairingData.channel);
				pairingStatus = PAIR_PAIRED;
			}
			break;
	}
}

void initBuzzer(){
	if(esp_now_init() != ESP_OK){
		return;
	}

	esp_now_register_send_cb(esp_now_send_cb_t(OnBuzzerDataSent));
	esp_now_register_send_cb(esp_now_send_cb_t(OnBuzzerDataRecv));

	attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), BuzzerButtonHandler, FALLING);

}

PairingStatus autoPairing(){
	switch(pairingStatus){
		case NOT_PAIRED:
		case PAIR_REQUEST:
			ESP_ERROR_CHECK(esp_wifi_set_set_channel(channel, WIFI_SECOND_CHAN_NONE));
			if(esp_now_init != ESP_OK){
				return;
			}
			pairingData.msgType = PAIRING;
			pairingData.id = BOARD_ID;
			pairingData.channel = channel;

			esp_wifi_get_mac(WIFI_IF_STA, pairingData.macAddr);

			addPeer(serverAddress, channel);
			esp_now_send(serverAddress, (uint8_t *)&pairingData, sizeof(pairingData));
			pairingStatus = PAIR_REQUESTED;
			break;
		case PAIR_REQUESTED:
			channel++;
			if(channel > 11) // 11 for NA, 13 for EU (according to https://randomnerdtutorials.com/esp-now-auto-pairing-esp32-esp8266/)
				channel = 1;
			pairingStatus = PAIR_REQUEST;
			break;
		case PAIR_PAIRED:
			break;
		case PAIR_DENIED:
			break;		
	}
	return pairingStatus;
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
		initHost();
	}else{
		initBuzzer();
	}

	ulong previous = 0;
	while(pairingStatus != PAIR_PAIRED){
		if(millis() - previous > 1000){
			previous = millis();
			autoPairing();
		}
	}
}

void HostButtonHandler(){
	// TODO fill this out
}

void BuzzerButtonHandler(){
	// TODO fill this out
	if(buzzerStatus != READY_TO_SEND)
		return;

	buzzerStatus = STANDBY;

	outgoingSetpoints.id = 1;
	outgoingSetpoints.actionType = CAN_I_BE_WINNER;
	esp_wifi_get_mac(WIFI_IF_STA, pairingData.macAddr);

	esp_now_send(NULL, (uint8_t *)&outgoingSetpoints, sizeof(outgoingSetpoints));
}

void loop() {
	// put your main code here, to run repeatedly:
}
