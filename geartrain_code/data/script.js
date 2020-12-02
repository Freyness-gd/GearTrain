var plusButton = document.getElementById("plus_pwm");
var minusButton = document.getElementById("minus_pwm");
var pwmVal = document.getElementById("val_pwm");
var temp = document.getElementById("temp");
var hum = document.getElementById("hum");
var rpmval = document.getElementById("rpmval");
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

if (!!window.EventSource) {
  var source = new EventSource('/events');

  source.addEventListener('open', function(e) {
  console.log("Events Connected");
  }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
}

function initWebSocket() {
  console.log('Trying to open WebSocket Connection...');
  websocket = new WebSocket(gateway);
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage;
}

function onOpen(event) {
  console.log('Connection opened');
}

function onClose(event) {
  console.log('Connection closed');
  setTimeout(initWebSocket, 2000);
}

function onMessage(event) {
  console.log('Message Received');
}

window.addEventListener('load', onLoad);
function onLoad(event) {
  initWebSocket();
}

document.getElementById('start_program').onclick = function(){
  document.getElementById('program_stat').innerHTML = "1";
  console.log("Start");
  websocket.send('test');
}

function addPwm() {
  websocket.send('pwmadd');
  console.log("PWM +++");
}

function subPwm() {
  websocket.send('pwmsub');
  console.log("PWM ---");
}

source.addEventListener('updatePWM', function(e){
  console.log("PWM: ", e.data);
  pwmVal.innerHTML = e.data;
}, false);

source.addEventListener('humidity', function(e){
  console.log("Humidity: ", e.data);
  hum.innerHTML = e.data;
}, false);

source.addEventListener('temperature', function(e){
  console.log("Temperature: ", e.data);
  temp.innerHTML = e.data;
}, false);

source.addEventListener('rpm', function(e){
  console.log("RPM: ", e.data);
  rpmval.innerHTML = e.data;
}, false);
