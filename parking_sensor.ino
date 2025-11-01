#include <LowPower.h>

/*----- HC-SR04 Ultrasonic Sensor -----*/
const int TRIGGER_PIN = 9;
const int ECHO_PIN = 10;
#define MAX_DISTANCE 200 // Maximum distance (in cm) to ping.
#define PING_INTERVAL 33 // Milliseconds between sensor pings (29ms is about the min to avoid cross-sensor echo).
const float PING_TIMEOUT = 2.0 * MAX_DISTANCE / 0.0343; // (microseconds)
int t_last_ping = 0; // time of the last ping (ms)

/*--------------- LEDs ---------------*/
const int COME_LED = 6;
const int STOP_LED = 5;

// tunable parameters for performance
const float PARKED_DISTANCE = 10.0; // (cm)
const int N_SAMPLES = 10;
const float SAMPLING_FREQUENCY = 50; // (Hz)
const float MOVEMENT_DISTANCE_THRESHOLD = 2.0; // (cm)
// calculated constants from tuned parameters
const float SAMPLING_RATE = 1000.0 * (1 / SAMPLING_FREQUENCY); // (ms)
const float WAKE_DISTANCE = 120.0;

// low-pass filter parameters
const float CUTOFF_FREQUENCY = 50.0; // (Hz)

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

float lowPassFilter(float prior, float current_measurement) {
  // low-pass filter coefficients for a cutoff frequency of 50Hz
  // TODO: calculate these before setup based on the desired frequency
  float current = 0.7284895*prior + 0.13575525*current_measurement + 0.13575525;

  return current;
}

int guideDistance(int seconds_parked) {
  // the user is currently trying to park
  // guide them closer until they stop moving for a set amount of time
  // continually update distance until the user is parked
  float distance = measureDistance();
  if (distance <= PARKED_DISTANCE) {
    // tell the user to STOP
    digitalWrite(COME_LED, LOW);
    digitalWrite(STOP_LED, HIGH);
    // delay 1 second to prevent lights from flickering green/red when user is close to threshold distance
    delay(1000);

    // only add to seconds_parked if a valid distance measurement was received
    if (distance >= 0.0) seconds_parked += 1;
  } else {
    // tell the user to keep coming closer
    seconds_parked = 0;
    digitalWrite(COME_LED, HIGH);
    digitalWrite(STOP_LED, LOW);
  }

  return seconds_parked;
}

float measureDistance() {
  // ensure there has been a sufficient time since the last ping
  float t1 = millis();
  float t_since_last_ping = abs(t1 - t_last_ping); // abs() to account for millis() rollover (s)
  if (t_since_last_ping < PING_INTERVAL) {
    delay(PING_INTERVAL - t_since_last_ping);
  }
  t_last_ping = millis();
  
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
  float distance = (duration*.0343)/2; // (cm)

  if (distance == 0.0) {
    // if an object was not detected, denote an invalid measurement
    distance = -1.0;
  }

  Serial.println(distance);

  return distance;
}

bool checkMovement() {
  float distance0 = measureDistance();
  delay(100);
  float distance1 = measureDistance();

  bool moving;
  if (abs(distance1 - distance0) > MOVEMENT_DISTANCE_THRESHOLD) {
    moving = true;
  } else {
    moving = false;
  }

  return moving;
}

void loop() {
  // check if something is at a measurable distance away
  float distance;
  while (true) {
    distance = measureDistance();
    // if a measured distance is 
    if ((distance > WAKE_DISTANCE) || (distance < 0.0)) {
      LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
    } else {
      break;
    }
  }

  // TODO: add a timeout if the user hasnt hit the distance in a certain amount of time
  // otherwise once the green light comes on it stays on forever
  int seconds_parked = 0;
  while (seconds_parked <= 5) {
    seconds_parked = guideDistance(seconds_parked);
  }
  
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
