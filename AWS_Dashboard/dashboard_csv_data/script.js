// Globals
var allCharts = [];
var allHighCharts = {};
var displayLastMmtPoints = 10;
var nodeNames = [];
var liveDataIntervId = null;
const topicUpdatePeriodCommand = "BG96_demoThing/mmtPeriods/command";
const topicUpdatePeriodResponse = "BG96_demoThing/mmtPeriods/response";
const AWS = window.AWS;
var s3;
const bucketName = "ws-bucket24";
const folderKey = "mqtt_data/";
var isInLiveMode;
var iotdata;
var knownPbrNodes = ["NODE-BME280", "NODE-Photoresistor", "NODE-Acs712", "NODE-Mhz19b", "NODE-TS-300B", "NODE-4502c"];
var updateMmtPetiodResponseReceived = false;
var nodeMmtPeriods = {};

// Load AWS credentials
$(document).ready(function () {
    $.ajax({
        url: "secrets/aws_credentials.txt",
        dataType: "json",
        async: false,
        success: function (credentials) {
            AWS.config.update(credentials);
            s3 = new AWS.S3();

            // Create an IoT data object
            iotdata = new AWS.IotData({
                endpoint: "a3i8xypesjyhwe-ats.iot.eu-central-1.amazonaws.com"
            });

            console.log("Successfully updated AWS credentials");
            // showLiveData();
        },
        error: function () {
            console.error("Failed to load AWS credentials.");
        }
    });
});

// Measurement period settings
const nodeNameInput = document.getElementById("node-input");
const hoursInput = document.getElementById("hours");
const minutesInput = document.getElementById("minutes");
const secondsInput = document.getElementById("seconds");
const fromDateInput = document.getElementById("from-date");
const toDateInput = document.getElementById("to-date");

// Increase or decrease input value while hovering mouse, using scroll
hoursInput.addEventListener("wheel", handleWheel);
minutesInput.addEventListener("wheel", handleWheel);
secondsInput.addEventListener("wheel", handleWheel);
hoursInput.addEventListener("change", handleInput);
minutesInput.addEventListener("change", handleInput);
secondsInput.addEventListener("change", handleSecondsInput);

// Send nodes measurement time period to the concentrator
const updatePeriodBtn = document.getElementById("update-period-button");
updatePeriodBtn.addEventListener("click", updatePeriod);

// Attach event listener to the show data button
const showBtn = document.getElementById("show-btn");
showBtn.addEventListener("click", showTimePeriodData);

// Attach event listener to the live data button
const liveBtn = document.getElementById("live-btn");
liveBtn.addEventListener("click", showLiveData);
// liveBtn.addEventListener("click", startPeriodicDataRefresh);

// Attach event listener to the download data button
const downloadBtn = document.getElementById("download-btn");
downloadBtn.addEventListener("click", downloadS3Bucket);

// Create websocket object and connect to URL using the WSS (WebSocket Secure) protocol
const socket = new WebSocket(
    "wss://ikomqw0ee9.execute-api.eu-central-1.amazonaws.com/production"
);

socket.addEventListener("open", (event) => {
    console.log(
        "WebSocket successfully connected.", event
    );
});

socket.addEventListener("message", iotEventHandler);

function updateNodePeriodInChart(node, period){
    let subtitleText = allHighCharts[node].subtitle.textStr;

    var h = Math.floor(period / 3600);
    var m = Math.floor((period % 3600) / 60);
    var s = period % 60;

    if (subtitleText.includes("Period:")) {
        subtitleText = subtitleText.replace(
            /Period:.*$/,
            "Period: " +
            h.toString().padStart(2, "0") +
            ":" +
            m.toString().padStart(2, "0") +
            ":" +
            s.toString().padStart(2, "0")
        );
    } else {
        subtitleText +=
            "Period: " +
            h.toString().padStart(2, "0") +
            ":" +
            m.toString().padStart(2, "0") +
            ":" +
            s.toString().padStart(2, "0");
    }

    allHighCharts[node].subtitle.update({
        text: subtitleText,
        style: {
            color: "#1AAD20",
            fontWeight: "bold"
        }
    });

    allHighCharts[node].title.update({
        style: {
            color: "#1AAD20",
            fontWeight: "bold"
        }
    });

    storeNodeMmtPeriodUpdatedInChart(node, true);

    setTimeout(function () {
        allHighCharts[node].subtitle.update({
            style: {
                color: "#5F5B5B",
                fontWeight: "normal"
            }
        });
    }, 1000);
}

