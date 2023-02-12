var allCharts = [];
var allHighCharts = {};
var displayLastMmtPoints = 10;

const socket = new WebSocket(
    'wss://ikomqw0ee9.execute-api.eu-central-1.amazonaws.com/production'
); //example: 'wss://3143544j.execute-api.us-east-1.amazonaws.com/production'

socket.addEventListener('open', event => {
    console.log(
        'WebSocket is connected, now check for your new Connection ID in Cloudwatch on AWS'
    );
});

socket.addEventListener('message', event => {
    console.log('Your iot event:', event);
    //   console.log('Your iot payload is (event.data):', event.data);
    drawChart(event.data);
});

let drawChart = function(data) {
    // Extract JSON object from data
    var IoT_Payload = JSON.parse(data);
    let {
        timestamps,
        measurements
    } = IoT_Payload;
    let chartIndex;


    incomingNodeName = IoT_Payload.nodeName;

    chartIndex = allCharts.findIndex(function(chart) {
        return chart.nodeName === incomingNodeName;
    });

    console.log('Received chartIndex: ', chartIndex);

    if (chartIndex !== -1) {
        console.log("Node name already exists in allCharts at index: " + chartIndex);

        for (let i = 0; i < measurements.length; i++) {
            let pq = measurements[i].physicalQuantity + " [" + measurements[i].unit + "]";
            let value = measurements[i].value;

            //console.log("Chart Data " + allCharts[chartIndex].chartData[pq]);

            if (pq in allCharts[chartIndex].chartData) {
                allCharts[chartIndex].chartData[pq].push(value);
            } else {
                console.log("This pq was not in first IoT payload: " + pq);
            }
        }
    } else {
        console.log("Creating new chart with name: " + IoT_Payload.nodeName);
        var newChart = {
            nodeName: incomingNodeName,
            chartData: {},
            timeSteps: []
        }

        // Create a new container
        let container = document.createElement("div");
        document.getElementById("mainContainer").appendChild(container);

        // Create the chart using the new container
        allHighCharts[incomingNodeName] = Highcharts.chart(container, {
            title: {
                text: incomingNodeName,
                style: {
                    fontWeight: 'bold',
                    fontSize: '28px',
                    color: "#000000"
                }
            },
            subtitle: {
                text: 'Sensor: ' + measurements[0].sensorName,
                style: {
                    fontSize: '18px',
                    color: "grey"
                }
            },
            yAxis: {
                title: {
                    text: 'Value',
                    style: {
                        fontWeight: 'bold',
                        fontSize: '16px',
                        color: 'grey'
                    }
                }
            },
            xAxis: {
                type: 'datetime',
                labels: {
                    formatter: function() {
                        return Highcharts.dateFormat('%d.%m.%y %H:%M:%S.%L', this.value);
                    }
                },
                categories: newChart.timeSteps
            },
            legend: {
                layout: 'vertical',
                align: 'right',
                verticalAlign: 'middle'
            },
            plotOptions: {
                series: {
                    label: {
                        connectorAllowed: false
                    }
                }
            },
            series: [],
            responsive: {
                rules: [{
                    condition: {
                        maxWidth: 500
                    },
                    chartOptions: {
                        legend: {
                            layout: 'horizontal',
                            align: 'center',
                            verticalAlign: 'bottom'
                        }
                    }
                }]
            },
            credits: {
                enabled: false
            }
        });

        for (let i = 0; i < measurements.length; i++) {
            let pq = measurements[i].physicalQuantity + " [" + measurements[i].unit + "]";
            let value = measurements[i].value;

            newChart.chartData[pq] = [value];
            allHighCharts[incomingNodeName].addSeries({
                name: pq,
                data: [value]
            });
            console.log("New Node: " + newChart.nodeName + ", New pq: " + pq);
            console.log("New data: " + newChart.chartData[pq]);
        }

        allCharts.push(newChart);
        chartIndex = allCharts.length - 1;
        console.log("allCharts New Node: " + allCharts[chartIndex].nodeName);

    }

    allCharts[chartIndex].timeSteps.push(Number(IoT_Payload.timestamps));
    if (allCharts[chartIndex].timeSteps.length > displayLastMmtPoints) {
        allCharts[chartIndex].timeSteps.shift();
    }

    console.log("Chart ts: " + allCharts[chartIndex].timeSteps);

    Object.keys(allCharts[chartIndex].chartData).forEach(physicalQuantity => {
        let seriesIndex = allHighCharts[incomingNodeName].series.findIndex(s =>
            s.name === physicalQuantity);

        if (allCharts[chartIndex].chartData[physicalQuantity].length > displayLastMmtPoints) {
            allCharts[chartIndex].chartData[physicalQuantity] = allCharts[chartIndex].chartData[physicalQuantity].slice((-1 * displayLastMmtPoints));
        }

        allHighCharts[incomingNodeName].series[seriesIndex].setData(
            allCharts[chartIndex].chartData[physicalQuantity], true);
        console.log("new dataa ", allCharts[chartIndex].chartData[physicalQuantity]);
    });

};