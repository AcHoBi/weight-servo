#pragma once

const char PAGE[] PROGMEM = R"=="==(
<!DOCTYPE html>
<html>

<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    body {
      text-align: center;
      font-family: "Trebuchet MS", Arial;
      margin-left: auto;
      margin-right: auto;
    }
    
    table, th, td {
      border: 1px solid silver;
      border-collapse: collapse;
    }
    th, td {
      padding: 5px;
      text-align: center;    
    }

    .center {
      margin-left: auto;
      margin-right: auto;
    }
    input[type=range] {
      width: 300px;
    }
    
    input[type=number] {
      width: 75px;
    }

    .tooltip {
      position: relative;
      display: inline-block;
      border-bottom: 1px dotted black;
    }
    
    .tooltip .tooltiptext {
      visibility: hidden;
      width: 180px;
      background-color: #555;
      color: #fff;
      text-align: center;
      border-radius: 6px;
      padding: 5px 0;
      position: absolute;
      z-index: 1;
      bottom: 125%;
      left: 50%;
      margin-left: -60px;
      opacity: 0;
      transition: opacity 0.3s;
    }
    
    .tooltip .tooltiptext::after {
      content: "";
      position: absolute;
      top: 100%;
      left: 50%;
      margin-left: -5px;
      border-width: 5px;
      border-style: solid;
      border-color: #555 transparent transparent transparent;
    }
    
    .tooltip:hover .tooltiptext {
      visibility: visible;
      opacity: 1;
    }

    .status {
      padding: 20px;
      font-weight: 600;
      font-size: 2em;
    }

    .status.yellow .type {
      color: orange;
    }

    .status.red .type {
      color: red;
    }

    .status.green .type {
      color: green;
    }
  </style>
</head>

<body>
  <table class="center" style="width:300px;">
    <tr><td>
      <h1>Weight Serv<span id="Emoji" style='font-size:20px;'>&#x1F60E;</span></h1>
      <p>Load : <span id="pos"></span> %</p>
      <input type="range" min="0" max="100" class="slider" id="slider" value="100" step="1"/>
      <br /><br />
        <table class="center" style="width:300px;">
          <!-- <caption>Sweep Settings</caption> -->
          <tr>
            <th>Min</th>
            <th>Steps</th>
            <th>Delay</th>
            <th>Max</th>
          </tr>
          <tr>
            <td><span id="minLoopPos">???</span></td>
            <td><input type="number" id="steps"></td>
            <!-- minimum 1 second delay -->
            <td><input type="number" id="delay" min="1000"></td>
            <td><span id="maxLoopPos">???</span></td>
          </tr>
          <tr>
            <td><button onclick="setMinLoopPos()" id="minLoopPosBtn" title="MinPos">Set</button></td>
            <td colspan="2"><label><input type="checkbox" id="loop"> <div class="tooltip">Sweep<span class="tooltiptext">Duration: 5 minutes</span></span></div></label></td>
            <!--<td colspan="2"><label><input type="checkbox" id="loop"> Sweep</label></td>-->
            <td><button onclick="setMaxLoopPos()" id="maxLoopPosBtn" title="MaxPos">Set</button></td>
          </tr>
        </table>
        <div class="status yellow">
          <div class="type">CONNECTING</div>
          <div class="description"></div>
        </div>
    </td></tr>
  </table>
 
  <script>
    const $ = x => document.querySelector(x)

    URI = new URL(location)

    // constants
    VALUE = 0
    SLIDER = $("#slider")
    IS_LOOPING = $("#loop")
    POS = $("#pos")
    MIN_LOOP_POS = $("#minLoopPos")
    MAX_LOOP_POS = $("#maxLoopPos")
    STEPS = $("#steps")
    DELAY = $("#delay")
    EMOJI = $("#Emoji")
    STATUS = $(".status")
    STATUS_TYPE = $(".status .type")
    STATUS_DESCRIPTION = $(".status .description")
    STATUS_LOCKED = false

    // variables
    LOOP_TIMEOUT = null
    LOOP_GUARD_TIMEOUT = null
    LOOP_VELOCITY = null
    // stop sweep after 5 minutes -> 1000 * 60 * 5
    LOOP_GUARD_DELAY = 1000 * 60 * 5
    LOOP_DIRECTION = +1
    WS = new WebSocket(`ws://${URI.host}/ws`);
    
    function updateVelocity() {
      const min = Number(MIN_LOOP_POS.innerText)
      const max = Number(MAX_LOOP_POS.innerText)
      const delta = max - min
      LOOP_VELOCITY = delta / STEPS.value
      console.log({delta, LOOP_VELOCITY})
    }

    function updateValue() {
      SLIDER.value = VALUE
      POS.innerText = VALUE.toFixed(0)
    }

    function setValue(value) {
      VALUE = Number(value)
      updateValue()

      if (WS.readyState === 1) {
        WS.send(`${VALUE}`)
      }
    }

    function clamp(value, min, max) {
      return Math.min(Math.max(value, min), max)
    }

    function stepLoop() {
      let value = VALUE + LOOP_VELOCITY * LOOP_DIRECTION
      const min = Number(MIN_LOOP_POS.innerText)
      const max = Number(MAX_LOOP_POS.innerText)
      
      // we're shifting the limits by a small amount to 
      // account for floating point inaccuracy
      const EPSILON = 0.1

      if (value >= max - EPSILON) {
        value = Number(MAX_LOOP_POS.innerText)
        LOOP_DIRECTION = -1
      } else if (value <= min + EPSILON) {
        value = Number(MIN_LOOP_POS.innerText)
        LOOP_DIRECTION = +1
      }
      
      console.log("stepLoop", {value})
      setValue(value)
    }

    function stopLoop() {
      if (LOOP_TIMEOUT) {
        clearInterval(LOOP_TIMEOUT)
        LOOP_TIMEOUT = null
      }

      if (LOOP_GUARD_TIMEOUT) {
        clearTimeout(LOOP_GUARD_TIMEOUT)
        LOOP_GUARD_TIMEOUT = null
        IS_LOOPING.checked = false
      }

      // SMILING FACE WITH SUNGLASSES
      EMOJI.innerText =  String.fromCodePoint(0x1F60E)
    }
    
    function loopGuard() {
      stopLoop()
      // close socket, servo will go to default position
      WS.close(4001)
    }

    function startLoop() {
      stopLoop()
      LOOP_TIMEOUT = setInterval(stepLoop, Number(DELAY.value))
      LOOP_GUARD_TIMEOUT = setTimeout(loopGuard, LOOP_GUARD_DELAY)
      // SMILING FACE WITH HAND ON MOUTH
      EMOJI.innerText =  String.fromCodePoint(0x1F92D)
    }

    function updateURL() {
      window.history.pushState({}, null, URI.href)
    }

    function setMinLoopPos() {
      const value = SLIDER.value
      MIN_LOOP_POS.innerText = value
      URI.searchParams.set("min", value)
      updateURL()
      updateVelocity()
    }

    function setMaxLoopPos() {
      const value = SLIDER.value
      MAX_LOOP_POS.innerText = value
      URI.searchParams.set("max", value)
      updateURL()
      updateVelocity()
    }

    function setStatus(clas, type, description = "") {
      // if (STATUS_LOCKED) return null
      STATUS.className = "status " + clas
      STATUS_TYPE.innerText = type
      STATUS_DESCRIPTION.innerText = description
      STATUS_LOCKED = true
    }

    WS.addEventListener("error", (event) => {
      setStatus("red", "ERROR")
    })

    WS.addEventListener("close", (event) => {
      setStatus("red", "DISCONNECTED", event.code === 4000 ? "(only one client allowed)" : "")
    })

    WS.addEventListener("open", (event) => {
      setStatus("green", "CONNECTED")
    })

    async function setup() {
      MIN_LOOP_POS.innerText = URI.searchParams.get("min") || 0
      MAX_LOOP_POS.innerText = URI.searchParams.get("max") || 100
      STEPS.value = URI.searchParams.get("steps") || 10
      DELAY.value = URI.searchParams.get("delay") || 1000

      VALUE = Number(await fetch("/state").then(r => r.text()))
      if (Number.isNaN(VALUE)) {
        VALUE = 50
        // throw "NaN"
      }

      updateVelocity()
      updateValue()

      SLIDER.addEventListener("change", () => {
        setValue(SLIDER.value)
      })

      IS_LOOPING.addEventListener("change", () => {
        IS_LOOPING.checked ? startLoop() : stopLoop()
      })

      STEPS.addEventListener("change", () => {
        URI.searchParams.set("steps", STEPS.value)
        updateURL()
        updateVelocity()
      })

      DELAY.addEventListener("change", () => {
        DELAY.value = Math.max(1000, DELAY.value)
        URI.searchParams.set("delay", DELAY.value)
        updateURL()
      })
    }

    setup()
  </script>
</body>
</html>
)=="==";