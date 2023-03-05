// Globals
var allCharts = [];
var allHighCharts = {};
var displayLastMmtPoints = 10;
var nodeNames = [];
var liveDataIntervId = null;
const AWS = window.AWS;
var s3;
const bucketName = "ws-bucket24";
const folderKey = "mqtt_data/";

$(document).ready(function () {
    $.ajax({
        url: "secrets/aws_credentials.txt",
        dataType: "json",
        async: false,
        success: function (credentials) {
            AWS.config.update(credentials);
            s3 = new AWS.S3();
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

// Increase or decrease input value while hovering mouse, using scroll
hoursInput.addEventListener("wheel", handleWheel);
minutesInput.addEventListener("wheel", handleWheel);
secondsInput.addEventListener("wheel", handleWheel);
hoursInput.addEventListener("change", handleInput);
minutesInput.addEventListener("change", handleInput);
secondsInput.addEventListener("change", handleSecondsInput);

// Send measurement time period when button is clicked
const updatePeriodBtn = document.getElementById("update-period-button");
updatePeriodBtn.addEventListener("click", updatePeriod);

// Attach event listener to the show data button
const csvBtn = document.getElementById("show-btn");
csvBtn.addEventListener("click", showTimePeriodData);

// Attach event listener to the live data button
const liveBtn = document.getElementById("live-btn");
liveBtn.addEventListener("click", showLiveData);
liveBtn.addEventListener("click", startPeriodicDataRefresh);

// Attach event listener to the download data button
const downloadBtn = document.getElementById("download-btn");
downloadBtn.addEventListener("click", downloadS3Bucket);

// Create an IoT data object
const iotdata = new AWS.IotData({
    endpoint: "a3i8xypesjyhwe-ats.iot.eu-central-1.amazonaws.com"
});

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

function iotEventHandler(event) {
    // console.log("Received iot event:", event);
    var IoT_Payload = JSON.parse(event.data);
    
    let topic = IoT_Payload.topic;
    let incomingNodeName = IoT_Payload.nodeName;
    console.log("Received IoT_Payload:", IoT_Payload);

    if (topic == "BG96_demoThing/mmtPeriods/response") {
        let period = IoT_Payload.period;
        var hours = Math.floor(period / 3600);
        var minutes = Math.floor((period % 3600) / 60);
        var seconds = period % 60;

        let subtitleText = allHighCharts[incomingNodeName].subtitle.textStr;
        if (subtitleText.includes("Period:")) {
            subtitleText = subtitleText.replace(
                /Period:.*$/,
                "Period: " +
                    hours.toString().padStart(2, "0") +
                    ":" +
                    minutes.toString().padStart(2, "0") +
                    ":" +
                    seconds.toString().padStart(2, "0")
            );
        } else {
            subtitleText +=
                "Period: " +
                hours.toString().padStart(2, "0") +
                ":" +
                minutes.toString().padStart(2, "0") +
                ":" +
                seconds.toString().padStart(2, "0");
        }

        allHighCharts[incomingNodeName].subtitle.update({
            text: subtitleText,
            style: {
                color: "#4caf50",
                fontWeight: "bold"
            }
        });

        setTimeout(function () {
            allHighCharts[incomingNodeName].subtitle.update({
                style: {
                    color: "#5F5B5B",
                    fontWeight: "normal"
                }
            });
        }, 1000);

        return;
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

function updatePeriod() {
    const nodeName = nodeNameInput.value;
    const hours = parseInt(hoursInput.value, 10) * 60 * 60;
    const minutes = parseInt(minutesInput.value, 10) * 60;
    const seconds = parseInt(secondsInput.value, 10);
    const period = hours + minutes + seconds;

    if (period < 5)
    {
        nodeNameInput.style.border = "2px solid #FF0000";
        hoursInput.style.border = "2px solid #FF0000";
        minutesInput.style.border = "2px solid #FF0000";
        secondsInput.style.border = "2px solid #FF0000";
        setTimeout(function () {
            nodeNameInput.style.border = "2px solid #ccc";
            hoursInput.style.border = "2px solid #ccc";
            minutesInput.style.border = "2px solid #ccc";
            secondsInput.style.border = "2px solid #ccc";
        }, 400);
        return;
    }

    console.log(`Updating node: "${nodeName}" period: ${period} seconds`);

    if (nodeName.length !== 0) {
        const message = {
            nodeName: nodeName,
            period: period
        };

        const topic = "BG96_demoThing/mmtPeriods/command";

        const params = {
            topic: topic,
            payload: JSON.stringify(message)
        };

        iotdata.publish(params, function (err, data) {
            if (err) {
                console.log(err);
            } else {
                console.log("Message published:", data);
                nodeNameInput.style.border = "2px solid #4caf50";
                hoursInput.style.border = "2px solid #4caf50";
                minutesInput.style.border = "2px solid #4caf50";
                secondsInput.style.border = "2px solid #4caf50";
                setTimeout(function () {
                    nodeNameInput.style.border = "2px solid #ccc";
                    hoursInput.style.border = "2px solid #ccc";
                    minutesInput.style.border = "2px solid #ccc";
                    secondsInput.style.border = "2px solid #ccc";
                }, 400);
            }
        });
    } else {
        nodeNameInput.style.border = "2px solid red";
        setTimeout(function () {
            nodeNameInput.style.border = "2px solid #ccc";
        }, 500);
        console.log("Missing node name, but period is: " + period);
    }
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
            console.log("fromDate: ", fromDate);
            console.log("toDate: ", toDate);
            console.log("fileDate: ", fileDate);
            return fileDate >= fromDate && fileDate <= toDate;
        });
        console.log("Files found: ", filteredFiles);
        if (filteredFiles.length === 0) {
            console.log("is empty");
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

function startPeriodicDataRefresh() {
    let period = 5000;
    if (liveDataIntervId === null) {
        console.log("START PeriodicDataRefresh");
        liveDataIntervId = setInterval(periodicDataRefresh, period);
    }
}

function stopPeriodicDataRefresh() {
    console.log("STOP PeriodicDataRefresh");
    clearInterval(liveDataIntervId);
    liveDataIntervId = null;
}

async function periodicDataRefresh(){
    console.log("Periodic Data Refresh called");
    let csvFiles = [];
    csvFiles = await getCsvFilesFromS3Bucket(2);

    if (csvFiles.length === 0) {
        console.log("In the given time period no CSV files found in S3 bucket.");
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
    for (const nodeName in sensorQtyMap) {
        addNodeName(nodeName);
    }

    drawChart(sensorQtyMap, false);
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
        return;
    }

    stopPeriodicDataRefresh();
    console.log("In the given time period CSV files found in S3 bucket: ", csvFiles);

    let csvData = [];
    csvData = await extractDataFromCsvFiles(csvFiles);
    console.log("Extracted data from CSV files: ",csvData);

    let sensorQtyMap = {};
    sensorQtyMap = mapDataToSensors(csvData);
    // console.log("Mapped sensor data: ", sensorQtyMap);

    drawChart(sensorQtyMap, true);
}

// Show live CSV data
async function showLiveData() {

    let csvFiles = [];
    csvFiles = await getCsvFilesFromS3Bucket("last day");

    if (csvFiles.length === 0) {
        console.log("In the given time period no CSV files found in S3 bucket.");
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
    for (const nodeName in sensorQtyMap) {
        addNodeName(nodeName);
    }

    drawChart(sensorQtyMap, true);
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
                        
                        // console.log("series", series);
                        // console.log("existingChart.timeSteps", existingChart.timeSteps);
                    }
                }

                // console.log("existingChart", existingChart);
                // console.log("allCharts[chartIndex]", allCharts[chartIndex]);
                for (let i = 0; i < existingChart.series.length; i++) {
                    
                    let newLiveDataArr = [];
                    newLiveDataArr = compareArrays(allCharts[chartIndex].series[i].data, existingChart.series[i].data);

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
                    type: 'line'
                },
                time: {
                    timezoneOffset: -1 * 60
                },
                title: {
                    text: sensorNode,
                    style: {
                        fontFamily: "Segoe UI, Tahoma, sans-serif",
                        fontWeight: "bold",
                        fontSize: "28px",
                        color: "#000000"
                    }
                },
                subtitle: {
                    text: "Period: ??:??:??",
                    style: {
                        fontFamily: "Segoe UI, Tahoma, sans-serif",
                        fontSize: "20px",
                        color: "#5F5B5B"
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
                colors: ['#009EF8', '#F800E9', '#00D77C', '#F47300'],
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

            console.log("newChart", newChart);
            allCharts.push(newChart);
        }
    }
};



function compareArrays(originalArr, newArr) {
    let diffElements = [];
    for (let i = 0; i < newArr.length; i++) {
        const found = originalArr.some((el) => el[0] === newArr[i][0] && el[1] === newArr[i][1]);
        if (!found) {
            console.log("NEW ARRAY HAS:", newArr[i], "ORIGINAL DOES NOT");
            diffElements.push(newArr[i]);
        }
    }

    return diffElements;
}