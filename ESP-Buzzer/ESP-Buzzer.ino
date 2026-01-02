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
	uint8_t actionType; // ActionType enum
} struct_message;

typedef struct struct_pairing
{
	uint8_t msgType;
	uint8_t id;
	uint8_t macAddr[6];
	uint8_t channel;
} struct_pairing;

// Networking Stuff \\

MessageType messageType;
uint8_t clientMacAddress[6];
esp_now_peer_info_t peer;
int chan;

// Structs
struct_message incomingReadings;
struct_message outgoingSetpoints;
struct_pairing pairingData;

// Global Vars
bool isHost;
unsigned long appointmentTime = 0;
bool ledOn = false;

// ATTACHED HARDWARE STUFF \\

#define LED_PIN 2
#define IS_HOST_PIN 25
// #define IS_BUZZER_PIN 3
#define BUTTON_PIN 26

// ---------- HOST SPECIFIC ---------- \\

uint8_t PairedMacAddresses[20][6];

enum HostStatus
{
	HOST_PAIRING,
	HOST_STANDBY,
	RECEIVING_BUZZER_RESPONSES,
	WINNER_SELECTION_FLASHING,
};
HostStatus hostStatus = HOST_PAIRING;
volatile bool FoundWinner = false;

void OnHostDataSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status)
{
	Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Last packet delivered successfully" : "Last packet delivery failed!");
}

void OnHostDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
	uint8_t type = incomingData[0]; // first byte of the message is the type of the message
	Serial.println("Received something!");
	switch (type)
	{
	case DATA:
		Serial.println("It was data!");
		if (hostStatus == HOST_PAIRING || hostStatus == HOST_STANDBY || hostStatus == WINNER_SELECTION_FLASHING)
			return;
		if (hostStatus == RECEIVING_BUZZER_RESPONSES)
		{
			memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));

			if (incomingReadings.id == 0)
				return;

			if (!FoundWinner)
			{
				FoundWinner = true;
				memcpy(clientMacAddress, incomingReadings.macAddr, sizeof(clientMacAddress));

				outgoingSetpoints.msgType = DATA;
				outgoingSetpoints.id = 0;
				outgoingSetpoints.actionType = YOU_ARE_WINNER;

				memcpy(outgoingSetpoints.macAddr, clientMacAddress, sizeof(clientMacAddress));

				esp_now_send(NULL, (uint8_t *)&outgoingSetpoints, sizeof(outgoingSetpoints));
				hostStatus = WINNER_SELECTION_FLASHING;
			}
		}
		break;
	case PAIRING:
		Serial.println("It was PAIRING");
		if (hostStatus != HOST_PAIRING)
			return;

		memcpy(&pairingData, incomingData, sizeof(pairingData));

		clientMacAddress[0] = pairingData.macAddr[0];
		clientMacAddress[1] = pairingData.macAddr[1];
		clientMacAddress[2] = pairingData.macAddr[2];
		clientMacAddress[3] = pairingData.macAddr[3];
		clientMacAddress[4] = pairingData.macAddr[4];
		clientMacAddress[5] = pairingData.macAddr[5];

		if (pairingData.id > 0)
		{
			if (pairingData.msgType == PAIRING)
			{
				pairingData.id = 0;
				esp_wifi_get_mac(WIFI_IF_STA, pairingData.macAddr);
				pairingData.channel = chan;
				esp_err_t result = esp_now_send(clientMacAddress, (uint8_t *)&pairingData, sizeof(pairingData));
				HostAddPeer(clientMacAddress);
				Serial.println("Added peer!");
			}
		}
		break;
	}
}

bool HostAddPeer(const uint8_t *peer_addr)
{ // add pairing
	memset(&peer, 0, sizeof(peer));
	memcpy(peer.peer_addr, peer_addr, 6);

	peer.channel = chan;
	peer.encrypt = 0;
	bool exists = esp_now_is_peer_exist(peer.peer_addr);
	if (exists)
	{
		Serial.println("Already Paired");
		return true;
	}
	else
	{
		esp_err_t addStatus = esp_now_add_peer(&peer);
		if (addStatus == ESP_OK)
		{
			Serial.println("Pair success");
			return true;
		}
		else
		{
			Serial.println("Pair failed");
			return false;
		}
	}
}

