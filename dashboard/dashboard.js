/* globals Chart:false, feather:false */
var g_soc;
var g_gridP;
var g_chargeP;
var g_invP;
var g_BatP;
var g_mqttSuccess;
var g_mqttIP;
var g_mqttPort;
var g_mqttEnable;
var g_update_network_page = true;
var g_update_batteryset_page = true;
var dataSet; // = [['Example', '123'], ['Example2', 'abc']];
var g_DynInvP;
var g_sol_progn;
var g_maxPInv;
var g_maxPCharg;
var g_PresCharg;
var g_PresInv;
var g_PmanCharg;
var g_enPManCharg;
var g_enDynload;
var Socket;
var doUpdateNetwork = true;


function init() {
  //  Socket = new WebSocket('ws://' + window.location.hostname + ':81/');
  console.log("Init started");
  Socket = new WebSocket("ws://192.168.188.85:81/");
  Socket.onmessage = function (event) {
    processCommand(event);
    //    console.log(event.data);
  };
  Socket.onclose = function () {
    // connection closed, discard old websocket and create a new one in 2s
    Socket = null;
    setTimeout(init, 2000);
  };
}

function enable_network_settings() {
  $("#testMQTTbtn").prop("disabled", false).removeClass("disabled");
  $("#MQTTserverip").prop("disabled", false).removeClass("disabled");
  $("#MQTTserverport").prop("disabled", false).removeClass("disabled");
  $("#saveMQTTbtn").prop("disabled", false).removeClass("disabled");
  $("#resetWifibtn").prop("disabled", false).removeClass("disabled");
}

function disable_network_settings() {
  // $('input[type=button]').prop('disabled', true);
  $("#testMQTTbtn").prop("disabled", true);
  $("#MQTTserverip").prop("disabled", true);
  $("#MQTTserverport").prop("disabled", true);
  $("#saveMQTTbtn").prop("disabled", true);
  $("#resetWifibtn").prop("disabled", true);
}

function testMQTT() {
  g_mqttIP = document.getElementById("MQTTserverip");
  g_mqttPort = document.getElementById("MQTTserverport");
  g_mqttEnable = document.getElementById("MQTTenable").checked;
  if (g_mqttEnable) {
    console.log(g_mqttIP.value);
  }
  //  console.log(g_mqttPort.value);
  //  console.log(g_mqttIP.value);

  let msg = {
    command: "test_mqtt",
    ip: g_mqttIP.value,
    port: g_mqttPort.value,
  };
  console.log(msg);
  g_update_network_page = true;
  Socket.send(JSON.stringify(msg));
}

function saveMQTTsettings() {
  g_mqttIP = document.getElementById("MQTTserverip").value;
  g_mqttPort = document.getElementById("MQTTserverport").value;
  g_mqttEnable = document.getElementById("MQTTenable").checked;
  disable_network_settings();
  let msg = {
    command: "set_mqtt",
    ip: g_mqttIP,
    port: g_mqttPort,
    enable: g_mqttEnable,
  };
  //console.log(msg);
  g_update_network_page = true;
  Socket.send(JSON.stringify(msg));
}

