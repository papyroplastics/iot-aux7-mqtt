import random
import time
import paho.mqtt.client as mqtt

MQTT_TOPIC = "data/temperature"

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected with result code {reason_code}")

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.connect("127.0.0.1", 1883, 60)
client.loop_start()

while True:
    temperature = random.randrange(0,40)
    msg_bytes = int.to_bytes(temperature, length=4, byteorder='little')
    client.publish(MQTT_TOPIC, msg_bytes)
    time.sleep(0.5)

client.loop_stop()