void initHost()
{
	if (esp_now_init() != ESP_OK)
	{
		return;
	}

	esp_now_register_send_cb(esp_now_send_cb_t(OnHostDataSent));
	esp_now_register_recv_cb(esp_now_recv_cb_t(OnHostDataRecv));

	attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), HostButtonHandler, FALLING);
}

// -------- END HOST SPECIFIC -------- \\


// ---------- BUZZER SPECIFIC ---------- \\

enum BuzzerStatus
{
	LOOKING_TO_PAIR,
	BUZZER_PAIRED_STANDBY,
	BUZZER_FLASH,
	BUZZER_STANDBY,
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

void OnBuzzerDataSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status)
{
	Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Last packet delivered successfully" : "Last packet delivery failed!");
}

void OnBuzzerDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
	uint8_t type = incomingData[0]; // first byte of the message is the type of the message
	bool isUs = true;
	Serial.println("Received something");
	switch (type)
	{
	case DATA:
		Serial.println("It was data!");
		if (buzzerStatus == LOOKING_TO_PAIR) // We aren't paired with the host, anything here isn't for us
			return;

		memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
		for (int x = 0; x < 6; x++)
		{
			if (incomingReadings.macAddr[x] != clientMacAddress[x])
			{
				isUs = false;
			}
		}

		switch (incomingReadings.actionType)
		{
		case YOU_ARE_WINNER:
			if (isUs)
			{
				buzzerStatus = SELECTED_AS_WINNER;
				appointmentTime = 0;
			}
			else
				buzzerStatus = BUZZER_STANDBY;
			break;
		case STANDBY:
			buzzerStatus = BUZZER_STANDBY;
			break;
		case READY_TO_RECEIVE:
			buzzerStatus = READY_TO_SEND;
			digitalWrite(LED_PIN, HIGH);
			break;
		case FLASH:
			if (isUs)
			{
				buzzerStatus = BUZZER_FLASH;
				appointmentTime = 0;
			}

			break;
		}

		break;
	case PAIRING:
		Serial.println("It was PAIRING!");
		memcpy(&pairingData, incomingData, sizeof(pairingData));
		if (pairingData.id == 0)
		{
			BuzzerAddPeer(pairingData.macAddr, pairingData.channel);
			pairingStatus = PAIR_PAIRED;
			Serial.println("PAIRED!!! LETS GOOOOO");
			buzzerStatus = BUZZER_PAIRED_STANDBY;
		}
		break;
	}
}

void initBuzzer()
{
	if (esp_now_init() != ESP_OK)
	{
		Serial.println("Error!");
		return;
	}

	esp_now_register_send_cb(esp_now_send_cb_t(OnBuzzerDataSent));
	esp_now_register_recv_cb(esp_now_recv_cb_t(OnBuzzerDataRecv));

	attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), BuzzerButtonHandler, FALLING);
}

void autoPairing()
{
	Serial.println("Running autoPairing");
	Serial.print("Pairing Status:");
	Serial.println(pairingStatus);
	uint8_t serverAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	switch (pairingStatus)
	{
	case NOT_PAIRED:
	case PAIR_REQUEST:
		ESP_ERROR_CHECK(esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE));

		pairingData.msgType = PAIRING;
		pairingData.id = 1;
		pairingData.channel = chan;

		if (esp_wifi_get_mac(WIFI_IF_STA, pairingData.macAddr) != ESP_OK)
			Serial.println("BAD HAPPENED!");

		BuzzerAddPeer(serverAddress, chan);
		if (esp_now_send(serverAddress, (uint8_t *)&pairingData, sizeof(pairingData)) != ESP_OK)
			Serial.println("Error occured!");
		pairingStatus = PAIR_REQUESTED;
		break;
	case PAIR_REQUESTED:
		chan++;
		if (chan > 11) // 11 for NA, 13 for EU (according to https://randomnerdtutorials.com/esp-now-auto-pairing-esp32-esp8266/)
			chan = 1;
		pairingStatus = PAIR_REQUEST;
		break;
	case PAIR_PAIRED:
		break;
	case PAIR_DENIED:
		break;
	}
}

