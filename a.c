
const int PWM_PIN = 9;       // DRV8874 EN/IN1
const int DIR_PIN = 8;       // DRV8874 PH/IN2

const int ENC_A = 2;         // Encoder channel A
const int ENC_B = 3;         // Encoder channel B


const float COUNTS_PER_REV = 700.0;

const float TARGET_RPM = 3.0;

// Encoder counter
volatile long encoderCount = 0;

// Controller values
float integral = 0;

// These are starting values.
float Kp = 4.0;
float Ki = 1.0;

// Approximate PWM required to overcome motor friction.
float FEED_FORWARD = 22.0;

unsigned long lastUpdate = 0;
long lastEncoderCount = 0;

const unsigned long SAMPLE_TIME = 250; // milliseconds


void encoderISR()
{
  // Use channel B to determine direction
  if (digitalRead(ENC_B))
    encoderCount++;
  else
    encoderCount--;
}


void setup()
{
  Serial.begin(115200);

  pinMode(PWM_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);

  pinMode(ENC_A, INPUT);
  pinMode(ENC_B, INPUT);

  // Fixed direction
  digitalWrite(DIR_PIN, HIGH);

  // Count one rising edge of encoder A
  attachInterrupt(
    digitalPinToInterrupt(ENC_A),
    encoderISR,
    RISING
  );

  // Motor initially stopped
  analogWrite(PWM_PIN, 0);

  delay(1000);

  lastUpdate = millis();
}


void loop()
{
  unsigned long now = millis();

  if (now - lastUpdate >= SAMPLE_TIME)
  {
    unsigned long dt = now - lastUpdate;
    lastUpdate = now;

    // Safely copy encoder count
    noInterrupts();
    long currentCount = encoderCount;
    interrupts();

    long countDifference =
        currentCount - lastEncoderCount;

    lastEncoderCount = currentCount;

    // Calculate RPM
    float rpm =
        abs(countDifference) *
        60000.0 /
        (COUNTS_PER_REV * dt);

    // -------------------------
    // PI speed controller
    // -------------------------

    float error = TARGET_RPM - rpm;

    integral += error * (dt / 1000.0);

    // Prevent integral wind-up
    integral = constrain(integral, -50.0, 50.0);

    float pwm =
        FEED_FORWARD +
        Kp * error +
        Ki * integral;

    pwm = constrain(pwm, 0, 255);

    analogWrite(PWM_PIN, (int)pwm);

    // Show what is happening
    Serial.print("Target: ");
    Serial.print(TARGET_RPM, 2);

    Serial.print(" RPM | Actual: ");
    Serial.print(rpm, 2);

    Serial.print(" RPM | PWM: ");
    Serial.println(pwm, 1);
  }
}