// Parser of received mqtt message from concentrator
function iotEventHandler(event) {
    // console.log("Received iot event:", event);
    var IoT_Payload = JSON.parse(event.data);

    let topic = IoT_Payload.topic;
    var incomingNodeName = IoT_Payload.nodeName;
    console.log("Received IoT_Payload:", IoT_Payload);

    if (isInLiveMode === true) {
        if (topic === topicUpdatePeriodResponse) {
            let period = IoT_Payload.period;
            console.log("Node period:", period);
            if (period > 0) {
                if (!(incomingNodeName in allHighCharts)) {
                    const nodeQtyMap = {};
                    nodeQtyMap[incomingNodeName] = {};
                    drawChart(nodeQtyMap, false);
                    addNodeName(incomingNodeName);
                }

                storeNodeMmtPeriod(incomingNodeName, period);
                updateNodePeriodInChart(incomingNodeName, period);
            } else {
                console.log(`Node: "${incomingNodeName}" is not online.`);
            }
            updateMmtPetiodResponseReceived = true;
        }
        else {
            const node = IoT_Payload.nodeName;
            let measurements = IoT_Payload.measurements;
            let timestamps = IoT_Payload.timestamps;
            const nodeQtyMap = {};
            addNodeName(node);
            if (measurements && (measurements.length > 0)) {
                for (let i = 0; i < measurements.length; i++) {
                    const qty = measurements[i].physicalQuantity +
                        " [" +
                        measurements[i].unit +
                        "]";
                    const value = measurements[i].value;
                    const tsUnix = timestamps;

                    // Create empty array for the sensor if it doesn't exist yet
                    if (!nodeQtyMap[node]) {
                        nodeQtyMap[node] = {};
                    }

                    // Create an empty array for the physical quantity if it doesn't exist yet
                    if (!nodeQtyMap[node][qty]) {
                        nodeQtyMap[node][qty] = [];
                    }

                    // Add the value to the array for the corresponding sensor and physical quantity
                    nodeQtyMap[node][qty].push([tsUnix, value]);
                }
                drawChart(nodeQtyMap, false);
            }

            asyncStopFlag = false;
            // getNodeMmtPeriodAndUpdateChart(node);
            getNodeMmtPeriodAndUpdateChartWithTimeout(node);
        }
    } else {
        console.log("But not in live mode.");
    }
}

function addNodeName(nodeName) {
    // Add the new node name, if it's not in the array
    if (!nodeNames.includes(nodeName)) {
        nodeNames.push(nodeName);
        
        const option = document.createElement("option");
        option.value = nodeName;
        option.textContent = nodeName;

        nodeInput.appendChild(option);
    }
}

// Node input available node names
const nodeInput = document.getElementById("node-input");
nodeNames.forEach((nodeName) => {
    const option = document.createElement("option");
    option.value = nodeName;
    option.textContent = nodeName;
    nodeInput.appendChild(option);
});

// Saturate input value
function handleInput(e) {
    const input = e.currentTarget;
    const value = parseInt(input.value, 10);

    if (value > parseInt(input.max, 10)) {
        input.value = input.max;
    } else if (value < parseInt(input.min, 10)) {
        input.value = input.min;
    }
}

// Increment/dectrement while hover over
function handleWheel(e) {
    const input = e.currentTarget;
    const step = parseInt(input.step, 10);
    const value = parseInt(input.value, 10);
    const delta = e.deltaY < 0 ? step : -step;
    input.value = Math.max(Math.min(value + delta, input.max), input.min);
    if (input.id === "seconds") {
        input.value = Math.round(value / step) * step;
    }

    e.preventDefault(); // Disable page scrolling when hovering over the value
}

