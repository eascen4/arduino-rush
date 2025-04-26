#include <Wire.h>

// I2C addresses
const byte INPUT_ARDUINO_ADDRESS = 0x08;
const byte DISPLAY_ARDUINO_ADDRESS = 0x09;

// Buzzer pin
const int BUZZER_PIN = 10;

// LED pins
const int LED_PINS[3] = {2, 3, 4}

// Constants
bool TEST_MODE = true;
const int LANE_COUNT = 3;
const int DISPLAY_WIDTH = 16;

const int MIN_UPDATE_INTERVAL = 50; // The fastest speed
const int MIN_OBSTACLE_INTERVAL = 800; // The fastest that obstancles spawn
const int DIFFICULTY_STEP = 10;

// State
bool isGameOver = false;
int playerLane = 1; // {0, 1, 2} -> {TOP, MIDDLE, BOTTOM}
unsigned long score = 0;

bool obstacles[LANE_COUNT][DISPLAY_WIDTH];

// Time state
unsigned long prevUpdateTime = 0;
int updateInterval = 200;

unsigned long prevObstacleTime = 0;
int obstacleInterval = 2000;

unsigned long lastFrameTime = 0;
unsigned long lastPerformanceReport = 0;

void setup() {
  Serial.begin(9600); // Debug
  Wire.begin();

  for(int i = 0; i < LANE_COUNT; i++) {
    for(int j = 0; j < DISPLAY_WIDTH; j++) {
      obstacles[i][j] = false;
    }
  }

  randomSeed(2);

  Serial.println("Game State Controller started...");
  Serial.println(TEST_MODE ? "TEST MODE" : "NORMAL MODE - Using I2C");
}

void createObstacle() {
  int safeLane = random(LANE_COUNT);
  for(int i = 0; i < LANE_COUNT; i++) {
    if (i != safeLane && random(100) < 60) { // 60% chance
      obstacles[i][DISPLAY_WIDTH - 1] = true;
    }
  }
}

void resetGame() {
  score = 0;
  playerLane = 1;
  isGameOver = false;
  updateInterval = 200;
  obstacleInterval = 2000;

  for(int i = 0; i < LANE_COUNT; i++) {
    for(int j = 0; j < DISPLAY_WIDTH; j++) {
      obstacles[i][j] = false;
    }
  }

  Serial.println("Game has been reset.");
}

void readInput() {
  if(TEST_MODE) {

    return;
  }

  Wire.requestFrom(INPUT_ARDUINO_ADDRESS, 1, true); // 1 byte

  if(Wire.available()) {
    byte inputData = Wire.read();

    if ((inputData & 0x80) == 0x80) { // Get bit 7 (1000 0000)
      resetGame();
      return;
    }

    if(!isGameOver) {
      byte movement = inputData & 0x0F; // Get bits 0-3 (0000 1111)

      // UP / DOWN
      if(movement == 1 && playerLane > 0) {
        playerLane--;
        Serial.print("Player moved UP to lane: ");
        Serial.println(playerLane);

      } else if (movement == 2 && playerLane < (LANE_COUNT - 1)) {
        playerLane++;
        Serial.print("Player moved DOWN to lane: ");
        Serial.println(playerLane);
      }
    }
  }
}

void sendGameState() {
  if(TEST_MODE) {
    _printGameState();
    return;
  }

  Wire.beginTransmission(DISPLAY_ARDUINO_ADDRESS);
  
  Wire.write(playerLane);
  Wire.write(isGameOver ? 1 : 0);
  
  for (int i = 0; i < LANE_COUNT; i++) {
    for (int j = 0; j < DISPLAY_WIDTH; j++) {
      Wire.write(obstacles[i][j] ? 1 : 0);
    }
  }
  
  Wire.endTransmission();
}

void playBuzzer() {
  tone(BUZZER_PIN, 1000, 200); // Play a tone for 200 milliseconds
}

void updateLEDs(int difficultyLevel) {
  for (int i = 0; i < 3; i++) {
    if (difficultyLevel > i * 2) { // Adjust multiplier for sensitivity
      digitalWrite(LED_PINS[i], HIGH);
    } else {
      digitalWrite(LED_PINS[i], LOW);
    }
  }
}

void update() {
  // Make all obstacles move one space to the left
  for(int i = 0; i < LANE_COUNT; i++) {
    for(int j = 0; j < DISPLAY_WIDTH - 1; j++) {
      obstacles[i][j] = obstacles[i][j + 1];
    }
    obstacles[i][DISPLAY_WIDTH - 1] = false;
  }

  if (obstacles[playerLane][0]) {
    isGameOver = true;
    Serial.println("GAME OVER!");
    playBuzzer();
    return;
  }

  score++;
  updateDifficulty();
}

void updateDifficulty() {
  int difficultyLevel = score / DIFFICULTY_STEP;
  updateInterval = max(MIN_UPDATE_INTERVAL, 200 - difficultyLevel * 10);
  obstacleInterval = max(MIN_OBSTACLE_INTERVAL, 2000 - difficultyLevel * 100);
  updateLEDs(difficultyLevel);
}

void loop() {
  unsigned long currTime = millis();
  unsigned long frameStart = currTime;

  readInput();
  
  if (!isGameOver) {

    if(currTime - prevUpdateTime >= updateInterval) {
      update();
      prevUpdateTime = currTime;
    }

    if(currTime - prevObstacleTime >= obstacleInterval) {
      createObstacle();
      prevObstacleTime = currTime;
    }
  } else if (TEST_MODE) {
    if(currTime - prevUpdateTime >= 10000) {
      resetGame();
      prevUpdateTime = currTime;
    }
  }

  sendGameState();

  lastFrameTime = millis() - frameStart;
}

void _printGameState() {
  unsigned long currTime = millis();
  if(currTime - lastPerformanceReport >= 1000) {
    
    Serial.print("Player in lane: ");
    Serial.println(playerLane);
    Serial.print("Score: ");
    Serial.println(score);
    
    Serial.println("Obstacles:");
    for (int i = 0; i < LANE_COUNT; i++) {
      Serial.print("Lane ");
      Serial.print(i);
      Serial.print(": ");
      for (int j = 0; j < DISPLAY_WIDTH; j++) {
        Serial.print(obstacles[i][j] ? "X" : (playerLane == i && j == 0) ? "O" : ".");
      }
      Serial.println();
    }

    Serial.print("ms | Frame time: ");
    Serial.print(lastFrameTime);
    Serial.println("ms");
    lastPerformanceReport = currTime;
  }
}
