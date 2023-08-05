/* main code
created by : mohammad farahany 
2023\7\30
*/

#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <esp_system.h> // only for esp 32
#include <esp_task_wdt.h> // only for esp 32
#include "BasicStepperDriver.h"
//*************************set pins********************************
#define step_pin 22
#define hotend_pin 2
#define run_out_pin 21
#define ADC_PIN 36 // pt100 pin from termistor on the board
//*****************************************************************

//*****************************************************************
#define micro_steps 32
//*****************************************************************

//***********************set ssid and password *******************
const char* ssid = "filapet";
const char* password = "12345678";
//******************************************************************

/* temp count
created by : mohammad farahany 
2023\7\28

by : dataBase.xlsx

electrical circuit :

gnd --- R2 ----- termistor ----- vcc(V1)
             |
             |
             |
          ADC_PIN

the termistor is : pt100

about the R2 :
if you want to measure x ohms to y ohms it should be about x+y/2
in this code 170 to 300 Degrees Celsius is Important 
and the resistance is 1k to 100 ohms so 470 ohm is very god for hear

*/


//********************* config this values *********************
#define V1 3.3 // termistor input voltag = max voltage of ADC_PIN
#define ADC_RESOLUTION 4096 // ADC resolution (in esp32 = 4096)
#define R2 470 // R2 value (ohms)
#define ohm_eror -40 // eroe of ohm 
#define tmp_eror 0 // eroe of temp
//**************************************************************



float  x ;
float y ;

int speed = 50; 
int RPM = 200; 
int temp = 270; 

int address = 1;
int address2 = 2;

bool run_out_mode = LOW;

String mode = "working";

BasicStepperDriver stepper(100, 12, 22); // Define the stepper object here

WebServer server(80); // Declare the server object outside setup()

void setup() {
  IPAddress local_ip(192, 168, 1, 1);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);

  Serial.begin(115200); 

  EEPROM.begin(512);

  if (EEPROM.read(address) > 0) {
    EEPROM.get(address, temp);
  }

  if (EEPROM.read(address2) > 0) {
    EEPROM.get(address2, speed);
  }

  pinMode(step_pin, OUTPUT);  
  pinMode(hotend_pin, OUTPUT); 
  pinMode(run_out_pin, INPUT); 
  pinMode(ADC_PIN, INPUT);  

  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);

  Serial.print("Connect to My access point: ");
  Serial.println(ssid);

  server.on("/", handle_root);

  server.begin();
  Serial.println("HTTP server started");
  delay(100);
}

void loop() {
  server.handleClient();
  server_insert() ;
  run_out();
  heater();
  speed_set();
}

const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <style>
       body {
      background-color: lightblue;
      color: white;
      text-align: center;
    }

    h3 {
      color: orange;
      font-size: 42px;
    }

    form {
      background-color: #333;
      padding: 30px;
    }

    input[type="number"] {
      color: white;
      background-color: #444;
      padding: 8px;
      padding-right: 20px; 
      padding-left: 20px; 
      border: none;
      width: 80px; 
      border-radius: 10px;
    }

    input[type="submit"] {
      background-color: #FF6347;
      color: white;
      font-size: 24px;
      padding: 12px 24px;
      border: 2px solid #FF6347;
      border-radius: 10px;
      cursor: pointer;
    }

    p {
      font-size: 20px;
      color: black;
    }

    .toggle-container {
      display: flex;
      flex-direction: column;
      align-items: center;
      margin-top: 20px;
    }

    .toggle {
      position: relative;
      display: block;
      width: 100px;
      height: 35px;
      color: white;
      outline: 0;
      text-decoration: none;
      border-radius: 100px;
      border: 2px solid #546E7A;
      background-color: #263238;
      transition: all 500ms;
    }

    .toggle:active {
      background-color: #1c2429;
    }

    .toggle:hover:not(.toggle--moving):after {
      background-color: #455A64;
    }

    .toggle:after {
      display: block;
      position: absolute;
      top: 39px;
      left 1PX;
      right: -70PX;
      width: 240%;
      font-size: 20px;
      color: white;
      content: 'Run Out sensor '; 
    }

    .toggle--on {
      background-color: #66BB6A;
    }

    .toggle--off {
      background-color: #f44336;
    }

    .toggle--moving {
      background-color: #1c2429;
    }

    .toggle--moving:after {
      color: transparent;
      border-color: #435862;
      background-color: #222c31;
      transition: color 0s, transform 500ms, border-radius 500ms, background-color 500ms;
    }

    .toggle::before {
      content: '';
      display: block;
      position: absolute;
      top: 50%;
      left: -5px;
      transform: translateY(-50%);
      width: 35px;
      height: 35px;
      border-radius: 50%;
      background-color: white;
      transition: left 500ms;
    }

    .toggle--on::before {
      left: calc(65%);
    }

    .toggle--off::before {
      left: -5px;
    }/
  </style>
  <script>
    document.addEventListener('DOMContentLoaded', function() {
      const toggle = document.querySelector('.toggle');
      let run_out_mode = 0;

      function updateToggleAnimation() {
        const runOutModeField = document.querySelector('input[name="run_out_mode"]');

        if (toggle.classList.contains('toggle--off')) {
          toggle.classList.remove('toggle--on');
          toggle.classList.add('toggle--off');
          run_out_mode = 0;
        } else {
          toggle.classList.remove('toggle--off');
          toggle.classList.add('toggle--on');
          run_out_mode = 1;
        }

        console.log('run_out_mode:', run_out_mode);
        runOutModeField.value = run_out_mode;
      }

      toggle.addEventListener('click', function() {
        updateToggleAnimation();
      });


      setTimeout(updateToggleAnimation, 0);
    });
  </script>