// Round seconds input value to closest multiple of 5
function handleSecondsInput() {
    const value = parseInt(secondsInput.value, 10);
    const step = parseInt(secondsInput.step, 10);
    if (secondsInput.value >= parseInt(secondsInput.max, 10)) {
        secondsInput.value = parseInt(secondsInput.max, 10);
    } else if (value % step !== 0) {
        secondsInput.value = Math.round(value / step) * step;
    } else if (secondsInput.value < parseInt(secondsInput.min, 10)) {
        secondsInput.value = parseInt(secondsInput.min, 10);
    }
}

// Publish mqtt message to be received by concentrator ("downlink")
function updatePeriod() {
    const nodeName = nodeNameInput.value;
    const hours = parseInt(hoursInput.value, 10) * 60 * 60;
    const minutes = parseInt(minutesInput.value, 10) * 60;
    const seconds = parseInt(secondsInput.value, 10);
    const period = hours + minutes + seconds;

    if (period < 5)
    {
        highlightElementBorder(nodeNameInput, "#FF0000", 600);
        highlightElementBorder(hoursInput, "#FF0000", 600);
        highlightElementBorder(minutesInput, "#FF0000", 600);
        highlightElementBorder(secondsInput, "#FF0000", 600);
        return;
    }

    console.log(`Updating node: "${nodeName}" period: ${period} seconds`);

    if (nodeName.length !== 0) {
        const message = {
            nodeName: nodeName,
            period: period
        };

        const params = {
            topic: topicUpdatePeriodCommand,
            payload: JSON.stringify(message)
        };

        iotdata.publish(params, function (err, data) {
            if (err) {
                console.log(err);
            } else {
                // Publish success
                console.log("Message published:", data);
                highlightElementBorder(nodeNameInput, "#4caf50", 600);
                highlightElementBorder(hoursInput, "#4caf50", 600);
                highlightElementBorder(minutesInput, "#4caf50", 600);
                highlightElementBorder(secondsInput, "#4caf50", 600);
            }
        });
    } else {
        highlightElementBorder(nodeNameInput, "#FF0000", 750);
        console.log("Missing node name, but period is: " + period);
    }
}

// Ask for nodes mmt period without updating it
async function requestNodesMmtPeriod(nodeName) {
    const requestPeriodMsg = {
        nodeName: nodeName,
        period: "?"
    };
    
    const requestParams = {
        topic: topicUpdatePeriodCommand,
        payload: JSON.stringify(requestPeriodMsg)
    };
    return new Promise((resolve, reject) => {
        iotdata.publish(requestParams, function (err) {
            if (err) {
                console.log(err);
                reject(err);
            } else {
                resolve();
            }
        });
    });
}

