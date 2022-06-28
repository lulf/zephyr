.. _drogue-iot-mqtt-sample:

Drogue IoT MQTT Sample
######################

Overview
********

This sample application demonstrates a "full stack" application.  This
currently is able to

- Acquire a DHCPv4 lease.
- Establish a TLS connection with a Drogue IoT service
- Publish data to a Drogue IoT service

The source code for this sample application can be found at:
:zephyr_file:`samples/net/cloud/drogue_iot_mqtt`.

Requirements
************
- Entropy source
- Drogue IoT instance or account on the sandbox
- Drogue IoT application and device name
- Drogue IoT command line tool `drg`
- Network connectivity

Building and Running
********************
This application has been built and tested on the STM32L4.  RSA or
ECDSA certs/keys are used to authenticate to the Google IOT Cloud.
The application includes a key creation script.

Users will also be required to configure the following Kconfig options
based on their Drogue IoT application.  The following values come
from Drogue IoT itself:

- DROGUE_APPLICATION: A name given for the application.
- DROGUE_DEVICE: A name given for the device.  When viewing the table of
  devices, this will be shown.
- DROGUE_DEVICE_PASSWORD: A password given for the device.
- DROGUE_HOSTNAME: The hostname pointing to the Drogue IoT service being used.
- DROGUE_PORT: The port for the MQTT service on the Drogue IoT service being used.

From these values, the config values can be set using the following
template:

.. code-block:: kconfig

   CONFIG_DROGUE_HOSTNAME="mqtt.sandbox.drogue.cloud"
   CONFIG_DROGUE_PORT="443"
   
   CONFIG_DROGUE_APPLICATION="example-app"
   CONFIG_DROGUE_DEVICE="device1"
   CONFIG_DROGUE_DEVICE_PASSWORD="hey-rodney"

See `Drogue IoT MQTT Documentation
<https://book.drogue.io/drogue-cloud/dev/user-guide/endpoint-mqtt.html>`_.
