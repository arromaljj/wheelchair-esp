// Web MQTT and UI logic for Wheelchair Web Controller

// MQTT connection parameters (WebSocket)
const MQTT_BROKER = 'wss://ceff3b2fc9074ac487a7ba2d62c24ef5.s1.eu.hivemq.cloud:8884/mqtt'; // Update port/path if needed

// MQTT Topics
const STATE_TOPIC = 'wheelchair/state';
const MOTOR_CMD_TOPIC = 'wheelchair/command/motor';
const EMERGENCY_CMD_TOPIC = 'wheelchair/command/emergency';

let client;
let isConnected = false;
let periodicInterval = null;

// DOM Elements
let statusEl, connContainer;
let manualLeftEl, manualRightEl, manualSendBtn;
let periodicLeftEl, periodicRightEl, periodicIntervalEl, periodicToggleBtn;
let emergencyStopBtn, emergencyStartBtn;
let stateLeftEl, stateRightEl;
let joystickMinEl, joystickMaxEl, joystickZone;

// Initialize on DOM ready
window.addEventListener('DOMContentLoaded', () => {
  // Grab elements
  connContainer = document.getElementById('connection-status');
  statusEl = document.getElementById('mqtt-status');

  manualLeftEl = document.getElementById('manual-left');
  manualRightEl = document.getElementById('manual-right');
  manualSendBtn = document.getElementById('manual-send');

  periodicLeftEl = document.getElementById('periodic-left');
  periodicRightEl = document.getElementById('periodic-right');
  periodicIntervalEl = document.getElementById('periodic-interval');
  periodicToggleBtn = document.getElementById('periodic-toggle');

  emergencyStopBtn = document.getElementById('emergency-stop');
  emergencyStartBtn = document.getElementById('emergency-start');

  stateLeftEl = document.getElementById('state-left');
  stateRightEl = document.getElementById('state-right');

  joystickMinEl = document.getElementById('joystick-min');
  joystickMaxEl = document.getElementById('joystick-max');
  joystickZone = document.getElementById('joystick-zone');

  // Setup UI callbacks
  manualSendBtn.addEventListener('click', sendManualCommand);
  periodicToggleBtn.addEventListener('click', togglePeriodic);
  emergencyStopBtn.addEventListener('click', () => publishSimple(EMERGENCY_CMD_TOPIC, 'STOP'));
  emergencyStartBtn.addEventListener('click', () => publishSimple(EMERGENCY_CMD_TOPIC, 'START'));

  // Initialize joystick control
  setupJoystick();

  // Connect to MQTT
  connectMQTT();
});

// MQTT Connection and callbacks
function connectMQTT() {
  const clientId = 'web-' + Math.random().toString(16).substr(2, 8);
  client = new Paho.MQTT.Client(MQTT_BROKER, clientId);

  client.onConnectionLost = onConnectionLost;
  client.onMessageArrived = onMessageArrived;

  const options = {
    userName: MQTT_USERNAME,
    password: MQTT_PASSWORD,
    useSSL: true,
    onSuccess: onConnect,
    onFailure: (err) => {
      console.error('MQTT connect failed:', err);
      updateStatus(false);
    }
  };

  client.connect(options);
}

function onConnect() {
  console.log('MQTT connected');
  updateStatus(true);
  // Subscribe to state topic
  client.subscribe(STATE_TOPIC, { qos: 1 });
}

function onConnectionLost(responseObject) {
  console.warn('MQTT connection lost:', responseObject.errorMessage);
  updateStatus(false);
  // Optionally try reconnect
}

function onMessageArrived(message) {
  // console.log('Message arrived:', message.destinationName, message.payloadString);
  if (message.destinationName === STATE_TOPIC) {
    try {
      const data = JSON.parse(message.payloadString);
      stateLeftEl.textContent = data.left_speed;
      stateRightEl.textContent = data.right_speed;
    } catch (e) {
      console.error('Invalid JSON in state message:', e);
    }
  }
}