// Download MQTT data stored in csv files
async function downloadS3Bucket() {
    const s3 = new AWS.S3();
    const bucketName = "ws-bucket24";
    const folderKey = "mqtt_data/";
    try {
        // List all files in the S3 bucket folder
        const fileListResponse = await s3
            .listObjectsV2({
                Bucket: bucketName,
                Prefix: folderKey
            })
            .promise();
        const fileList = fileListResponse.Contents.map((file) => file.Key);
        // Filter files by date range
        const fromDate = new Date(document.getElementById("from-date").value);
        fromDate.setHours(0);
        fromDate.setMinutes(0);
        fromDate.setSeconds(1);
        const toDate = new Date(document.getElementById("to-date").value);
        toDate.setHours(23);
        toDate.setMinutes(59);
        toDate.setSeconds(59);
        const filteredFiles = fileList.filter((fileKey) => {
            const dateParts = fileKey
                .split("/")
                .slice(-4)
                .map((part) => parseInt(part));
            const fileDate = new Date(
                Date.UTC(
                    dateParts[0],
                    dateParts[1] - 1,
                    dateParts[2],
                    dateParts[3]
                )
            );
            // console.log("fromDate: ", fromDate);
            // console.log("toDate: ", toDate);
            // console.log("fileDate: ", fileDate);
            return fileDate >= fromDate && fileDate <= toDate;
        });

        console.log("Files found: ", filteredFiles);
        if (filteredFiles.length === 0) {
            console.log("No files found");
            highlightElementBorder(fromDateInput, "#FF0000", 600);
            highlightElementBorder(toDateInput, "#FF0000", 600);
            return;
        }

        // Create a ZIP archive containing all files
        const zip = new JSZip();
        for (const fileKey of filteredFiles) {
            const fileResponse = await s3
                .getObject({
                    Bucket: bucketName,
                    Key: fileKey
                })
                .promise();
            const fileName = fileKey.substring(folderKey.length);
            zip.file(fileName, fileResponse.Body);
        }

        const zipBlob = await zip.generateAsync({
            type: "blob"
        });

        // Download the ZIP file to user's computer
        const downloadLink = document.createElement("a");
        downloadLink.href = window.URL.createObjectURL(zipBlob);
        downloadLink.download = `mqtt_data_${fromDate
            .toDateString()
            .replace(/ /g, "-")}_${toDate
            .toDateString()
            .replace(/ /g, "-")}.zip`;
        downloadLink.click();
    } catch (err) {
        console.error(err);
        alert("An error occurred while downloading the MQTT data.");
    }
}

async function getCsvFilesFromS3Bucket(numOfLastFiles) {
    try {
        // List all files in the S3 bucket folder
        const fileListResponse = await s3
            .listObjectsV2({
                Bucket: bucketName,
                Prefix: folderKey
            })
            .promise();
        const fileList = fileListResponse.Contents.map((file) => file.Key);
        
        let filteredFiles = [];

        if (numOfLastFiles === "all"){
            // Filter files by chosen date range
            const fromDate = new Date(document.getElementById("from-date").value);
            fromDate.setHours(0);
            fromDate.setMinutes(0);
            fromDate.setSeconds(1);
            const toDate = new Date(document.getElementById("to-date").value);
            toDate.setHours(23);
            toDate.setMinutes(59);
            toDate.setSeconds(59);

            filteredFiles = fileList.filter((fileKey) => {
                const dateParts = fileKey
                    .split("/")
                    .slice(-4)
                    .map((part) => parseInt(part));
                const fileDate = new Date(
                    Date.UTC(
                        dateParts[0],
                        dateParts[1] - 1,
                        dateParts[2],
                        dateParts[3]
                    )
                );
                return fileDate >= fromDate && fileDate <= toDate;
            });
        } else {
            const yesterdayDate = new Date();
            yesterdayDate.setDate(yesterdayDate.getDate() - 1);

            const todayDate = new Date();
            todayDate.setHours(23);
            todayDate.setMinutes(59);
            todayDate.setSeconds(59);

            filteredFiles = fileList.filter((fileKey) => {
                const dateParts = fileKey
                    .split("/")
                    .slice(-4)
                    .map((part) => parseInt(part));
                const fileDate = new Date(
                    Date.UTC(
                        dateParts[0],
                        dateParts[1] - 1,
                        dateParts[2],
                        dateParts[3]
                    )
                );
                return fileDate >= yesterdayDate && fileDate <= todayDate;
            });

            if (numOfLastFiles !== "last day") {
                filteredFiles = filteredFiles.slice(-numOfLastFiles);
            }
        }

        return filteredFiles;

    } catch (err) {
        console.error(err);
        alert("An error occurred.");
    }
}

