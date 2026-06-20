import paho.mqtt.client as mqtt
import time

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected to broker with code: {reason_code}")
    topic = "devices/+/sensor/temperature"
    client.subscribe(topic)

def on_message(client, userdata, msg):
    temperature = int.from_bytes(msg.payload[:4], byteorder="little")
    print(f"{msg.topic}: {temperature}")

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message
client.connect("127.0.0.1", 1883, 60)
client.loop_start()

time.sleep(10000)

client.loop_stop()