function updatePage() {
  var currentLocation = window.location.pathname;
  console.log(currentLocation);

  // Update header and sidebar

  if (currentLocation.length === 0 || currentLocation === "/" || currentLocation.endsWith("index.html") || currentLocation.endsWith("#index")) {
    //    console.log("processing index page");
    //document.getElementById("vGrid").innerHTML = g_gridP;
    document.getElementById("vGrid").value    = g_gridP + "W";
    document.getElementById("vSOC").value = g_soc + "%";
    document.getElementById("vSOCbar").setAttribute("style", "width:" + Number(g_soc) + "%");
    document.getElementById("vCharge").value  = g_chargeP + "W";
    document.getElementById("vInv").value = g_invP + "W";
    document.getElementById("vBatt").value = g_BatP + "W";


  }
  else if (currentLocation.endsWith("set_network.html") &  g_update_network_page & (g_mqttIP !== undefined)) {
    //    console.log("processing network settings page");
    enable_network_settings();
    //    console.log(g_mqttSuccess);
    if (g_mqttSuccess == true) {
      $("#testMQTTbtn").removeClass("btn-danger").addClass("btn-success");
      document.getElementById("testMQTTbtn").innerHTML = "Success";
      $("#indicator_mqtt").removeClass("btn-secondary").addClass("btn-success").tooltip("hide").attr("data-original-title", "MQTT server connected");
    } 
    else {
      $("#testMQTTbtn").removeClass("btn-success").addClass("btn-warning");
      document.getElementById("testMQTTbtn").innerHTML = "Failed";
    }
    // $("#MQTTserverip").val( g_mqttIP);  -- uüdate of the value is also possible in this way
    document.getElementById("MQTTserverip").value = g_mqttIP;
    document.getElementById("MQTTserverport").value = g_mqttPort;
    g_update_network_page = false;
  } else if (currentLocation.endsWith("set_battery.html") & g_update_batteryset_page) {
    document.getElementById("maxPInvVal").innerHTML = g_maxPInv + "W";
    document.getElementById("maxPInv").value = g_maxPInv;
    document.getElementById("maxPChargVal").innerHTML = g_maxPCharg + "W";
    document.getElementById("maxPCharg").value = g_maxPCharg;
    document.getElementById("PresChargVal").innerHTML = g_PresCharg + "W";
    document.getElementById("PresCharg").value = g_PresCharg;
    document.getElementById("PresInvVal").innerHTML = g_PresInv + "W";
    document.getElementById("PresInv").value = g_PresInv;
    document.getElementById("manPChargVal").innerHTML = g_PmanCharg + "W";
    document.getElementById("manPCharg").value = g_PmanCharg;
    document.getElementById("enDynControl").checked =  g_enDynload;
    //document.getElementById("enManControl").checked =  g_enManload;
  } else if (currentLocation.endsWith("stat_battery.html")) {

  }
}



function processCommand(event) {
  //  console.log("receiving data");
  var obj = JSON.parse(event.data);
  // dataSet = json2array(obj);
  dataSet = obj;

  //  console.log(dataSet);

  g_gridP = parseInt(obj.gridP);
  g_enDynload = Boolean(obj.enDynload);
  g_soc = parseInt(obj.SOC);
  g_chargeP = parseInt(obj.chargeP);
  g_invP = parseInt(obj.invP);
  g_BatP = parseInt(obj.Battery_Power);
  g_mqttSuccess = Boolean(obj.mqttSuccess);
  g_mqttIP = obj.mqttIP;
  g_mqttPort = parseInt(obj.mqttPort);
  g_mqttEnable = Boolean(obj.mqttEnable);
  g_DynInvP = parseInt(obj.DynInvP);
  g_sol_progn = parseInt(obj.sol_progn);
  g_maxPInv = parseInt(obj.maxPInv);
  g_maxPCharg = parseInt(obj.maxPCharg);
  g_PresCharg = parseInt(obj.PresCharg);
  g_PresCharg = parseInt(obj.PresCharg);
  g_PresInv = parseInt(obj.PresInv);
  g_PmanCharg = parseInt(obj.PmanCharg);
  g_enPManCharg = Boolean(obj.enPManCharg);

  //  console.log("read message: ");
  //  console.log(g_enDynload);
  updatePage();

}



window.addEventListener
  ? window.addEventListener("load", function () {
    console.log("Site has been loaded");
    localStorage.clear();
    //      sessionStorageStorage.clear();
    //      alert(window.onload);
    init();
  })
  : window.attachEvent &&
  window.attachEvent("onload", function () {
    console.log("Site has been loaded");
    //      alert(window.onload);
    init();
  });