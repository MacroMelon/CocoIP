# CocoIP
A quick and easy way to transfer data over the internet between any two devices.

It is a API that transforms the lengthy process of socket handling into a succinct operation that allows more time for endevours of a higher caliber and frees up resources.

There is still much work to do but core functionality is up to par.

# Important Note:
As of now, only the windows client library and the linux server library is on this repo. I'm still working on the windows server and linux client.

# Use cases:
client(device eg rover) <-> Server <-> client(controller ie a computer running a GUI control interface and can display telemetry)

client(device eg rover <-> Server (server linked to controll gui so the server itself sends commands to rover)

server(device eg rover) <-> client controller to send commands to it (not recomended, TODO- why it isnt recomended)

# Documentation
As of now, here is a basic example:

Tracking a GPS unit in real time via the internet:

Check the examples folder for a line by line breakdown of the functions and how to use them in each case (client/ server)
The example code is quite self explanetory.

TODO- finish documentation
