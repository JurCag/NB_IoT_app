# NB-IoT Application

**Introduction:**<br />
Project showcases the application of NB-IoT technology for uploading data to the cloud. 
It involves an NB-IoT concentrator and 6 sensor nodes equipped with different sensors, specifically designed to measure parameters of a [photobioreactor](https://en.wikipedia.org/wiki/Photobioreactor).<br />

<img src="https://github.com/JurCag/NB_IoT_app/raw/master/images/system_architecture.svg" width="580" alt="System Architecture">

## WEB APP

**Description:**<br />
Simple web application for sensor data visualization and control of NB-IoT concentrator.

**External libraries:**
- AWS SDK: to access AWS services
- JSZip: to manipulate ZIP files
- Highcharts: data visualization

## FIRMWARE

**Development tools:**<br />
- VSCode + [PlatformIO](https://platformio.org/)
- Framework: ESP-IDF (within platformio project, no need to install separately)

### NB-IoT Concentrator 
**Description:**<br />
Firmware of NB-IoT concentrator:
- The concentrator, acting as a master device, collects measurement data from sensor nodes and uploads it to the AWS IoT Core.
- The concentrator also establishes a local BLE mesh network and provisions other nodes into the network.
- BLE mesh models implemented by concentrator:
  - Configuration Server
  - Configuration Client
  - Sensor Client 

Simplifed design of concentrator firmware is shown in the diagram below:

<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/Concentrator_FW.svg" width="700">

**Hardware:**<br />
Custom PCB board with ESP32-C3 controller and Quectel BG96 module.

| Component     | Link  |
| :-----------  |:---------------:|
| **MCU**                 | [ESP32-C3](https://www.espressif.com/en/products/socs/esp32-c3) |
| **NB-IoT module**       | [Quectel BG96](https://www.quectel.com/product/lpwa-bg96-cat-m1-nb1-egprs) |
| **NB-IoT module board** | [LTE IoT 2 Click](https://www.mikroe.com/lte-iot-2-click) |
| **NB-IoT concentrator** | [NB-IoT_PCB_rev_1_1](https://github.com/JurCag/NB_IoT_app/tree/master/NB-IoT_PCB_rev_1_1) |

<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/custom_PCB_with_BG96_and_ESP32-C3.png" width="600" alt="System Architecture">

### Sensor boards
**Description:**<br />
Firmware of individual sensor nodes:
- Each sensor node incorporates the same BLE mesh models.
- BLE mesh models implemented by sensor nodes:
  - Configuration Server
  - Sensor Server 
  - Sensor Setup Server 
- The drivers are specific to the corresponding sensor they are equipped with.

Simplifed design of their firmware is shown in the diagram below:

<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/Sensor_node_FW.svg" width="700">

**Hardware:**<br />
 - **Control board:<br />**
 Each sensor node has the same control board: [ESP32-C3-DevKitC-02](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/hw-reference/esp32c3/user-guide-devkitc-02.html)

 - **Sensors:<br />**

| Physical quantities        |     Sensor        |  Connection     | 
| :-----------: |:---------------:|:---------------:|
| temperature,<br /> humidity,<br /> pressure | BME280    |<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/temp_hum_press_sensor_BME280.png" width="500">|
| pH                                          | PH4502C   |<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/pH_sensor_4502C.png" width="500">|
| CO_2 concentration                          | MHZ19B    |<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/CO2_sensor_MHZ19B.png" width="500">|
| light intensity                             | GL5528    |<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/photoresistor_GL5528.png" width="300">|
| turbidity                                   | TS300B    |<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/turbidity_sensor_TS300B.png" width="500">|
| electric current                            | ACS712    |<img src="https://github.com/JurCag/NB_IoT_app/blob/master/images/current_sensor_ACS712.png" width="500">|
