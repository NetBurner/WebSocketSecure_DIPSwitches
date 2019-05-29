var dip1, dip2, dip3, dip4, dip5, dip6, dip7, dip8;
var led1, led2, led3, led4, led5, led6, led7, led8;
var report;
var ws;
var consoleLog = true;      // set to true to print debug statements to your browser's console

function updateCheckBox() {
    if (dip1 === "Off") {
        document.getElementById("cbox1").checked = false;
    } else if (dip1 === "On") {
        document.getElementById("cbox1").checked = true;
    }

    if (dip2 === "Off") {
        document.getElementById("cbox2").checked = false;
    } else if (dip1 === "On") {
        document.getElementById("cbox2").checked = true;
    }

    if (dip3 === "Off") {
        document.getElementById("cbox3").checked = false;
    } else if (dip3 === "On") {
        document.getElementById("cbox3").checked = true;
    }

    if (dip4 === "Off") {
        document.getElementById("cbox4").checked = false;
    } else if (dip4 === "On") {
        document.getElementById("cbox4").checked = true;
    }

    if (dip5 === "Off") {
        document.getElementById("cbox5").checked = false;
    } else if (dip5 === "On") {
        document.getElementById("cbox5").checked = true;
    }

    if (dip6 === "Off") {
        document.getElementById("cbox6").checked = false;
    } else if (dip6 === "On") {
        document.getElementById("cbox6").checked = true;
    }

    if (dip7 === "Off") {
        document.getElementById("cbox7").checked = false;
    } else if (dip7 === "On") {
        document.getElementById("cbox7").checked = true;
    }

    if (dip8 === "Off") {
        document.getElementById("cbox8").checked = false;
    } else if (dip8 === "On") {
        document.getElementById("cbox8").checked = true;
    }

}

function SetLED(event) {

    var ledJsonText;

    switch (event.id) {
        case "ledcb1":
            led1 = document.getElementById("ledcb1").checked;
            if(consoleLog) { console.log(event.id + " : " + led1); }
            ledJsonText = '{ "' + event.id + '" : "' + led1 + '" }';
            break;
        case "ledcb2":
            led2 = document.getElementById("ledcb2").checked;
            if(consoleLog) { console.log(event.id + " : " + led2); }
            ledJsonText = '{ "' + event.id + '" : "' + led2 + '" }';
            break;
        case "ledcb3":
            led3 = document.getElementById("ledcb3").checked;
            if(consoleLog) { console.log(event.id + " : " + led3); }
            ledJsonText = '{ "' + event.id + '" : "' + led3 + '" }';
            break;
        case "ledcb4":
            led4 = document.getElementById("ledcb4").checked;
            if(consoleLog) { console.log(event.id + " : " + led4); }
            ledJsonText = '{ "' + event.id + '" : "' + led4 + '" }';
            break;
        case "ledcb5":
            led5 = document.getElementById("ledcb5").checked;
            if(consoleLog) { console.log(event.id + " : " + led5); }
            ledJsonText = '{ "' + event.id + '" : "' + led5 + '" }';
            break;
        case "ledcb6":
            led6 = document.getElementById("ledcb6").checked;
            if(consoleLog) { console.log(event.id + " : " + led6); }
            ledJsonText = '{ "' + event.id + '" : "' + led6 + '" }';
            break;
        case "ledcb7":
            led7 = document.getElementById("ledcb7").checked;
            if(consoleLog) { console.log(event.id + " : " + led7); }
            ledJsonText = '{ "' + event.id + '" : "' + led7 + '" }';
            break;
        case "ledcb8":
            led8 = document.getElementById("ledcb8").checked;
            if(consoleLog) { console.log(event.id + " : " + led8); }
            ledJsonText = '{ "' + event.id + '" : "' + led8 + '" }';
    }

    if ((ws != null) && (ws.readyState == WebSocket.OPEN)) {
        ws.send(ledJsonText);
    }
}

function MakeDataSocket() {
    if ("WebSocket" in window) {
        if ((ws == null) || (ws.readyState == WebSocket.CLOSED)) {
            ws = new WebSocket("wss://" + window.location.hostname + "/INDEX.HTML");
            ws.onopen = function() {};
            ws.onmessage = function(evt) {
                var rxMsg = evt.data;
                if(consoleLog) { console.log(rxMsg); }
                report = JSON.parse(rxMsg);

                dip1 = report.dipSwitches.dip1;
                dip2 = report.dipSwitches.dip2;
                dip3 = report.dipSwitches.dip3;
                dip4 = report.dipSwitches.dip4;
                dip5 = report.dipSwitches.dip5;
                dip6 = report.dipSwitches.dip6;
                dip7 = report.dipSwitches.dip7;
                dip8 = report.dipSwitches.dip8;

                updateCheckBox();
            };
            ws.onerror = function(error){
               console.log('WebSocket error detected: ' + error);
            }
            ws.onclose = function() {};
        }
    }
}