function updateStatus(connected) {
  isConnected = connected;
  if (connected) {
    connContainer.classList.add('connected');
    statusEl.textContent = 'Connected';
  } else {
    connContainer.classList.remove('connected');
    statusEl.textContent = 'Disconnected';
  }
}

function publishSimple(topic, payload) {
  if (!isConnected) {
    console.warn('Not connected, cannot publish', topic, payload);
    return;
  }
  const message = new Paho.MQTT.Message(payload);
  message.destinationName = topic;
  client.send(message);
}

function publishJSON(topic, obj) {
  publishSimple(topic, JSON.stringify(obj));
}

// Manual command
function sendManualCommand() {
  const left = parseInt(manualLeftEl.value, 10) || 0;
  const right = parseInt(manualRightEl.value, 10) || 0;
  publishJSON(MOTOR_CMD_TOPIC, { left, right });
}

// Periodic command
function togglePeriodic() {
  if (periodicInterval === null) {
    const interval = parseInt(periodicIntervalEl.value, 10) || 200;
    periodicToggleBtn.textContent = 'Stop';
    periodicInterval = setInterval(() => {
      const left = parseInt(periodicLeftEl.value, 10) || 0;
      const right = parseInt(periodicRightEl.value, 10) || 0;
      publishJSON(MOTOR_CMD_TOPIC, { left, right });
    }, interval);
  } else {
    clearInterval(periodicInterval);
    periodicInterval = null;
    periodicToggleBtn.textContent = 'Start';
  }
}

// Joystick control
function setupJoystick() {
  const manager = nipplejs.create({
    zone: joystickZone,
    mode: 'static',
    position: { left: '50%', top: '50%' },
    color: 'blue',
    size: 300,
    restOpacity: 0.5
  });

  const maxDistance = manager.options.size / 2;
  const intervalTime = 30; // ms - Send command interval
  let joystickInterval = null;
  let isJoystickActive = false;
  let currentJoystickData = null; // Store the latest joystick data

  function publishJoystickPosition() {
    if (!isJoystickActive || !currentJoystickData || !currentJoystickData.vector) {
        return; // Don't publish if not active or no data
    }

    const normX = currentJoystickData.vector.x;   // -1 to 1
    const normY = currentJoystickData.vector.y;   // -1 to 1
    let factor = currentJoystickData.distance / maxDistance;
    if (factor > 1) factor = 1;

    const minSpeed = parseInt(joystickMinEl.value, 10) || 0;
    const maxSpeed = parseInt(joystickMaxEl.value, 10) || 100;

    // Mix for differential drive (Flipped Left/Right)
    // Y-axis (normY) controls forward/backward speed.
    // X-axis (normX) controls turning.
    let leftNorm = (normY - normX) / 2 * factor;  // Steering: Positive X (right) decreases left speed
    let rightNorm = (normY + normX) / 2 * factor; // Steering: Positive X (right) increases right speed

    const leftSpeed = Math.sign(leftNorm) * (minSpeed + (maxSpeed - minSpeed) * Math.abs(leftNorm));
    const rightSpeed = Math.sign(rightNorm) * (minSpeed + (maxSpeed - minSpeed) * Math.abs(rightNorm));

    publishJSON(MOTOR_CMD_TOPIC, { left: Math.round(leftSpeed), right: Math.round(rightSpeed) });
  }

  manager.on('start', (evt, data) => {
    console.log("Joystick start");
    isJoystickActive = true;
    currentJoystickData = data; // Store initial data
    // Clear any previous interval just in case
    if (joystickInterval) clearInterval(joystickInterval);
    // Start sending periodically
    joystickInterval = setInterval(publishJoystickPosition, intervalTime);
  });

  manager.on('move', (evt, data) => {
    // Just update the data, the interval function will use it
    currentJoystickData = data;
  });

  manager.on('end', () => {
    console.log("Joystick end");
    isJoystickActive = false;
    currentJoystickData = null;
    // Stop the interval timer
    if (joystickInterval) {
      clearInterval(joystickInterval);
      joystickInterval = null;
    }
    // Send a final stop command
    publishJSON(MOTOR_CMD_TOPIC, { left: 0, right: 0 });
  });
} 