</head>

<body>
<body>
  <h3>Control the Project</h3>
  <form action="/" method="get">
    Speed: <input type="number" name="speed" min="0" max="100" value="10"><br><br>
    Temperature: <input type="number" name="temp" min="0" max="700" value="300"><br><br>
    <input type="hidden" name="run_out_mode" value="0">
    <input type="submit" value="Submit">
    <div class="toggle-container">
      <a href="#" class="toggle toggle--off"></a>
    </div>
  </form>
  <p>The temp is: @@ANALOG@@</p>
  <p>mode is: @@MODE@@</p>
  <script>
    document.addEventListener('DOMContentLoaded', function() {
      const toggle = document.querySelector('.toggle');
      let run_out_mode = 0;

      function updateToggleAnimation() {
        if (toggle.classList.contains('toggle--off')) {
          toggle.classList.remove('toggle--off');
          toggle.classList.add('toggle--on');
          run_out_mode = 1;
        } else {
          toggle.classList.remove('toggle--on');
          toggle.classList.add('toggle--off');
          run_out_mode = 0;
        }

        console.log('run_out_mode:', run_out_mode);

        const runOutModeField = document.querySelector('input[name="run_out_mode"]');
        runOutModeField.value = run_out_mode;
      }

      toggle.addEventListener('click', function() {
        updateToggleAnimation();
      });

      setTimeout(updateToggleAnimation, 0);
    });
  </script>
</body>
</html>




)=====";

void handle_root() {
  String page = String(MAIN_page); 
  page.replace("@@MODE@@", mode);  
  page.replace("@@ANALOG@@", String(temp_now()));  
  server.send(200, "text/html", page);
}

void heater() {
  if (temp_now() < temp) {
    digitalWrite(hotend_pin, HIGH);
  } else {
    digitalWrite(hotend_pin, LOW);
  }
}

void server_insert() {
  if (server.hasArg("speed") && server.hasArg("temp")) {
    speed = server.arg("speed").toInt();
    temp = server.arg("temp").toInt(); 
    run_out_mode =  server.arg("run_out_mode").toInt(); 
    EEPROM.put(address, temp);
    EEPROM.put(address2, speed);
    EEPROM.commit();
  }  
}

void run_out() {
  if (run_out_mode == HIGH) {
    if (!digitalRead(run_out_pin)) {
      speed = 0;
      temp = 0;
      mode = "stop";
    }
    else 
    {
      mode = "working";
    }
  }
   else 
    {
      mode = "working";
    }
}

void speed_set() {
  RPM = map(speed, 0, 100, 0, 400);
  stepper.begin(RPM, micro_steps); // Update the RPM value here
  stepper.rotate(360);
  stepper.move(-100 * micro_steps);
}

float temp_now()
{
  x = (V1 * R2 / ((V1 / ADC_RESOLUTION * 1.000000000) * analogRead(ADC_PIN) )) - R2 ;
 y = (2.7542 * (x+ohm_eror) )- 272.73 ;
  return y + tmp_eror ;
}