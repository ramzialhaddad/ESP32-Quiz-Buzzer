#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

esp_now_peer_info_t slave;
int chan;

enum MessageType
{
  PAIRING,
  DATA,
};
MessageType messageType;

int counter = 0;

uint8_t clientMacAddress[6];

uint8_t winnerMacAddress[6];
bool FoundWinner = false;

// Structure example to receive data
// Must match the sender structure
typedef struct struct_message
{
  uint8_t msgType;
  uint8_t id;
  uint8_t macAddr[6];
} struct_message;

typedef struct struct_pairing
{ // new structure for pairing
  uint8_t msgType;
  uint8_t id;
  uint8_t macAddr[6];
  uint8_t channel;
} struct_pairing;

struct_message incomingReadings;
struct_message outgoingSetpoints;
struct_pairing pairingData;

void readMacAddress()
{
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK)
  {
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                  baseMac[0], baseMac[1], baseMac[2],
                  baseMac[3], baseMac[4], baseMac[5]);
  }
  else
  {
    Serial.println("Failed to read MAC address");
  }
}

// ---------------------------- esp_ now -------------------------
void printMAC(const uint8_t *mac_addr)
{
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
}

bool addPeer(const uint8_t *peer_addr)
{ // add pairing
  memset(&slave, 0, sizeof(slave));
  const esp_now_peer_info_t *peer = &slave;
  memcpy(slave.peer_addr, peer_addr, 6);

  slave.channel = chan; // pick a channel
  slave.encrypt = 0;    // no encryption
  // check if the peer exists
  bool exists = esp_now_is_peer_exist(slave.peer_addr);
  if (exists)
  {
    // Slave already paired.
    Serial.println("Already Paired");
    return true;
  }
  else
  {
    esp_err_t addStatus = esp_now_add_peer(peer);
    if (addStatus == ESP_OK)
    {
      // Pair success
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

// callback when data is sent
void OnDataSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status)
{
  char macStr[18];
  Serial.print("Last Packet Send Status: ");
  Serial.print(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success to " : "Delivery Fail to ");
  // Copies the receiver mac address to a string
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
  Serial.println();
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
  Serial.print(len);
  Serial.println(" bytes of new data received.");
  uint8_t type = incomingData[0]; // first message byte is the type of message
  switch (type)
  {
  case DATA: // the message is data type
    memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));

    if (incomingReadings.id == 0)
      return;

    if (!FoundWinner)
    {
      FoundWinner = true;
      memcpy(winnerMacAddress, incomingReadings.macAddr, sizeof(winnerMacAddress));

      outgoingSetpoints.msgType = DATA;
      outgoingSetpoints.id = 0;
      
      memcpy(outgoingSetpoints.macAddr, winnerMacAddress, sizeof(winnerMacAddress));

      esp_now_send(NULL, (uint8_t *)&outgoingSetpoints, sizeof(outgoingSetpoints));
    }
    break;

  case PAIRING: // the message is a pairing request
    memcpy(&pairingData, incomingData, sizeof(pairingData));
    Serial.println(pairingData.msgType);
    Serial.println(pairingData.id);
    Serial.print("Pairing request from MAC Address: ");
    printMAC(pairingData.macAddr);
    Serial.print(" on channel ");
    Serial.println(pairingData.channel);

    clientMacAddress[0] = pairingData.macAddr[0];
    clientMacAddress[1] = pairingData.macAddr[1];
    clientMacAddress[2] = pairingData.macAddr[2];
    clientMacAddress[3] = pairingData.macAddr[3];
    clientMacAddress[4] = pairingData.macAddr[4];
    clientMacAddress[5] = pairingData.macAddr[5];

    if (pairingData.id > 0)
    { // do not replay to server itself
      if (pairingData.msgType == PAIRING)
      {
        pairingData.id = 0; // 0 is server
        // Server is in AP_STA mode: peers need to send data to server soft AP MAC address
        esp_wifi_get_mac(WIFI_IF_STA, pairingData.macAddr);
        Serial.print("Pairing MAC Address: ");
        printMAC(clientMacAddress);
        pairingData.channel = chan;
        Serial.println(" send response");
        esp_err_t result = esp_now_send(clientMacAddress, (uint8_t *)&pairingData, sizeof(pairingData));
        addPeer(clientMacAddress);
      }
    }
    break;
  }
}

void initESP_NOW()
{
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}

void setup()
{
  // Initialize Serial Monitor
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  Serial.print("Server MAC Address: ");
  readMacAddress();

  // Set the device as a Station and Soft Access Point simultaneously
  chan = WiFi.channel();
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());

  initESP_NOW();
}

void loop()
{
  // static unsigned long lastEventTime = millis();
  // static const unsigned long EVENT_INTERVAL_MS = 1000;
  // if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
  //   lastEventTime = millis();
  //   readDataToSend();
  //   esp_now_send(NULL, (uint8_t *) &outgoingSetpoints, sizeof(outgoingSetpoints));
  // }
}