#include "mbed.h"  // Include the mbed library for platform-specific functions and definitions

#define ERROR_THRESHOLD 0.2 // Define the error threshold for sensor readings (20% error range)

// Note definitions for the melody
#define NOTE_C2   65    // Define the frequency of note C2
#define NOTE_E2   82    // Define the frequency of note E2
#define NOTE_G2   98    // Define the frequency of note G2
#define NOTE_A2   110   // Define the frequency of note A2
#define NOTE_C3   131   // Define the frequency of note C3
#define NOTE_E3   165   // Define the frequency of note E3
#define NOTE_G3   196   // Define the frequency of note G3
#define NOTE_A3   220   // Define the frequency of note A3

// Define note duration in milliseconds
#define SIXTEENTH_NOTE_DURATION 125  // Define the duration of a sixteenth note

bool cycleComplete = false; // Flag to track cycle completion

// Initialize serial communication with the PC
BufferedSerial pc(USBTX, USBRX, 115200);

// Initialize analog input pins and digital pins for various components
AnalogIn FSR(PA_1);                      // Force sensor pin
DigitalIn button_door(PC_11);            // Button pin for door
DigitalOut lock_led(PA_4);               // Red LED pin for lock
BusOut segments(PA_11, PA_12, PB_1, PB_15, PB_14, PB_12, PB_11, PB_2); // 7-segment display pins

// Define RGB LED objects for LED colors
PwmOut red_LED(PB_3);
PwmOut green_LED(PB_5);
PwmOut blue_LED(PB_4);

// Initialize the on/off switch pin
DigitalIn switchButton(PD_2, PullDown);

// Define the LEDs
DigitalOut led1(PB_0);
DigitalOut led2(PC_1);
DigitalOut led3(PC_0);

// Define the cycle selection button
DigitalIn cycleButton(PC_10);

// Define the analog input pin for the potentiometer
AnalogIn potentiometer(PA_5);

// Initialize the temperature sensor pin
AnalogIn TMP(PC_3); 

// Initialize the buzzer pin
PwmOut buzzer(PA_15);

// Melody definitions
const int drill_melody[] = {
    NOTE_C2, NOTE_E2, NOTE_G2, NOTE_A2,
    NOTE_C3, NOTE_E3, NOTE_G3, NOTE_A3
};

// Note durations corresponding to the melody
const int drill_note_durations[] = {
    SIXTEENTH_NOTE_DURATION, SIXTEENTH_NOTE_DURATION, SIXTEENTH_NOTE_DURATION, SIXTEENTH_NOTE_DURATION,
    SIXTEENTH_NOTE_DURATION, SIXTEENTH_NOTE_DURATION, SIXTEENTH_NOTE_DURATION, SIXTEENTH_NOTE_DURATION
};

// Function prototypes
void displayError();  // Function to display "Error" on the 7-segment display
bool load();  // Function to check and load the washing machine based on user input and sensor readings
bool temp(int comparisonTemp);  // Function to monitor temperature and detect anomalies
void displayDigit(int digit);  // Function to display a digit on the 7-segment display
bool blinkLED(DigitalOut& led, float duration, char stage, int tmp);  // Function to blink LEDs during washing machine cycles
void cycles();  // Function to execute washing machine cycles based on user input
void play_note_with_LEDs(int frequency, int duration);  // Function to play a note with corresponding LED indication
void change_LED_color(float red, float green, float blue, int steps);  // Function to change LED color gradually
void playMelody();  // Function to play a melody with changing LED colors

// Define a flag to keep track of machine state
bool machineOn = false;

int main() {
    while (true) {
        // Check if the switch button is pressed
        if (switchButton) {
            // If machine is not already on, print "Machine is on" and set the flag
            if (!machineOn) {
                printf("Machine is on\n");
                machineOn = true;
            }

            // Set RGB to green
            red_LED = 0.0f;
            green_LED = 1.0f;
            blue_LED = 0.0f;

            // Start loading function
            if (load()) {
                // If loading successful, start cycles
                cycles();

                // If the cycle completed successfully, start melody
                if (cycleComplete) {
                    playMelody();
                }
            }
            // Turn off all components after completion
            //red_LED = 0.0f;
            //green_LED = 1.0f;
            //blue_LED = 0.0f;
            lock_led = 0;
            led1 = 0;
            led2 = 0;
            led3 = 0;
            segments = 0x00;
            //ThisThread::sleep_for(1s);
        } else {
            // If machine is on, print "Machine off" and reset the flag
            if (machineOn) {
                printf("Machine off\n");
                machineOn = false;
            }

            // Reset cycle completion flag when switch button is off
            cycleComplete = false;

            // Turn off all components
            red_LED = 0.0f;
            green_LED = 0.0f;
            blue_LED = 0.0f;
            lock_led = 0;
            led1 = 0;
            led2 = 0;
            led3 = 0;
            segments = 0x00;

            // Wait for a short duration before checking the switch button again
            ThisThread::sleep_for(100ms);
        }
    }
}