async function extractDataFromCsvFiles(files) {
    const csvData = [];

    // Loop through each fileKey in the files array
    for (const fileKey of files) {
        const fileResponse = await s3.getObject({ Bucket: bucketName, Key: fileKey }).promise();

        // Convert the file data from a binary buffer to a string
        const fileData = fileResponse.Body.toString();
        const rows = fileData.trim().split("\n");

        const csvDict = {
            nodeName: [],
            sensorName: [],
            physicalQuantity: [],
            unit: [],
            value: [],
            timestamp: []
        };

        for (const row of rows) {
            // Split into columns
            const columns = row.split(";");

            // This column order is given by lambda function
            // that orders mqtt data into csv file
            csvDict.nodeName.push(columns[0]);
            csvDict.sensorName.push(columns[1]);
            csvDict.physicalQuantity.push(columns[2]);
            csvDict.unit.push(columns[3]);
            csvDict.value.push(columns[4]);
            csvDict.timestamp.push(columns[5]);
        }

        csvData.push(csvDict);
    }

    return csvData;
}

function mapDataToSensors(csvData){
    const nodeQtyMap = {};
    
    // Iterate over each element in csvData, which holds all nodes data of 1 hour
    for (let i = 0; i < csvData.length; i++) {
        const nodeNames = csvData[i].nodeName;
        const physicalQuantities = csvData[i].physicalQuantity;
        const values = csvData[i].value.map((str) => parseFloat(str));
        const timestamps = csvData[i].timestamp;
        const units = csvData[i].unit;

        for (let nodeIdx = 0; nodeIdx < nodeNames.length; nodeIdx++) {
            const node = nodeNames[nodeIdx];
            const qty = physicalQuantities[nodeIdx] + " [" + units[nodeIdx] + "]";
            const value = values[nodeIdx];
            const ts = timestamps[nodeIdx];
            const tsUnix = Date.parse(ts.replace("\r", ""));

            // Create empty array for the sensor if it doesn't exist yet
            if (!nodeQtyMap[node]) {
                nodeQtyMap[node] = {};
            }

            // Create an empty array for the physical quantity if it doesn't exist yet
            if (!nodeQtyMap[node][qty]) {
                nodeQtyMap[node][qty] = [];
            }

            // Add the value to the array for the corresponding sensor and physical quantity
            nodeQtyMap[node][qty].push([tsUnix, value]);
        }
    }

    return nodeQtyMap;
}

// Show CSV data within chosen time period
async function showTimePeriodData() {
    let csvFiles = [];
    csvFiles = await getCsvFilesFromS3Bucket("all");

    if (csvFiles.length === 0) {
        console.log("In the given time period no CSV files found in S3 bucket.");
        highlightElementBorder(fromDateInput, "#FF0000", 600);
        highlightElementBorder(toDateInput, "#FF0000", 600);
        return;
    }

    showBtn.style.border = "3px solid black";
    liveBtn.style.border = "3px solid #ccc";
    isInLiveMode = false;
    stopAsyncAskingNodesMmtPeriod();

    // stopPeriodicDataRefresh();
    console.log("In the given time period CSV files found in S3 bucket: ", csvFiles);

    let csvData = [];
    csvData = await extractDataFromCsvFiles(csvFiles);
    // console.log("Extracted data from CSV files: ",csvData);

    let sensorQtyMap = {};
    sensorQtyMap = mapDataToSensors(csvData);
    // console.log("Mapped sensor data: ", sensorQtyMap);

    // Note that periods are not updated in charts since this is not live mode
    for (const node in nodeMmtPeriods) {
        if (nodeMmtPeriods.hasOwnProperty(node)) {
            storeNodeMmtPeriodUpdatedInChart(node, false);
        }
    }

    cleanAllCharts();
    drawChart(sensorQtyMap, true);
}

