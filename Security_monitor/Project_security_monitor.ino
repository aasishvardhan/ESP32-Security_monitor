// For the circuit go to:https://app.cirkitdesigner.com/project/d76789f4-6846-4cf1-885b-f4adeda357bc
//––––––All Libraries––––––––––––––––––––––––  
  #include <Wire.h>
  #include <WiFi.h>
  #include <FastIMU.h>
  #include "esp_sleep.h"
  #include <WebServer.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_Sensor.h>
  #include <Adafruit_SH110X.h>
  #include <WebSocketsServer.h>
//––––––––––––––defining all properties/credentials of to be used objects–––––––––––––––––––––
  #define IMU_ADDRESS 0x68

  #define SCREEN_WIDTH 128
  #define SCREEN_HEIGHT 64

  #define BUTTON_PIN GPIO_NUM_33

  const char* SSID = "YOUR_WIFI_SSID";  //Wifi connectivity credentials 
  const char* password = "YOUR_WIFI_PASSWORD";

//––––––––––––Defining all the pins and objects to be used––––––––––––––––––––––––
  WebSocketsServer wsServer(81);  //websocket realted objects 
  WebServer httpServer(80);

  MPU6500 IMU;          //MPU 6500 reakted objects 
  calData calib = {0};
  AccelData accel;
  GyroData gyro;

  Adafruit_SH1106G display(SCREEN_WIDTH,SCREEN_HEIGHT,&Wire,-1);  //object for oled display

  int red = 15,green = 2,blue = 4;  //led pins 
  int buzzer = 19;  //buzzer pin
  int echopin = 13,trigpin = 12;  //HCSR04 pins
  int PIR_pin = 34; //pins used for interrupts 
  bool firstReading = true,vibrate = false,motion= false,proximity= false;
  //The three bools vibrate,motion,proximity represent if the system detects any of the following true for yes and 
  // false for no
  int dist=0;
  float prev_ax;  //measurements for vibration
  float prev_ay;
  float prev_az;
  enum Systemstate{
    DISARMED,ARMED,ALERT
  };
  RTC_DATA_ATTR Systemstate state = DISARMED;
//–––––funtions for measurement––––––––––––––––––––––––––––––––
  void distance(void* parametre){
    while(true){
      int duration=0;
      for(int i=0;i<10;i++){
        digitalWrite(trigpin,LOW);
        delayMicroseconds(2);
        digitalWrite(trigpin,HIGH);
        delayMicroseconds(10);
        digitalWrite(trigpin,LOW);
        duration += pulseIn(echopin,HIGH,30000);
      }
      dist = ((duration/10)/2)*0.0343;
      if( dist <30){
        proximity = true;
      }
      else proximity = false;
      vTaskDelay(pdMS_TO_TICKS(200));
    }
  }   //fucntion reads 10 values and updates the proximity bool 
  void vibration(void* parametre){
    while(true){
      IMU.update();
      IMU.getAccel(&accel);
      float ax = accel.accelX;
      float ay = accel.accelY;
      float az = accel.accelZ;
      if(firstReading)
      {
          prev_ax = ax;
          prev_ay = ay;
          prev_az = az;
          firstReading = false;
      }
      float delta =
          abs(ax - prev_ax) +
          abs(ay - prev_ay) +
          abs(az - prev_az);
      prev_ax = ax;
      prev_ay = ay;
      prev_az = az;
      if(delta >1){
        vibrate = true;
        state = ALERT;
      }
      else vibrate = false;
      vTaskDelay(pdMS_TO_TICKS(100));
      }
  }  //adjust the motion bool if it senses any vibration
     //adjust the delta value to change the magnitude of vibration detection

//––––ISR : for interrupts––––––––––––––––––––––––––––––
  void IRAM_ATTR On_button_press(){
    state = ARMED;
  }  //executed when button is pressed 
  void IRAM_ATTR On_PIR(){
    motion = true; 
  }  //executed when PIR sensor detects motion and updates the motion bool

