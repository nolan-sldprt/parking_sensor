#include <LowPower.h>

/*----- HC-SR04 Ultrasonic Sensor -----*/
const int TRIGGER_PIN = 9;
const int ECHO_PIN = 10;
#define MAX_DISTANCE 200 // Maximum distance (in cm) to ping.
#define PING_INTERVAL 33 // Milliseconds between sensor pings (29ms is about the min to avoid cross-sensor echo).
const float PING_TIMEOUT = 2.0 * MAX_DISTANCE / 0.0343; // (microseconds)
int t_last_ping = 0; // time of the last ping (ms)

/*---------- Low-pass Filter ----------*/
const float CUTOFF_FREQUENCY = 15.0; // (Hz)
float dt = 0.0; // time between the last two pings (s)
float ultrasonic_prior = -1.0;

/*--------------- LEDs ---------------*/
const int COME_LED = 6;
const int STOP_LED = 5;

// tunable parameters for performance
const float PARKED_DISTANCE = 10.0; // (cm)
const int PARKED_TIME = 5; // (s)
const int PARKING_TIMEOUT = 20000; // (ms)
const float MOVEMENT_DISTANCE_THRESHOLD = 2.0; // (cm)
const float WAKE_DISTANCE = 120.0; // (cm)

void setup() {
  // setup the HC-SR04 ultrasonic sensor
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // setup the LEDs
  pinMode(COME_LED, OUTPUT);
  pinMode(STOP_LED, OUTPUT);
  
  Serial.begin(9600);
  Serial.println("beginning logging");
}

float lowPassFilter(float measurement) {
  // calculate the filtering coefficient
  float a = exp(-2.0f * PI * CUTOFF_FREQUENCY * dt); // filtering coefficient, [0 <= a <= 1]
  // filter and update the state prior
  ultrasonic_prior = a * ultrasonic_prior + (1.0f - a) * measurement;

  return ultrasonic_prior;
}

int guideDistance(int seconds_parked) {
  /*
    guide the user closer until they stop moving for a set amount of time
    continually update distance until the user is parked
  */
  
  float distance = measureDistance();
  if (distance >= 0.0) {
    distance = lowPassFilter(distance);
  }
  
  if (distance <= PARKED_DISTANCE) {
    // tell the user to STOP
    digitalWrite(COME_LED, LOW);
    digitalWrite(STOP_LED, HIGH);
    // delay 0.5 seconds to prevent lights from flickering green/red when user is close to threshold distance
     delay(500);

    // only add to seconds_parked if a valid distance measurement was received
    if (distance >= 0.0) seconds_parked += 1;
  } else {
    // reset the parked counter
    seconds_parked = 0;
    
    // tell the user to keep coming closer
    digitalWrite(COME_LED, HIGH);
    digitalWrite(STOP_LED, LOW);
  }

  return seconds_parked;
}

float measureDistance() {
  // ensure there has been a sufficient time since the last ping
  float t_since_last_ping = abs(millis() - t_last_ping); // abs() to account for millis() rollover (ms)
  if (t_since_last_ping < PING_INTERVAL) {
    delay(PING_INTERVAL - t_since_last_ping);
  }
  
  // get the current time of the ping
  float t_current_ping = millis();
  // update dt for low-pass filtering
  dt = (t_current_ping - t_last_ping) / 1000; // (s)
  // update the time of last ping
  t_last_ping = t_current_ping;
  
  // make sure the trigger pin is not already firing
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  // transmit for 10 microseconds
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  // turn the transmitter off
  digitalWrite(TRIGGER_PIN, LOW);
  
  // detect time-of-flight (TOF) of pulse
  float duration = pulseIn(ECHO_PIN, HIGH, PING_TIMEOUT); // (microseconds)
  // convert two-way TOF to one-way distance using speed of sound in air
  // TODO: document/comment on this and explain value and units
  float distance = (duration*.0343)/2; // (cm)

  if (distance == 0.0) {
    // if an object was not detected, denote an invalid measurement
    distance = -1.0;
  }

  Serial.println(distance);

  return distance;
}

bool checkForwardMovement() {
  // make two ultrasonic measurements with a delay between them to check for movement
  float distance0 = measureDistance();
  delay(100);
  float distance1 = measureDistance();

  bool moving;
  if ((distance1 - distance0) > MOVEMENT_DISTANCE_THRESHOLD) {
    moving = true;
  } else {
    moving = false;
  }

  return moving;
}

void endParking() {
  // tell the user parking is complete
  digitalWrite(COME_LED, LOW);
  digitalWrite(STOP_LED, LOW);
  delay(300);
  digitalWrite(COME_LED, HIGH);
  digitalWrite(STOP_LED, HIGH);
  delay(1000);
  // turn off LEDs and power down to conserve energy
  digitalWrite(COME_LED, LOW);
  digitalWrite(STOP_LED, LOW);
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}

void loop() {
  // check if something is at a measurable distance away
  float distance;
  while (true) {
    distance = measureDistance();
    // check if the measured distance should wake the system
    if ((distance > WAKE_DISTANCE) || (distance < 0.0)) {
      LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
      delay(100); // TODO: only here for debugging (lowpower messes with Serial
    } else {
      break;
    }
  }

  // guide the user into the parking spot
  int seconds_parked = 0;
  int t_start_parking = millis(); // timeout to prevent the system from staying in this loop forever (ms)
  while ( (seconds_parked <= 5) && ((millis() - t_start_parking) <= PARKING_TIMEOUT) ){
    seconds_parked = guideDistance(seconds_parked);
  }
  
  endParking();
}
