#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>

#ifndef STASSID
#define STASSID "Advictor"  // 公司wifi 2.4G
#define STAPSK "advictor2858@"
//#define STASSID "mario"  // 测试wifi
//#define STAPSK "12345678"
#endif

// 本地端口
uint16_t localPort = 8888;

// 上位机端口
uint16_t remotePort = 8888;

// 接收缓冲区大小固定为8字节
const int MAX_BUFFER_LENGTH = 8;

// 接收缓冲区
uint8_t packetBuffer[MAX_BUFFER_LENGTH] = { 0 };

// 发送缓冲区大小固定224字节
const int STREAM_PACKET_SIZE = 224;

// 发送缓冲区
uint8_t streamBuffer[STREAM_PACKET_SIZE] = { 0 };

// 包计数
uint32_t streamSequence = 0;

// mDNS
const char MDNS_HOSTNAME[] = "ir24-device";
const char MDNS_SERVICE_TYPE[] = "ir24";
const char MDNS_SERVICE_PROTO[] = "udp";

WiFiUDP Udp;

void fillStreamBuffer() {
  // 默认清零，其它字段按协议填充
  for (int i = 0; i < STREAM_PACKET_SIZE; i++) {
    streamBuffer[i] = 0;
  }

  // 第1-4字节固定为 84 6F 0A 00
  streamBuffer[0] = 0x84;
  streamBuffer[1] = 0x6F;
  streamBuffer[2] = 0x0A;
  streamBuffer[3] = 0x00;

  // 第5-8字节放递增包计数(大端字节序)
  streamBuffer[4] = static_cast<uint8_t>((streamSequence >> 24) & 0xFF);
  streamBuffer[5] = static_cast<uint8_t>((streamSequence >> 16) & 0xFF);
  streamBuffer[6] = static_cast<uint8_t>((streamSequence >> 8) & 0xFF);
  streamBuffer[7] = static_cast<uint8_t>(streamSequence & 0xFF);

  streamSequence++;
}

void printBufferHex(const uint8_t* buffer, int length) {
  for (int i = 0; i < length; i++) {
    uint8_t b = buffer[i];
    if (b < 0x10) {
      Serial.print('0');
    }
    Serial.print(b, HEX);
    Serial.print(' ');
  }
  Serial.println();
}

void setup() {
  // 以115200波特率初始化串口
  Serial.begin(115200);

  // 初始化WiFi连接
  WiFi.mode(WIFI_STA);
  WiFi.hostname(MDNS_HOSTNAME);
  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // 初始化mDNS
  if (!MDNS.begin(MDNS_HOSTNAME)) {
    Serial.println("mDNS responder start failed");
  } else {
    MDNS.addService(MDNS_SERVICE_TYPE, MDNS_SERVICE_PROTO, localPort);
    MDNS.addServiceTxt(MDNS_SERVICE_TYPE, MDNS_SERVICE_PROTO, "device", "24-channel-ir");
    Serial.printf("mDNS service published: _%s._%s.local on port %d\n", MDNS_SERVICE_TYPE, MDNS_SERVICE_PROTO, localPort);
    Serial.printf("mDNS host: %s.local\n", MDNS_HOSTNAME);
  }

  // 初始化UDP
  Udp.begin(localPort);
  Serial.printf("UDP server on port %d\n", localPort);
}

void loop() {
  // 错误检查
  // 如果WIFI是断开的，则直接返回 TODO: 可以考虑重试连接或者重新启动WiFi模块
  if (WL_CONNECTED != WiFi.status()) {
    return;
  }

  MDNS.update();

  // 有数据到来
  int packetSize = Udp.parsePacket();

  if (packetSize > 0) {
    // 读数据
    int n = Udp.read(packetBuffer, MAX_BUFFER_LENGTH - 1);

    if (n <= 0) {
      Serial.println("Error reading UDP packet");
      return;
    }

    packetBuffer[n] = 0;
    Serial.print("Received packet length: ");
    Serial.println(n);

    Serial.println("Received Contents (hex):");
    printBufferHex(reinterpret_cast<const uint8_t*>(packetBuffer), n);

    Serial.print("Sender IP: ");
    Serial.print(Udp.remoteIP());
    Serial.print(", Port: ");
    Serial.println(Udp.remotePort());


    fillStreamBuffer();
    Serial.print("Target IP: ");
    Serial.print(Udp.remoteIP());
    Serial.print(", Port: ");
    Serial.println(Udp.remotePort());
    Serial.println("Sent content (hex):");
    printBufferHex(streamBuffer, STREAM_PACKET_SIZE);
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(streamBuffer, STREAM_PACKET_SIZE);
    Udp.endPacket();
  }
}