//–––––Fucntions for handling of each state––––––––––––––––––
  void disp(void* parametre){
    while(true){
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SH110X_WHITE);
      display.setCursor(0,0);
      switch(state){
        case(DISARMED):{   //display if any motion is detected or not and goes to alert if it does 
          display.println("Checking for motion....");
          break;
        }
        case(ARMED):{   //for displaying stats during armed state 
          display.print("distance:");
          display.print(dist);
          display.println(" cm");
          display.print("Movement: ");
          if(motion) display.println("yes");
          else display.println("no");
          display.print("Vibration: ");
          if(vibrate) display.println("Yes");
          else display.println("No");
          break;
        }
        case(ALERT):{  //for displaying the reason for the alert
          if(digitalRead(34)){
            display.println("Motion detected!!!");
          }
          else if(vibrate){
            display.println("Vibration detected!!!");
          }
          else if(dist < 30){
            display.println("Proximity breached!!!");
          }
          break;
        }
      }
      display.display();
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }  //takes care of the displaying part on the screen depending on the state it is in
  void DISARM(){
    digitalWrite(green, HIGH);
    digitalWrite(red,   LOW);
    digitalWrite(blue,  LOW);

    // Clear any false triggers from PIR before warmup starts
    motion = false;

    // ── PIR warmup phase ─────────────────────────────────────────────────
    // Ignore all motion during this window — sensor is unstable
    Serial.println("PIR warming up — 5s");

    unsigned long warmupStart = millis();
    while (millis() - warmupStart < 5000) {

      // ← THE KEY FIX: exit immediately if anything changed state
      // This handles: web ARM button, physical button press
      if (state != DISARMED) {
        Serial.println("State changed during warmup — exiting DISARM");
        return;
      }

      motion = false;  // keep discarding false warmup triggers

      int remaining = (5000 - (millis() - warmupStart)) / 1000;
      Serial.printf("Warmup: %ds remaining\n", remaining);

      vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Warmup done — clear flag one final time
    motion = false;
    Serial.println("PIR ready. Watching for motion...");

    // ── Motion watch phase ───────────────────────────────────────────────
    // Watch for real motion for 10 seconds before deciding to sleep
    unsigned long watchStart = millis();
    while (millis() - watchStart < 10000) {

      // ← Same fix: exit if state changed from web or button
      if (state != DISARMED) {
        Serial.println("State changed during watch — exiting DISARM");
        return;
      }

      if (motion) {
        Serial.println("Motion confirmed — transitioning to ARMED");
        state = ARMED;
        return;
      }

      vTaskDelay(pdMS_TO_TICKS(200));
    }

    // ── Sleep phase ───────────────────────────────────────────────────────
    // Check one final time before committing to sleep
    // Handles the edge case of ARM pressed exactly as watch phase ends
    if (state != DISARMED) {
      Serial.println("State changed just before sleep — aborting sleep");
      return;
    }

    motion = false;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("No motion detected. Sleeping...");
    Serial.flush();
    esp_deep_sleep_start();
  }
  void ARM(){
    digitalWrite(blue,HIGH);
    digitalWrite(green,LOW);
    digitalWrite(red,LOW);
    if(vibrate||proximity||motion){
      motion = false;
      state = ALERT;
    }
  }
  void ALARM(){
    digitalWrite(red,HIGH);
    digitalWrite(blue,LOW);
    digitalWrite(green,LOW);
    unsigned long start = millis();
    while (millis() - start < 5000)
    {
      digitalWrite(buzzer, HIGH);
      vTaskDelay(pdMS_TO_TICKS(10));
    } 
    digitalWrite(buzzer, LOW);
    state = ARMED;
    while(digitalRead(34)==HIGH){
      vTaskDelay(pdMS_TO_TICKS(50));
    }
    motion = false;
  }
  void system_state(void* parametre){
    while(true){
      switch(state){
        case (DISARMED):{
          DISARM();
          break;
        }
        case (ARMED):{
          ARM();
          break;
        }
        case (ALERT):{
          ALARM();
          break;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
//––––––Website HTML script––––––––––––––––––––––––-
  const char* dashboardHTML = R"rawhtml(
  <!DOCTYPE html>
  <html>
  <head>
    <title>Security Monitor</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      * { box-sizing:border-box; margin:0; padding:0; }
      body { font-family:sans-serif; background:#0d1117; color:#c9d1d9; padding:20px; }
      h1   { color:#58a6ff; text-align:center; margin-bottom:20px; font-size:22px; }

      .state-banner {
        text-align:center; padding:16px; border-radius:12px;
        margin-bottom:20px; font-size:28px; font-weight:bold; transition:all 0.3s;
      }
      .state-DISARMED { background:#0f3d1f; color:#3fb950; border:2px solid #3fb950; }
      .state-ARMED    { background:#0d1f3d; color:#58a6ff; border:2px solid #58a6ff; }
      .state-ALERT    { background:#3d0d0d; color:#f85149; border:2px solid #f85149;
                        animation:pulse 1s infinite; }
      @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:0.6} }

      .grid { display:grid; grid-template-columns:1fr 1fr; gap:12px; margin-bottom:20px; }
      .card {
        background:#161b22; border:1px solid #30363d;
        border-radius:12px; padding:16px; text-align:center;
      }
      .card-label { font-size:11px; color:#8b949e; margin-bottom:8px;
                    text-transform:uppercase; letter-spacing:.05em; }
      .card-value { font-size:28px; font-weight:bold; }
      .ok    { color:#3fb950; }
      .alert { color:#f85149; }
      .info  { color:#58a6ff; }

      .bar-bg { background:#21262d; border-radius:4px; height:8px; margin-top:8px; overflow:hidden; }
      .bar    { height:100%; border-radius:4px; transition:width 0.3s, background 0.3s; }

      .btn-row { display:grid; grid-template-columns:1fr 1fr; gap:12px; margin-bottom:16px; }
      .btn {
        padding:14px; border:none; border-radius:10px;
        font-size:16px; font-weight:500; cursor:pointer;
      }
      .btn-arm    { background:#1f6feb; color:white; }
      .btn-disarm { background:#da3633; color:white; }

      .ws-status { text-align:center; font-size:12px; color:#8b949e; }
      .ws-status.ok  { color:#3fb950; }
      .ws-status.bad { color:#f85149; }
    </style>
  </head>
  <body>
    <h1>Security Monitor</h1>

    <div class="state-banner state-DISARMED" id="banner">DISARMED</div>

    <div class="grid">
      <div class="card">
        <div class="card-label">Distance</div>
        <div class="card-value info" id="dist">--</div>
        <div style="font-size:11px;color:#8b949e;margin-top:2px;">cm</div>
        <div class="bar-bg">
          <div class="bar" id="distBar" style="width:0%;background:#58a6ff;"></div>
        </div>
      </div>

      <div class="card">
        <div class="card-label">Motion</div>
        <div class="card-value ok" id="motion">NO</div>
      </div>

      <div class="card">
        <div class="card-label">Vibration</div>
        <div class="card-value ok" id="vibration">NO</div>
      </div>

      <div class="card">
        <div class="card-label">Proximity</div>
        <div class="card-value ok" id="proximity">CLEAR</div>
      </div>
    </div>

    <div class="btn-row">
      <button class="btn btn-arm"    onclick="send('ARM')">ARM</button>
      <button class="btn btn-disarm" onclick="send('DISARM')">DISARM</button>
    </div>

    <p class="ws-status bad" id="wsStatus">Connecting...</p>

    <script>
      const ws = new WebSocket('ws://' + window.location.hostname + ':81');

      function send(cmd) {
        if (ws.readyState === WebSocket.OPEN) ws.send(cmd);
      }

      ws.onopen = function() {
        const s = document.getElementById('wsStatus');
        s.textContent = 'Connected — live updates active';
        s.className = 'ws-status ok';
      };

      ws.onclose = function() {
        const s = document.getElementById('wsStatus');
        s.textContent = 'Disconnected — refresh to reconnect';
        s.className = 'ws-status bad';
      };

      ws.onmessage = function(event) {
        const d = JSON.parse(event.data);

        // State banner
        const banner = document.getElementById('banner');
        banner.textContent = d.state;
        banner.className = 'state-banner state-' + d.state;

        // Distance + bar (max range 200cm)
        document.getElementById('dist').textContent = d.distance;
        const pct = Math.min(d.distance / 200 * 100, 100);
        const bar = document.getElementById('distBar');
        bar.style.width  = pct + '%';
        bar.style.background = d.proximity ? '#f85149' : '#58a6ff';

        // Motion
        const motEl = document.getElementById('motion');
        motEl.textContent = d.motion ? 'YES' : 'NO';
        motEl.className = 'card-value ' + (d.motion ? 'alert' : 'ok');

        // Vibration
        const vibEl = document.getElementById('vibration');
        vibEl.textContent = d.vibration ? 'YES' : 'NO';
        vibEl.className = 'card-value ' + (d.vibration ? 'alert' : 'ok');

        // Proximity
        const proxEl = document.getElementById('proximity');
        proxEl.textContent = d.proximity ? 'BREACH' : 'CLEAR';
        proxEl.className = 'card-value ' + (d.proximity ? 'alert' : 'ok');
      };
    </script>
  </body>
  </html>
  )rawhtml";
//–––––handler funtions––––––––––––––––––––––––
  // ── WebSocket event handler ────────────────────────────────────────────────
    void onWebSocketEvent(uint8_t clientNum, WStype_t type,
                          uint8_t* payload, size_t length) {
      if (type == WStype_CONNECTED) {
        Serial.printf("Dashboard client %d connected\n", clientNum);
      }
      if (type == WStype_DISCONNECTED) {
        Serial.printf("Dashboard client %d disconnected\n", clientNum);
      }
      if (type == WStype_TEXT) {
        String msg = String((char*)payload);

        if (msg == "ARM") {
          state = ARMED;
          Serial.println("Remote: ARMED via dashboard");
        }
        if (msg == "DISARM") {
          state = DISARMED;
          Serial.println("Remote: DISARMED via dashboard");
        }
      }
    }

  // ── WiFi + WebSocket task (runs on Core 0) ─────────────────────────────────
    void wifiTask(void* parameter) {
      // HTTP server — serves the dashboard page
      httpServer.on("/", []() {
        httpServer.send(200, "text/html", dashboardHTML);
      });
      httpServer.begin();

      // WebSocket server
      wsServer.begin();
      wsServer.onEvent(onWebSocketEvent);

      Serial.println("Dashboard ready.");

      unsigned long lastBroadcast = 0;

      while (true) {
        httpServer.handleClient();
        wsServer.loop();

        // Broadcast sensor data every 500ms to all connected browsers
        if (millis() - lastBroadcast >= 500) {
          lastBroadcast = millis();

          // Build state string
          String stateStr;
          switch (state) {
            case DISARMED: stateStr = "DISARMED"; break;
            case ARMED:    stateStr = "ARMED";    break;
            case ALERT:    stateStr = "ALERT";    break;
          }

          // Build JSON with all sensor readings
          String json = "{\"state\":\""    + stateStr               + "\","
                         "\"distance\":"   + String(dist)            + ","
                         "\"motion\":"     + (motion    ? "true" : "false") + ","
                         "\"vibration\":"  + (vibrate   ? "true" : "false") + ","
                         "\"proximity\":"  + (proximity ? "true" : "false") + "}";

          wsServer.broadcastTXT(json);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }

void setup(){
  Serial.begin(115200);
  Serial.println("Boot");
  pinMode(red,OUTPUT);
  pinMode(green,OUTPUT);
  pinMode(blue,OUTPUT);
  pinMode(buzzer,OUTPUT);
  pinMode(trigpin,OUTPUT);
  pinMode(echopin,INPUT);
  pinMode(PIR_pin,INPUT);
  pinMode(BUTTON_PIN,INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN,On_button_press,FALLING);
  attachInterrupt(PIR_pin,On_PIR,RISING);
  Wire.begin();
    int err = IMU.init(calib,IMU_ADDRESS);  //MPU setup
    if(err!=0){
      Serial.println("MPU6500 is not connected");
      while(1);
    }  //if MPU does not work due to some issue it reports and stops any other code to run
  
    display.begin(0x3C,true);   //display setup
    display.clearDisplay();
    display.display();
 
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();  //this changes the state depending upon the reason it
  //woke up
  Serial.println(reason);
  switch(reason){
    case(ESP_SLEEP_WAKEUP_TIMER):{
      state = DISARMED;
      break;
    }
    case (ESP_SLEEP_WAKEUP_EXT0):{
      state = ARMED;
      break;
    }
  }
  esp_sleep_enable_timer_wakeup(10*1000000ULL);
  esp_sleep_enable_ext0_wakeup(BUTTON_PIN,0);

  WiFi.begin(SSID, password);    //wifi connection initiated 
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected: http://" + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi failed — continuing without network.");
  }

  xTaskCreatePinnedToCore(disp,"display",4096,NULL,1,NULL,1);   //setting up the priorities of the tasks
  xTaskCreatePinnedToCore(vibration,"vibration",4096,NULL,2,NULL,1);
  xTaskCreatePinnedToCore(distance,"distance",2048,NULL,2,NULL,1);
  xTaskCreatePinnedToCore(system_state,"state_handler",4096,NULL,3,NULL,1);
  // WiFi task on Core 0 — keeps it away from sensor tasks on Core 1
  xTaskCreatePinnedToCore(wifiTask, "wifi", 8192, NULL, 2, NULL, 0);

}
void loop(){
  vTaskDelay(pdMS_TO_TICKS(1000));
}