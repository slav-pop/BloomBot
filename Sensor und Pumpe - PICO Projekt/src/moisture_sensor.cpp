#include "moisture_sensor.hpp"
#include <vector>

static uint8_t moisture_pin;

// Kalibrierungswerte für den HD38 am Pico (ggf. anpassen):
// Trocken an der Luft ~ 3800-4000 (12-Bit) bzw. ~ 950-1000 (10-Bit)
// In Wasser ~ 1200-1500 (12-Bit) bzw. ~ 300-400 (10-Bit)
// static const int DRY_VAL = 3500;   // 1023
// static const int WET_VAL = 1500;  // 372 - 486 ( 529 ) -> Alles komplett im Wasser

// Test Werte mit Wasser
static const int DRY_VAL = 1023;   // 1023
static const int WET_VAL = 372;  // 372 - 486 ( 529 ) -> Alles komplett im Wasser

void moisture_sensor_init(uint8_t pin) {
    moisture_pin = pin;
    pinMode(moisture_pin, INPUT);
}

int moisture_sensor_read_raw() {
    return analogRead(moisture_pin);
}

int moisture_sensor_read_raw_median(uint8_t samples = 5, uint16_t delay_ms = 10) {
    if (samples == 0) return 0;  // Edge case: No samples

    // Allocate array for readings
    std::vector<int> readings(samples);
    // int readings[samples];

    // Take samples
    for (int i = 0; i < samples; i++) {
        readings[i] = moisture_sensor_read_raw();
        if (i < samples - 1) delay(delay_ms);  // No delay after last reading
    }

    // Sort the array
    bubble_sort(readings, samples)

    // Return median
    if (samples % 2 == 1) {
        // Odd number of samples: Return middle value
        return readings[samples / 2];
    } else {
        // Even number of samples: Return average of two middle values
        return (readings[samples / 2 - 1] + readings[samples / 2]) / 2;
    }
}


float moisture_sensor_read_percentage() {
    // int raw = moisture_sensor_read_raw();
    int raw = moisture_sensor_read_raw_median();
    
    // Map-Funktion: Trocken = 0%, Nass = 100%
    float percentage = map(raw, DRY_VAL, WET_VAL, 0, 100);

    // Begrenzen auf 0 - 100%
    return constrain(percentage, 0.0f, 100.0f);
}

void bubble_sort(int arr[], int len) {
    int cache;

    for (int i = 0; i < len; i++) {
        bool all_left_elements_smaller = true;

        for (int s = 0; s < len - i - 1; s++) {
            bool left_bigger = arr[s] > arr[s+1];
            if (left_bigger) {
                cache = arr[s+1];
                arr[s+1] = arr[s];
                arr[s] = cache;

                all_left_elements_smaller = false;
            }
        }
        
        if (all_left_elements_smaller) {
            // No swapping means, array already in right order
            break;
        }
    }
}