#include "APIReturn.h"
#include "ActionHandler.h"
#include "ScriptClient.h"
#include "esp_wifi.h"
#include <Arduino.h>
#include <Contract.h>
#include <Crypto.h>
#include <Util.h>
#include <Web3.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <WifiServer.h>
#include <functional>
#include <string>
#include <vector>
#include <trezor/rand.h>

#define FRONT_LEFT 14
#define REAR_RIGHT 27
#define REAR_LEFT 13
#define FRONT_RIGHT 12

#define REAR_LEFT_NEG 33
#define FRONT_RIGHT_NEG 32
#define FRONT_LEFT_NEG 26
#define REAR_RIGHT_NEG 25

#define CAR_CONTRACT "0xD5bE89AD0F3Ce89e72BAf4211A980F8367E2D668"

const char *ssid = "<SSID>";
const char *password = "<PASSWORD>";

const char *INFURA_HOST = "rinkeby.infura.io";
const char *INFURA_PATH = "/v3/c7df4c29472d4d54a39f7aa78f146853";
int wificounter = 0;
const char *seedWords[] = {"Dogecoin", "Bitcoin", "Litecoin", "EOS", "Stellar", "Nervos", "Huobi", "Qtum", "Dash", "Cardano", "Ethereum", 0};
string currentChallenge;
ActionHandler *actionHandler;
Web3 web3(INFURA_HOST, INFURA_PATH);

std::string validAddress = "0xD5bE89AD0F3Ce89e72BAf4211A980F8367E2D668";

const char *apiRoute = "api/";
enum APIRoutes
{
    api_getChallenge,
    api_checkSignature, //returns session key
    api_allForward,
    api_leftForward,   //require session key
    api_rightForward,  //require session key
    api_leftBackward,  //require session key
    api_rightBackward, //require session key
    api_leftTurn,
    api_rightTurn,
    api_backwards,
    api_checkToken,
    api_End
};
std::map<std::string, APIRoutes> s_apiRoutes;

void Initialize()
{
    s_apiRoutes["getChallenge"] = api_getChallenge;
    s_apiRoutes["checkSignature"] = api_checkSignature;
    s_apiRoutes["allForward"] = api_allForward;
    s_apiRoutes["leftForward"] = api_leftForward;
    s_apiRoutes["rightForward"] = api_rightForward;
    s_apiRoutes["leftBackward"] = api_leftBackward;
    s_apiRoutes["rightBackward"] = api_rightBackward;
    s_apiRoutes["turnLeft"] = api_leftTurn;
    s_apiRoutes["turnRight"] = api_rightTurn;
    s_apiRoutes["backwards"] = api_backwards;
    s_apiRoutes["checkToken"] = api_checkToken;
    s_apiRoutes["end"] = api_End;
}

#define USE_STATIC_IP 1
WiFiServer server(8080);
//use these if you want to expose your DApp server to outside world via portforwarding
// IPAddress ipStat(192, 168, 1, 180);
// IPAddress gateway(192, 168, 1, 1);
// IPAddress subnet(255, 255, 255, 0);
// IPAddress dns(192, 168, 1, 1);
IPAddress ipStat(192, 168, 1, 102);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 1, 1);

void setup_wifi();
void stop();
void generateSeed(BYTE *buffer);
void updateChallenge();
bool QueryBalance(string *userAddress);
void forward(int duration);
void leftForward(int duration);
void rightForward(int duration);
void turnLeft(int duration);
void turnRight(int duration);
void backwards(int duration);

void setup()
{
    pinMode(FRONT_LEFT, OUTPUT);
    pinMode(FRONT_RIGHT, OUTPUT);
    pinMode(REAR_LEFT, OUTPUT);
    pinMode(REAR_RIGHT, OUTPUT);
    pinMode(FRONT_LEFT_NEG, OUTPUT);
    pinMode(FRONT_RIGHT_NEG, OUTPUT);
    pinMode(REAR_LEFT_NEG, OUTPUT);
    pinMode(REAR_RIGHT_NEG, OUTPUT);

    Initialize();

    actionHandler = new ActionHandler(12);

    stop();

    Serial.begin(115200);

    setup_wifi();
    updateChallenge();
}

void toLowerCase(string &data)
{
    std::for_each(data.begin(), data.end(), [](char & c) {
		c = ::tolower(c);
	});
}