// Function to display "Error" on the 7-segment display
void displayError() {
    // Define segments for "overload"
    segments = 0x3F; // o
    ThisThread::sleep_for(500ms);
    segments = 0x3E; // v
    ThisThread::sleep_for(500ms);
    segments = 0x79; // e
    ThisThread::sleep_for(500ms);
    segments = 0x50; // r
    ThisThread::sleep_for(500ms);
    segments = 0x38; // l
    ThisThread::sleep_for(500ms);
    segments = 0x3F; // o
    ThisThread::sleep_for(500ms);
    segments = 0x77; // a
    ThisThread::sleep_for(500ms);
    segments = 0x5E; // d
    ThisThread::sleep_for(500ms);
    segments = 0x00; // Clear the display
}

bool load() {

    thread_sleep_for(1000);
    int highestForce = 0; // Variable to store the highest force value found
    
    while (!button_door) {
        if (!switchButton) {
            printf("Switch turned off. Exiting load.\n");
            return false;
        }
        
        // Continuously check the force sensor value every 100 ms
        int forceValue = static_cast<int>(FSR.read() * 120);
        //printf("FSR Value: %d\n", forceValue);
        
        // Update the highestForce if the current forceValue is higher
        highestForce = (forceValue > highestForce) ? forceValue : highestForce;
        
        ThisThread::sleep_for(100ms);
    }

    printf("Load weight is: %d\n", highestForce);

    if (highestForce > 108) {
        printf("Overload, reduce weight.\n");
        displayError();
        segments = 0x00;
        return false;
    } else {
        printf("Loading successful.\n");
        lock_led = 1;
        segments = 0x00;
        return true;
    }
}


// Function to monitor temperature and detect anomalies
bool temp(int comparisonTemp) {
    while (true) {
        // Read temperature sensor and convert to integer
        int tempInt = static_cast<int>(TMP.read() * 330);
        // Print temperature value
        printf("TMP Value: %dC\n", tempInt);
        // Check for significant change in temperature
        if (tempInt > comparisonTemp * (1 + ERROR_THRESHOLD) || tempInt < comparisonTemp * (1 - ERROR_THRESHOLD)) {
            printf("Temperature out of range, draining and restarting...\n");
            thread_sleep_for(5000); // Wait for 5 seconds
            return true; // Error detected
        }

        // No error
        else {
            return false;
            }
        // Delay
        thread_sleep_for(1000);
    }
}

// Function to display a digit on the 7-segment display
void displayDigit(int digit) {
    // Define the segment patterns for each digit in hexadecimal
    const int patterns[6] = {
        0x06, // 1
        0x5B, // 2
        0x4F, // 3
        0x66, // 4
        0x6D, // 5
        // You can add patterns for digits 6, 7, 8, 9 if needed
    };

    // Display the specific segments for the digit
    segments = patterns[digit - 1]; // Adjusting index to match array
}

// Function to blink an LED for a certain amount of time and then turn it on
bool blinkLED(DigitalOut& led, float duration, char stage, int tmp) {
    Timer timer;
    timer.start(); // Start the timer

    while(1) {
        // if switch off, exit function
        if (!switchButton) {
            //printf("Cycle interrupted. Machine off.\n");
            return false; // Indicate cycle interrupted
        }
        ThisThread::sleep_for(1000ms);
        if (timer.read() < duration) { // Toggle LED for specified duration
            // Wash cycle
            if (stage == 'w') {
                printf("washing...");
                displayDigit(1);
                // Check temperature with comparison value 
                if (temp(tmp)) {
                    timer.reset(); // Reset the timer
                    continue; // Restart LED blinking and temperature check
                }
            }
            // Spin cycle
            else if (stage == 's') {
                printf("spinning...\n");
                displayDigit(3);
            }
            // Rinse cycle
            else if (stage == 'r') {
                printf("rinsing...\n");
                displayDigit(2);
            }
            led = !led; // Toggle LED state
            ThisThread::sleep_for(500ms); // Toggle every 500 milliseconds
        } 
        // After specified duration, keep LED on
        else {
            if (stage == 'w') {
                printf("washing completed.\n");
                displayDigit(1);
            }
            else if (stage == 's') {
                printf("spinning completed.\n");
                displayDigit(3);
            }
            else if (stage == 'r') {
                printf("rinsing completed.\n");
                displayDigit(2);
            }
            led = 1; // After specified duration, keep LED on
            ThisThread::sleep_for(1000ms);
            return true; // Indicate cycle completed successfully
        }
    }
}

