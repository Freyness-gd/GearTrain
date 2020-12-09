var startButton = document.getElementById("start_program");
var fansButton = document.getElementById("enable_fans");
var motorButton = document.getElementById("enable_motor");
var directionButton = document.getElementById("motor_dir");
var plusButton = document.getElementById("plus_pwm");
var minusButton = document.getElementById("minus_pwm");
var ledButton = document.getElementById("led_state");

var pwmVal = document.getElementById("val_pwm");
var temp = document.getElementById("temp");
var hum = document.getElementById("hum");
var rpmval = document.getElementById("rpmval");
var errorField = document.getElementById("error");

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

startButton.onclick = function(){
  console.log("Start");
  websocket.send('prgStart');
}

fansButton.onclick = function(){
  console.log("FanStart");
  websocket.send('fans');
}

motorButton.onclick = function(){
  console.log("EnMotor");
  websocket.send('enmotor');
}

directionButton.onclick = function(){
  console.log("Change Direction");
  websocket.send('dirmotor');
}

ledButton.onclick = function(){
  console.log("LED");
  websocket.send('led');
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

source.addEventListener('error', function(e){
  console.log("Error: ", e.data);
  errorField.innerHTML = e.data;
}, false);