void handleAPI(APIReturn *apiReturn, ScriptClient *client)
{
    //handle the returned API call
    Serial.println(apiReturn->apiName.c_str());
    std::string address;

    switch (s_apiRoutes[apiReturn->apiName.c_str()])
    {
    case api_getChallenge:
        client->print(currentChallenge.c_str());
        break;
    case api_checkSignature:
    {
        address = Crypto::ECRecoverFromPersonalMessage(&apiReturn->params["sig"], &currentChallenge);
        boolean hasToken = QueryBalance(&address);
        updateChallenge(); //generate a new challenge after each check
        if (hasToken)
        {
            client->print("pass");
            validAddress = address;
        }
        else
        {
            client->print("fail: doesn't have token");
        }
    }
    break;
    case api_allForward:
        address = apiReturn->params["addr"];
        toLowerCase(address);
        if (address.compare(validAddress) >= 0)
        {
            forward(1000);
            client->print("going forward");
        }
        else
        {
            client->print("no access");
        }
        break;
    case api_backwards:
        address = apiReturn->params["addr"];
        toLowerCase(address);
        if (address.compare(validAddress) >= 0)
        {
            backwards(1000);
            client->print("going backwards");
        }
        else
        {
            client->print("no access");
        }
        break;
    case api_leftTurn:
        address = apiReturn->params["addr"];
        toLowerCase(address);
        if (address.compare(validAddress) >= 0)
        {
            turnLeft(1000);
            client->print("going left");
        }
        else
        {
            client->print("no access");
        }
        break;
    case api_rightTurn:
        address = apiReturn->params["addr"];
        toLowerCase(address);
        if (address.compare(validAddress) >= 0)
        {
            turnRight(1000);
            client->print("going right");
        }
        else
        {
            client->print("no access");
        }
        break;
    case api_checkToken:
        client->print(validAddress.c_str());
        break;
    case api_leftBackward:
        break;
    case api_rightBackward:
        break;
    default:
        break;
    }
}

void loop()
{
    setup_wifi(); //ensure we maintain a connection. This may cause the server to reboot periodically, if it loses connection

    WiFiClient c = server.available(); // Listen for incoming clients
    ScriptClient *client = (ScriptClient *)&c;

    if (*client)
    {
        client->checkClientAPI(apiRoute, &handleAPI); //method handles connection close etc.
    }

    actionHandler->CheckEvents(millis());
}

void stop()
{
    Serial.println("Stop");
    digitalWrite(FRONT_LEFT, LOW);
    digitalWrite(FRONT_RIGHT, LOW);
    digitalWrite(REAR_LEFT, LOW);
    digitalWrite(REAR_RIGHT, LOW);
    digitalWrite(FRONT_LEFT_NEG, LOW);
    digitalWrite(FRONT_RIGHT_NEG, LOW);
    digitalWrite(REAR_LEFT_NEG, LOW);
    digitalWrite(REAR_RIGHT_NEG, LOW);
}

void forward(int duration)
{
    digitalWrite(FRONT_LEFT, HIGH);
    digitalWrite(FRONT_RIGHT, HIGH);
    digitalWrite(REAR_LEFT, HIGH);
    digitalWrite(REAR_RIGHT, HIGH);

    digitalWrite(FRONT_LEFT_NEG, LOW);
    digitalWrite(FRONT_RIGHT_NEG, LOW);
    digitalWrite(REAR_LEFT_NEG, LOW);
    digitalWrite(REAR_RIGHT_NEG, LOW);

    actionHandler->RemoveAllEvents();
    actionHandler->AddCallback(duration, &stop);
}

void backwards(int duration)
{
    digitalWrite(FRONT_LEFT, LOW);
    digitalWrite(FRONT_RIGHT, LOW);
    digitalWrite(REAR_LEFT, LOW);
    digitalWrite(REAR_RIGHT, LOW);

    digitalWrite(FRONT_LEFT_NEG, HIGH);
    digitalWrite(FRONT_RIGHT_NEG, HIGH);
    digitalWrite(REAR_LEFT_NEG, HIGH);
    digitalWrite(REAR_RIGHT_NEG, HIGH);

    actionHandler->RemoveAllEvents();
    actionHandler->AddCallback(duration, &stop);
}

void turnRight(int duration)
{
    digitalWrite(FRONT_LEFT, HIGH);
    digitalWrite(FRONT_RIGHT, LOW);
    digitalWrite(REAR_LEFT, HIGH);
    digitalWrite(REAR_RIGHT, LOW);

    digitalWrite(FRONT_LEFT_NEG, LOW);
    digitalWrite(FRONT_RIGHT_NEG, HIGH);
    digitalWrite(REAR_LEFT_NEG, LOW);
    digitalWrite(REAR_RIGHT_NEG, HIGH);

    actionHandler->RemoveAllEvents();
    actionHandler->AddCallback(duration, &stop);
}

void turnLeft(int duration)
{
    digitalWrite(FRONT_LEFT, LOW);
    digitalWrite(FRONT_RIGHT, HIGH);
    digitalWrite(REAR_LEFT, LOW);
    digitalWrite(REAR_RIGHT, HIGH);

    digitalWrite(FRONT_LEFT_NEG, HIGH);
    digitalWrite(FRONT_RIGHT_NEG, LOW);
    digitalWrite(REAR_LEFT_NEG, HIGH);
    digitalWrite(REAR_RIGHT_NEG, LOW);

    actionHandler->RemoveAllEvents();
    actionHandler->AddCallback(duration, &stop);
}