// Function to execute washing machine cycles based on user input
void cycles() {
    // Variable to store the selected cycle
    int selectedCycle = 0;
    bool mainSwitchOff = false; // Track if the main switch was turned off

    while (!cycleComplete) {
        // if switch off exit function
        if (!switchButton) { 
            printf("Cycle interrupted. Machine off.\n");
            mainSwitchOff = true; // Set the flag
            return;
        }
        // Read voltage from potentiometer and convert it to a range of 0-3.3
        float voltage = potentiometer.read() * 3.3f;
        // Divide the voltage range into 5 equal segments
        int option = static_cast<int>((voltage / 3.3f) * 5) + 1; // +1 to match options starting from 1

        // If a new cycle is selected, print the selected cycle
        //if (selectedCycle != option) {
            //selectedCycle = option;
            //printf("Cycle selected: %d.\n", selectedCycle);
        //}
        // Display digits based on the selected option
        displayDigit(option);

        // Check if the button is pressed
        if (cycleButton == 1) {
            bool led1_result, led2_result, led3_result;
            switch(option) {
                case 1:
                    // Custom cycle option 1
                    printf("Running cycle 1.\n");
                    led1_result = blinkLED(led1, 4, 'w', 30);
                    led2_result = blinkLED(led2, 3, 'r', 30);
                    led3_result = blinkLED(led3, 7, 's', 30);
                    break;
                case 2:
                    // Custom cycle option 2
                    printf("Running cycle 2.\n");
                    led1_result = blinkLED(led1, 13, 'w', 30);
                    led2_result = blinkLED(led2, 10, 'r', 30);
                    led3_result = blinkLED(led3, 5, 's', 30);
                    break;
                case 3:
                    // Custom cycle option 3
                    printf("Running cycle 3.\n");
                    led1_result = blinkLED(led1, 11.5, 'w', 30);
                    led2_result = blinkLED(led2, 8, 'r', 30);
                    led3_result = blinkLED(led3, 4, 's', 30);
                    break;
                case 4:
                    // Custom cycle option 4
                    printf("Running cycle 4.\n");
                    led1_result = blinkLED(led1, 12, 'w', 30);
                    led2_result = blinkLED(led2, 8, 'r', 30);
                    led3_result = blinkLED(led3, 7.5, 's', 30);
                    break;
                case 5:
                    // Custom cycle option 5
                    printf("Running cycle 5.\n");
                    led1_result = blinkLED(led1, 12.5, 'w', 30);
                    led2_result = blinkLED(led2, 13, 'r', 30);
                    led3_result = blinkLED(led3, 10, 's', 30);
                    break;
                default:
                    break;
            }
            cycleComplete = led1_result && led2_result && led3_result; // Mark the cycle as complete if all stages were successful
        }
    }

    // Check if the cycle was interrupted due to main switch off before printing completion message
    if (!mainSwitchOff && cycleComplete) {
        printf("Cycle completed, ready for next cycle.\n");
    }
}




// Function to play a melody with changing LED colors
void playMelody(){
    while(switchButton && !button_door) {
        // Play the melody with changing LED colors
        for(int i = 0; i < sizeof(drill_melody) / sizeof(drill_melody[0]); i++){
            // Check if door button is pressed
            if (button_door) {
                cycleComplete = false;  // Reset cycle completion flag
                return;  // Exit function if door button is pressed
            }
            play_note_with_LEDs(drill_melody[i], drill_note_durations[i]);
        }

        // Turn off buzzer and LEDs
        buzzer.write(0.0f);
        red_LED.write(0.0);
        green_LED.write(0.0);
        blue_LED.write(0.0);

        // Delay between melodies
        //ThisThread::sleep_for(1s);
    }
}

// Function to play a note with corresponding LED indication
void play_note_with_LEDs(int frequency, int duration){
    buzzer.period(1.0f / frequency); // Set the period of the PWM signal
    buzzer.write(0.5f); // Set duty cycle to 50% (half the period)

    // Calculate LED intensities based on frequency
    float intensity = (frequency - NOTE_C2) / (float)(NOTE_A3 - NOTE_C2); // Normalize frequency
    red_LED.write(intensity * 0.9); // Set red LED intensity (0.0 to 0.9)
    green_LED.write(0.7 - intensity * 0.5); // Set green LED intensity (0.2 to 0.7)
    blue_LED.write(intensity * 0.8); // Set blue LED intensity (0.0 to 0.8)

    // Change LED color gradually
    change_LED_color(1.0 - intensity, intensity, 0.0, 10);

    ThisThread::sleep_for(duration); // Play sound for specified duration

    // Turn off buzzer and LEDs
    buzzer.write(0.0f);
    red_LED.write(0.0);
    green_LED.write(0.0);
    blue_LED.write(0.0);
}

// Function to change LED color gradually
void change_LED_color(float red, float green, float blue, int steps){
    float red_step = red / steps;
    float green_step = green / steps;
    float blue_step = blue / steps;

    for(int i = 0; i < steps; i++){
        red_LED.write(red - i * red_step);
        green_LED.write(green - i * green_step);
        blue_LED.write(blue - i * blue_step);
        ThisThread::sleep_for(50ms); // Adjust transition speed here
    }
}
