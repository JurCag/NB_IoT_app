# NB-IoT Application

Table of contents:
- [Introduction](#introduction)
- [System Architecture](#system-architecture)
- [Web Application](#web-application)
- [Firmware and Hardware](#firmware-and-hardware)
    - [Communication overview](#communication-overview)
    - [Development tools](#development-tools)
  - [NB-IoT Concentrator](#nb-iot-concentrator)
    - [Firmware](#firmware)
    - [Hardware](#hardware)
  - [Sensor Boards](#sensor-boards)
    - [Firmware](#firmware)
    - [Hardware](#hardware)
 
#### Introduction:
Project showcases the application of NB-IoT technology for uploading data to the cloud. 
Hardware involves an NB-IoT concentrator and 6 sensor nodes equipped with different sensors, specifically designed to measure parameters of a [photobioreactor](https://en.wikipedia.org/wiki/Photobioreactor).<br />
Simple concept of the application including PBR is available [here](https://github.com/JurCag/NB_IoT_app/blob/master/images/photobioreactor.svg).

#### System Architecture:
<div align="center">
<img src="https://github.com/JurCag/NB_IoT_app/raw/master/images/system_architecture.svg" width="580" alt="System Architecture">
</div>

## Web Application

#### Description:
Simple web application for sensor data visualization and control of NB-IoT concentrator.

#### External libraries:
- AWS SDK: to access AWS services
- JSZip: to manipulate ZIP files
- Highcharts: data visualization

<img src="https://github.com/JurCag/NB_IoT_app/raw/master/images/NB-IoT_dashboard.png" >

## Firmware and Hardware

#### Communication overview:
The application uses [MQTT](https://mqtt.org/) protocol to communicate with broker within the AWS IoT Core. MQTT topics and structure of JSON payloads sent in the messages is shown below.
1) The concentrator provisions new nodes into the BLE mesh network. Nodes provide their descriptors.
2) The user requests the node measurement period by publishing a message to the topic: **BG96_demoThing/mmtPeriods/command**.
3) The concentrator responds with the current measurement period of that node to the topic: **BG96_demoThing/mmtPeriods/response**.
4) The user can change the node measurement period (e.g. 10 minutes).
5) The concentrator confirms the change by repeating the message.
6) The concentrator stores the measurement period value into the NVS (Non-Volatile Storage) memory.
7) When the counter reaches the specified period, the concentrator requests data from the sensor node over the BLE mesh.
8) The sensor node sends measured data.
9) The concentrator uploads the sensory data to AWS on the MQTT topic: **BG96_demoThing/sensors/\<nodeName\>**.
  
<div align="center">
  <img src="https://github.com/JurCag/NB_IoT_app/raw/master/images/Comm_packets.svg" width="850" alt="System Architecture">
</div>

#### Development tools:
- VSCode + [PlatformIO](https://platformio.org/)
- Framework: ESP-IDF (within platformio project, no need to install separately)

### NB-IoT Concentrator 
#### Firmware:
- The concentrator, acting as a master device, collects measurement data from sensor nodes and uploads it to the AWS IoT Core.
- The concentrator also establishes a local BLE mesh network and provisions other nodes into the network.
- BLE mesh models implemented by concentrator:
  - Configuration Server
  - Configuration Client
  - Sensor Client 


#### Hardware:
[Custom PCB board](https://github.com/JurCag/NB_IoT_app/tree/master/NB-IoT_PCB_rev_1_1) with ESP32-C3 controller and Quectel BG96 module.

| Component     | Link  |
| :-----------  |:---------------:|
| **MCU**                 | [ESP32-C3](https://www.espressif.com/en/products/socs/esp32-c3) |
| **NB-IoT module**       | [Quectel BG96](https://www.quectel.com/product/lpwa-bg96-cat-m1-nb1-egprs) |
| **NB-IoT module board** | [LTE IoT 2 Click](https://www.mikroe.com/lte-iot-2-click) |
| **NB-IoT concentrator** | [NB-IoT_PCB_rev_1_1](https://github.com/JurCag/NB_IoT_app/tree/master/NB-IoT_PCB_rev_1_1) |

<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/custom_PCB_with_BG96_and_ESP32-C3.png" width="600" alt="System Architecture">

### Sensor boards
#### Firmware:
- Each sensor node incorporates the same BLE mesh models.
- BLE mesh models implemented by sensor nodes:
  - Configuration Server
  - Sensor Server 
  - Sensor Setup Server 
- The drivers are specific to the corresponding sensor they are equipped with.

#### Hardware:
 - **Control board:<br />**
 Each sensor node has the same control board: [ESP32-C3-DevKitC-02](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/hw-reference/esp32c3/user-guide-devkitc-02.html)

<img src="https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/_images/esp32-c3-devkitc-02-v1-annotated-photo.png">

 - **Sensors:<br />**

| Physical quantities        |     Sensor        |  Connection     | 
| :-----------: |:---------------:|:---------------:|
| temperature,<br /> humidity,<br /> pressure | BME280    |<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/temp_hum_press_sensor_BME280.png" width="500">|
| pH                                          | PH4502C   |<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/pH_sensor_4502C.png" width="500">|
| CO_2 concentration                          | MHZ19B    |<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/CO2_sensor_MHZ19B.png" width="500">|
| light intensity                             | GL5528    |<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/photoresistor_GL5528.png" width="300">|
| turbidity                                   | TS300B    |<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/turbidity_sensor_TS300B.png" width="500">|
| electric current                            | ACS712    |<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/current_sensor_ACS712.png" width="500">|