// Show live CSV data
async function showLiveData() {
    liveBtn.style.border = "3px solid black";
    showBtn.style.border = "3px solid #ccc";

    let csvFiles = [];
    csvFiles = await getCsvFilesFromS3Bucket("last day");

    for (let node of knownPbrNodes) {
        addNodeName(node);
        storeNodeMmtPeriodUpdatedInChart(node, false);
    }

    isInLiveMode = true;
    if (csvFiles.length === 0) {
        console.log("In the given time period no CSV files found in S3 bucket.");
        let checkPbrMmtNodes = true;
        
        for (let node of knownPbrNodes) {
            addNodeName(node);
        }
        // for (let node of knownPbrNodes) {
        //     await getNodeMmtPeriodAndUpdateChart(node);
        // }

        return;
    }
    console.log("In the given time period CSV files found in S3 bucket: ", csvFiles);

    let csvData = [];
    csvData = await extractDataFromCsvFiles(csvFiles);
    console.log("Extracted data from CSV files: ",csvData);

    let sensorQtyMap = {};
    sensorQtyMap = mapDataToSensors(csvData);
    // console.log("Mapped sensor data: ", sensorQtyMap);

    // Add node options to update period
    // for (const nodeName in sensorQtyMap) {
    //     addNodeName(nodeName);
    // }
    for (let node of knownPbrNodes) {
        addNodeName(node);
    }
    
    cleanAllCharts();
    drawChart(sensorQtyMap, true);
    
    asyncStopFlag = false;
    for (let node of knownPbrNodes) {
        await getNodeMmtPeriodAndUpdateChartWithTimeout(node);
    }
}

