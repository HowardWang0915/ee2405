#include "mbed.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"

// GLOBAL VARIABLES
WiFiInterface *wifi;
InterruptIn btn2(SW2);
InterruptIn btn3(SW3);
volatile int message_num = 0;
volatile int arrivedcount = 0;
volatile bool closed = false;

const char* topic = "Mbed";

Thread mqtt_thread(osPriorityHigh);
EventQueue mqtt_queue;

void messageArrived(MQTT::MessageData& md) {
      MQTT::Message &message = md.message;
      char msg[300];
      sprintf(msg, "Message arrived: QoS%d, retained %d, dup %d, packetID %d\r\n", message.qos, message.retained, message.dup, message.id);
      printf(msg);
      wait_ms(1000);
      char payload[300];
      sprintf(payload, "Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
      printf(payload);
      ++arrivedcount;
}

void publish_message(MQTT::Client<MQTTNetwork, Countdown>* client) {
      message_num++;
      MQTT::Message message;
      char buff[100];
      sprintf(buff, "QoS0 Hello, Python! #%d", message_num);
      message.qos = MQTT::QOS0;
      message.retained = false;
      message.dup = false;
      message.payload = (void*) buff;
      message.payloadlen = strlen(buff) + 1;
      int rc = client->publish(topic, message);

      printf("rc:  %d\r\n", rc);
      printf("Puslish message: %s\r\n", buff);
}

void close_mqtt() {
      closed = true;
}

int main() {

      wifi = WiFiInterface::get_default_instance();
      if (!wifi) {
            printf("ERROR: No WiFiInterface found.\r\n");
            return -1;
      }


      printf("\nConnecting to %s...\r\n", MBED_CONF_APP_WIFI_SSID);
      int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
      if (ret != 0) {
            printf("\nConnection error: %d\r\n", ret);
            return -1;
      }


      NetworkInterface* net = wifi;
      MQTTNetwork mqttNetwork(net);
      MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

      //TODO: revise host to your ip
      const char* host = "192.168.0.107";
      printf("Connecting to TCP network...\r\n");
      int rc = mqttNetwork.connect(host, 1883);
      if (rc != 0) {
            printf("Connection error.");
            return -1;
      }
      printf("Successfully connected!\r\n");

      MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
      data.MQTTVersion = 3;
      data.clientID.cstring = "Mbed";

      if ((rc = client.connect(data)) != 0){
            printf("Fail to connect MQTT\r\n");
      }
      if (client.subscribe(topic, MQTT::QOS0, messageArrived) != 0){
            printf("Fail to subscribe\r\n");
      }

      mqtt_thread.start(callback(&mqtt_queue, &EventQueue::dispatch_forever));
      btn2.rise(mqtt_queue.event(&publish_message, &client));
      btn3.rise(&close_mqtt);

      int num = 0;
      while (num != 5) {
            client.yield(100);
            ++num;
      }

      while (1) {
            if (closed) break;
            wait(0.5);
      }

      printf("Ready to close MQTT Network......\n");

      if ((rc = client.unsubscribe(topic)) != 0) {
            printf("Failed: rc from unsubscribe was %d\n", rc);
      }
      if ((rc = client.disconnect()) != 0) {
      printf("Failed: rc from disconnect was %d\n", rc);
      }

      mqttNetwork.disconnect();
      printf("Successfully closed!\n");

      return 0;
}