void leftForward(int duration)
{
    digitalWrite(FRONT_LEFT, HIGH);
    digitalWrite(FRONT_LEFT_NEG, LOW);
    digitalWrite(REAR_LEFT, HIGH);
    digitalWrite(REAR_LEFT_NEG, LOW);

    actionHandler->RemoveAllEvents();
    actionHandler->AddCallback(duration, &stop);
}

void leftBackward()
{
    digitalWrite(FRONT_LEFT, LOW);
    digitalWrite(FRONT_LEFT_NEG, HIGH);
    digitalWrite(REAR_LEFT, LOW);
    digitalWrite(REAR_LEFT_NEG, HIGH);
}

void rightForward()
{
    digitalWrite(FRONT_RIGHT, HIGH);
    digitalWrite(FRONT_RIGHT_NEG, LOW);
    digitalWrite(REAR_RIGHT, HIGH);
    digitalWrite(REAR_RIGHT_NEG, LOW);
}

void rightBackward()
{
    digitalWrite(FRONT_RIGHT, LOW);
    digitalWrite(FRONT_RIGHT_NEG, HIGH);
    digitalWrite(REAR_RIGHT, LOW);
    digitalWrite(REAR_RIGHT_NEG, HIGH);
}

void slowTurnLeft()
{
    digitalWrite(FRONT_LEFT, HIGH);
    digitalWrite(FRONT_RIGHT, LOW);
    digitalWrite(REAR_LEFT, HIGH);
    digitalWrite(REAR_RIGHT, LOW);
}

void slowTurnRight()
{
    digitalWrite(FRONT_LEFT, LOW);
    digitalWrite(FRONT_RIGHT, HIGH);
    digitalWrite(REAR_LEFT, LOW);
    digitalWrite(REAR_RIGHT, HIGH);
}

/* This routine is specifically geared for ESP32 perculiarities */
/* You may need to change the code as required */
/* It should work on 8266 as well */
void setup_wifi()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.persistent(false);
        WiFi.mode(WIFI_OFF);
        WiFi.mode(WIFI_STA);
        //Serial.println(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

#ifdef USE_STATIC_IP
        if (!WiFi.config(ipStat, gateway, subnet, dns, dns))
        {
            Serial.println("STA Failed to configure");
        }
#endif
        WiFi.begin(ssid, password);
    }

    wificounter = 0;
    while (WiFi.status() != WL_CONNECTED && wificounter < 50)
    {
        for (int i = 0; i < 500; i++)
        {
            delay(1);
        }
        Serial.print(".");
        wificounter++;
    }

    if (wificounter >= 10)
    {
        Serial.println("Restarting ...");
        ESP.restart(); //targetting 8266 & Esp32 - you may need to replace this
    }

    server.begin();
    //esp_wifi_set_max_tx_power(34); //save a little power if your unit is near the router. If it's located away then use 78 - max
    //esp_wifi_set_ps(WIFI_PS_MAX_MODEM);

    delay(10);

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("Gateway:");
    Serial.println(WiFi.gatewayIP());
}

void updateChallenge()
{
    //generate a new challenge
    int size = 0;
    while (seedWords[size] != 0)
        size++;
    Serial.println(size);
    char buffer[32];

    int seedIndex = random(0, size);
    currentChallenge = seedWords[seedIndex];
    currentChallenge += "-";
    long challengeVal = random32();
    currentChallenge += itoa(challengeVal, buffer, 16);
}

bool hasValue(BYTE *value, int length)
{
    for (int i = 0; i < length; i++)
    {
        if ((*value++) != 0)
        {
            return true;
        }
    }

    return false;
}

bool QueryBalance(string *userAddress)
{
    bool hasToken = false;
    // transaction
    const char *contractAddr = CAR_CONTRACT;
    Contract contract(&web3, contractAddr);
    string func = "balanceOf(address)";
    string param = contract.SetupContractData(func.c_str(), userAddress);
    string result = contract.ViewCall(&param);

    Serial.println(result.c_str());

    BYTE tokenVal[32];

    //break down the result
    vector<string> *vectorResult = Util::InterpretVectorResult(&result);
    for (auto itr = vectorResult->begin(); itr != vectorResult->end(); itr++)
    {
        Util::ConvertHexToBytes(tokenVal, itr->c_str(), 32);
        if (hasValue(tokenVal, 32))
        {
            hasToken = true;
            Serial.println("Has token");
            break;
        }
    }

    delete (vectorResult);
    return hasToken;
}