import random
import time
import paho.mqtt.client as mqtt

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected with result code {reason_code}")

def on_message(client, userdata, msg):
    print(msg.topic + ": " + str(msg.payload))

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

client.on_connect = on_connect
client.on_message = on_message

client.connect("127.0.0.1", 1883, 60)

client.loop_start()

while True:
    temperature = random.randrange(0,40)
    msg_bytes = int.to_bytes(temperature, length=4, byteorder='little')
    client.publish("python/temperature", msg_bytes)
    time.sleep(0.5)

client.loop_stop()