void BuzzerAddPeer(const uint8_t *mac_addr, uint8_t chan)
{
	ESP_ERROR_CHECK(esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE));
	esp_now_del_peer(mac_addr);
	memset(&peer, 0, sizeof(esp_now_peer_info_t));
	peer.channel = chan;
	peer.encrypt = false;
	memcpy(peer.peer_addr, mac_addr, sizeof(uint8_t[6]));
	if (esp_now_add_peer(&peer) != ESP_OK)
	{
		return;
	}
}

// -------- END BUZZER SPECIFIC -------- \\


void setup()
{
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	chan = WiFi.channel();

	while (!WiFi.STA.started())
	{
		delay(100);
	}

	pinMode(BUTTON_PIN, INPUT_PULLUP);
	pinMode(LED_PIN, OUTPUT);

	pinMode(IS_HOST_PIN, INPUT_PULLUP);
	isHost = !digitalRead(IS_HOST_PIN);

	if (isHost)
	{
		initHost();
		Serial.println("I am host!");
	}
	else
	{
		initBuzzer();
		unsigned long previous = 0;
		while (pairingStatus != PAIR_PAIRED)
		{
			if (millis() - previous > 1000)
			{
				previous = millis();
				autoPairing();
			}
		}
		digitalWrite(LED_PIN, HIGH);
		Serial.println("I am buzzer!!");
	}
}

void HostButtonHandler()
{
	switch (hostStatus)
	{
	case HOST_PAIRING:
		hostStatus = HOST_STANDBY;

		outgoingSetpoints.msgType = DATA;
		outgoingSetpoints.id = 0;
		outgoingSetpoints.actionType = STANDBY;

		esp_now_send(NULL, (uint8_t *)&outgoingSetpoints, sizeof(outgoingSetpoints));
		break;
	case HOST_STANDBY:
		hostStatus = RECEIVING_BUZZER_RESPONSES;

		outgoingSetpoints.msgType = DATA;
		outgoingSetpoints.id = 0;
		outgoingSetpoints.actionType = READY_TO_RECEIVE;

		esp_now_send(NULL, (uint8_t *)&outgoingSetpoints, sizeof(outgoingSetpoints));

		break;
	case RECEIVING_BUZZER_RESPONSES:
		// Cancel receiving requests??
		break;
	case WINNER_SELECTION_FLASHING:
		FoundWinner = false;
		hostStatus = HOST_STANDBY;

		outgoingSetpoints.msgType = DATA;
		outgoingSetpoints.id = 0;
		outgoingSetpoints.actionType = STANDBY;
		
		esp_now_send(NULL, (uint8_t *)&outgoingSetpoints, sizeof(outgoingSetpoints));
		break;
	}
}

void BuzzerButtonHandler()
{
	if (buzzerStatus != READY_TO_SEND)
		return;

	buzzerStatus = BUZZER_STANDBY;

	outgoingSetpoints.id = 1;
	outgoingSetpoints.actionType = CAN_I_BE_WINNER;
	esp_wifi_get_mac(WIFI_IF_STA, pairingData.macAddr);

	esp_now_send(NULL, (uint8_t *)&outgoingSetpoints, sizeof(outgoingSetpoints));
}

void loop()
{
	ledFlash();
}

void ledFlash()
{
	if (millis() < appointmentTime)
		return;

	if (isHost)
	{
		switch (hostStatus)
		{
		case HOST_PAIRING:
			appointmentTime = millis() + 250;
			digitalWrite(LED_PIN, ledOn = !ledOn);
			break;
		case HOST_STANDBY:
			appointmentTime = 0 - 1;
			digitalWrite(LED_PIN, ledOn = false);
			break;
		}
	}
	else
	{
		switch (buzzerStatus)
		{
		case BUZZER_PAIRED_STANDBY:
			appointmentTime = millis() + 500;
			digitalWrite(LED_PIN, ledOn = !ledOn);
			break;
		case STANDBY:
			appointmentTime = 0 - 1;
			digitalWrite(LED_PIN, ledOn = false);
			break;
		case BUZZER_FLASH:
			appointmentTime = millis() + 500;
			digitalWrite(LED_PIN, ledOn = !ledOn);
			break;
		case SELECTED_AS_WINNER:
			appointmentTime = millis() + 250;
			digitalWrite(LED_PIN, ledOn = !ledOn);
			break;
		}
	}
}
