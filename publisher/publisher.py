import paho.mqtt.client as mqtt
import random
import time

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected to broker with code: {reason_code}")

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.connect("127.0.0.1", 1883, 60)
client.loop_start()

while True:
    topic = "devices/5/sensor/temperature"
    temperature = random.randrange(0,40)
    msg_bytes = int.to_bytes(temperature, length=4, byteorder="little")

    client.publish(topic, msg_bytes)

    time.sleep(0.5)

client.loop_stop()