function drawChart (data, redrawOnUpdate) {
    for (const sensorNode in data) {
        // console.log("Node:", sensorNode);

        chartIndex = allCharts.findIndex(function (chart) {
            return chart.sensorName === sensorNode;
        });

        if (chartIndex !== -1) {
            // console.log("Sensor", sensorNode, "already exists in allCharts at index:", chartIndex);
            // console.log("sensor data: ", allCharts[chartIndex]);
            
            if (redrawOnUpdate === true) {
                console.log("redrawOnUpdate == true");

                let existingChart = {
                    sensorName: sensorNode,
                    series: [],
                };

                // Remove all previous series data
                while (allHighCharts[sensorNode].series.length) {
                    allHighCharts[sensorNode].series[0].remove(false);
                }

                for (const qty in data[sensorNode]) {
                    if (Object.hasOwnProperty.call(data[sensorNode], qty)) {
                        // Create a new series object for the physical quantity
                        const qtyDict = {
                            name: qty,
                            data: data[sensorNode][qty]
                        };

                        existingChart.series.push(qtyDict);
                        
                        allHighCharts[sensorNode].addSeries({
                            name: qtyDict.name,
                            data: qtyDict.data
                        });
                    }
                }

                allHighCharts[sensorNode].redraw();
            }
            else
            {
                let existingChart = {
                    sensorName: sensorNode,
                    series: [],
                };
                
                // console.log("data[sensor]", data[sensorNode]);
                for (const qty in data[sensorNode]) {
                    if (Object.hasOwnProperty.call(data[sensorNode], qty)) {
                        const valueArray = [];
                        const tsArray = [];

                        for (const [tsUnix, value] of data[sensorNode][qty]) {
                            valueArray.push(value);
                            tsArray.push(tsUnix);
                        }

                        // existingChart.chartData[qty] = valueArray;
                        // existingChart.timeSteps = tsArray;

                        // Create a new series object for the physical quantity
                        const qtyDict = {
                            name: qty,
                            data: data[sensorNode][qty]
                        };
                        existingChart.series.push(qtyDict); 
                    }
                }

                // console.log("existingChart", existingChart);
                // console.log("allCharts[chartIndex]", allCharts[chartIndex]);
                for (let i = 0; i < existingChart.series.length; i++) {
                    
                    let newLiveDataArr = [];
                    if (allCharts[chartIndex]?.series?.[i]?.data && existingChart?.series?.[i]?.data) {
                        // Node and it's series exist in allCharts
                        newLiveDataArr = compareArrays(allCharts[chartIndex].series[i].data, existingChart.series[i].data);
                    } else {
                        // Node and it's series is not in allCharts yet
                        console.log("ADDING NEW SERIES");
                        newLiveDataArr = existingChart.series[i].data;
                        allCharts[chartIndex].series.push(existingChart.series[i]);
                        allHighCharts[sensorNode].addSeries({
                            name: existingChart.series[i].name,
                            data: []
                        });
                    }

                    newLiveDataArr.forEach((newLiveData) => {
                        console.log("NEW LIVE DATA:",newLiveData);
                        allHighCharts[sensorNode].series[i].addPoint(newLiveData);
                    });
                }
            }

        } else {    // Create chart for new sensor node
            var newChart = {
                sensorName: sensorNode,
                series: []
            };

            // Create a new container
            let container = document.createElement("div");
            document.getElementById("mainContainer").appendChild(container);

            allHighCharts[sensorNode] = Highcharts.chart(container, {
                chart: {
                    height: 300,
                    type: 'line',
                    zoomType: 'x'
                },
                time: {
                    timezoneOffset: -1 * 60
                },
                title: {
                    text: sensorNode,
                    style: {
                        fontFamily: "Segoe UI, Tahoma, sans-serif",
                        fontWeight: "normal",
                        fontSize: "28px",
                        color: "#5F5B5B"
                    }
                },
                subtitle: {
                    text: "Period: *not available*",
                    style: {
                        fontFamily: "Segoe UI, Tahoma, sans-serif",
                        fontSize: "20px",
                        color: "#5F5B5B",
                        fontStyle: "italic"
                    }
                },
                yAxis: {
                    title: {
                        text: "Value",
                        style: {
                            fontWeight: "bold",
                            fontSize: "20px",
                            color: "#5F5B5B"
                        }
                    },
                    labels: {
                        style: {
                            fontSize: "15px"
                        }
                    }
                },
                xAxis: {
                    type: "datetime",
                    labels: {
                        style: {
                            fontSize: "15px"
                        }
                    }
                },
                legend: {
                    layout: "vertical",
                    align: "right",
                    verticalAlign: "middle",
                    itemStyle: {
                        fontSize: "20px"
                    }
                },
                plotOptions: {
                    series: {
                        marker: {
                            enabled: true,
                            radius: 3
                        }
                    }
                },
                series: [],
                responsive: {
                    rules: [
                        {
                            condition: {
                                maxWidth: 500
                            },
                            chartOptions: {
                                legend: {
                                    layout: "horizontal",
                                    align: "center",
                                    verticalAlign: "bottom",
                                    style: {
                                        fontSize: "20px"
                                    }
                                }
                            }
                        }
                    ]
                },
                colors: ['#009EF8', '#A900F2', '#00D77C', '#F47300'],
                credits: {
                    enabled: false
                }
            });


            for (const qty in data[sensorNode]) {
                if (Object.hasOwnProperty.call(data[sensorNode], qty)) {
                    const valueArray = [];
                    const tsArray = [];

                    for (const [tsUnix, value] of data[sensorNode][qty]) {
                        valueArray.push(value);
                        tsArray.push(tsUnix);
                    }

                    // newChart.chartData[qty] = valueArray;
                    // newChart.timeSteps = tsArray;

                    // Create a new series object for the physical quantity
                    const qtyDict = {
                        name: qty,
                        // data: valueArray
                        data: data[sensorNode][qty]
                    };

                    newChart.series.push(qtyDict);

                    allHighCharts[sensorNode].addSeries({
                        name: qtyDict.name,
                        data: qtyDict.data
                    });
                    // console.log("qtyDict (one element of chart series): ", qtyDict);
                }
            }

            console.log("Added newChart", newChart);
            allCharts.push(newChart);
        }
    }
};



function compareArrays(originalArr, newArr) {
    let diffElements = [];
    for (let i = 0; i < newArr.length; i++) {
        const found = originalArr.some((el) => el[0] === newArr[i][0] && el[1] === newArr[i][1]);
        if (!found) {
            diffElements.push(newArr[i]);
        }
    }
    console.log("NEW ELEMENTS:", diffElements);
    return diffElements;
}

