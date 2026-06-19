import time
import paho.mqtt.client as mqtt

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected with result code {reason_code}")
    client.subscribe("data/temperature")

def on_message(client, userdata, msg):
    print(f"{msg.topic}: {int.from_bytes(msg.payload[:4], byteorder="little")}")

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

client.on_connect = on_connect
client.on_message = on_message

client.connect("127.0.0.1", 1883, 60)

client.loop_start()

time.sleep(1000)

client.loop_stop()