function cleanAllCharts() {
    for (const sensorNode in allHighCharts) {
        if (Object.hasOwnProperty.call(allHighCharts, sensorNode)) {
            const chart = allHighCharts[sensorNode];
            if (chart) {
                if (chart.options.chart.forExport) {
                    // ensure the chart exists before destroy it
                    Highcharts.exportingChartMenu.update({
                        chart: chart
                    });
                }
                chart.destroy();
            }
        }
    }
    allHighCharts = {};
    allCharts = [];
}

function highlightElementBorder(htmlElement, color, timeout) {
    const originalBorderColor = htmlElement.style.borderColor;
    htmlElement.style.borderColor = color;

    return new Promise((resolve) => {
        setTimeout(() => {
            htmlElement.style.borderColor = originalBorderColor;
            resolve();
        }, timeout);
    });
}

var requestingMmtPeriodFirstTime = true;
function askNodesMmtPeriod(sensorNode) {
    return new Promise((resolve, reject) => {
        let timeoutId = setTimeout(() => {
            reject(new Error("Timed-out while waiting for response"));
        }, 10000);
        const intervalId = setInterval(() => {
            if (updateMmtPetiodResponseReceived || requestingMmtPeriodFirstTime) {
                clearInterval(intervalId);
                updateMmtPetiodResponseReceived = false;
                clearTimeout(timeoutId);
                requestNodesMmtPeriod(sensorNode);
                resolve();
                requestingMmtPeriodFirstTime = false;
            }
        }, 1000);
    });
}

function storeNodeMmtPeriod(node, period) {
    if (!nodeMmtPeriods[node]) {
        nodeMmtPeriods[node] = {};
    }
    nodeMmtPeriods[node]["period"] = period;

    if (!nodeMmtPeriods[node]["isInChart"])
    {
        nodeMmtPeriods[node]["isInChart"] = false;
    }
}

function storeNodeMmtPeriodUpdatedInChart(node, isUpdated) {
    if (!nodeMmtPeriods[node]) {
        nodeMmtPeriods[node] = {};
    }
    nodeMmtPeriods[node]["isInChart"] = isUpdated;
}

let asyncStopFlag = false;
async function getNodeMmtPeriodAndUpdateChart(node) {
    if (!nodeMmtPeriods[node]["isInChart"]) {
        if (nodeMmtPeriods[node]["period"]) {
            updateNodePeriodInChart(node, nodeMmtPeriods[node]["period"]);
        } else {
            try {
                await Promise.race([askNodesMmtPeriod(node), stopAsyncMmtRequestsOnFlag()]);
            } catch (error) {
                console.log(error, "By node:", node);
            }
        }
    } else {
        // console.log("Mmt period already updated in chart");
    }
}

async function stopAsyncMmtRequestsOnFlag() {
    while (!asyncStopFlag) {
        await new Promise(resolve => setTimeout(resolve, 100));
    }
}

function stopAsyncAskingNodesMmtPeriod() {
    asyncStopFlag = true;
    console.log("Stop requesting async functions");
}

async function getNodeMmtPeriodAndUpdateChartWithTimeout(node) {
    let timeoutPromise = new Promise((resolve, reject) => setTimeout(
        () => reject(new Error("Timeout while waiting for response")),
        10000));
    try {
        await Promise.race([getNodeMmtPeriodAndUpdateChart(node), timeoutPromise]);
    } catch (error) {
        console.log(error);
    }
}

// async function getNodeMmtPeriodAndUpdateChart(node) {
//     if (!nodeMmtPeriods[node]["isInChart"]) {
//         if (nodeMmtPeriods[node]["period"]) {
//             updateNodePeriodInChart(node, nodeMmtPeriods[node]["period"]);
//         } else {
//             try {
//                 await Promise.race([askNodesMmtPeriod(node), new Promise(
//                     (resolve, reject) => setTimeout(
//                         () => reject(new Error("Timeout while waiting for response")),
//                         5000))]);
//             } catch (error) {
//                 console.log(error, node);
//             }
//         }
//     }
// }
