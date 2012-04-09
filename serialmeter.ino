/*
 * serialmeter -- drives PWM on a pin to levels given over serial input
 *
 * Accepts commands on serial input, and drives a PWM-capable pin to the
 * requested levels; useful for bridging between a host machine and an analog
 * device, e.g. a needle gauge.
 *
 * Valid serial commands:
 *   level N - Set level to exactly N, which must be within [0, upper_limit]
 *   levelf F - Set level to F*upper_limit, where f must be within [0, 1.0].
 *              This abstracts away the range of the device, but somewhat
 *              diminishes resolution, as the AVR's PWM range is 255 values,
 *              whereas the floating point precision only accomodates 100.
 *   limit N - Set the upper limit PWM value to N, keeping subsequent 'levelf'
 *             calls adjusted to that range; useful when the usable range of
 *             the connected analog device is less than the PWM output range,
 *             such as a needle gauge that bounces when driven to its maximum
 *             value.
 *   pin N   - Sends subsequent values to (PWM-capable) pin N
 *   increment N - Smooth out transitions by shifting gradually from the old
 *                 value to the new, in increments of N steps across the
 *                 [0,upper_limit] output range.  Larger values make faster
 *                 transitions; set to 255 to always jump to new values.
 *   delay N - delay N msec between increments
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

int meter_pin = 9;
int adjust_increment = 5;
int adjust_delay = 30;
int upper_limit = 250;
char input[20];
char* input_p = input;
int current_level = 0;
boolean startup = true; 

void setup() {
  Serial.begin(9600); 
  *input_p = '\0';
  analogWrite(meter_pin, current_level);
}

void WaitForFirstInput() {
  while (!Serial.available()) {
    FadeTo(0);
    if (Serial.available()) {
      return;
    }
    FadeTo(upper_limit);
  }
}

void FadeTo(int level) {
  if (current_level < level) {
    for (; current_level < level;
           current_level += min(level - current_level, adjust_increment)) {
      if (Serial.available()) {
        return;
      }
      analogWrite(meter_pin, current_level);
      delay(adjust_delay);
    }
  }

  if (current_level > level) {
    for (; current_level > level;
           current_level -= min(current_level - level, adjust_increment)) {
      if (Serial.available()) {
        return;
      }
      analogWrite(meter_pin, current_level);
      delay(adjust_delay);
    }
  }
}

boolean ReadSerialByte() {
  char ch = Serial.read();
  if (ch != '\n') {
    if (input_p < input + sizeof(input) - 1) {
      *(input_p++) = ch;
    }
    return false;
  }
  *input_p = '\0';
  return true;
}

void SetLevel(float newvalue) {
  Serial.print("new value: ");
  Serial.println(newvalue);
  input_p = input;
  newvalue = max(min(newvalue, 1), 0);
  FadeTo(upper_limit * newvalue);
}

void SetUpperLimit(int new_limit) {
  if (new_limit < 1 || new_limit > 255) {
    Serial.print("limit out of hardware range");
  } else {
    float fraction = (float)current_level / upper_limit;
    upper_limit = new_limit;
    current_level = (float)upper_limit * fraction;
    analogWrite(meter_pin, current_level);
  }
}

void loop() {
  if (startup) {
    startup = false;
    WaitForFirstInput();
  }
  if (Serial.available()) {
    if (!ReadSerialByte()) {
      return;
    }
    Serial.print("buf: ");
    Serial.println(input);
    if (!strncmp(input, "level ", 6)) {
      FadeTo(max(min(atoi(input + 6), upper_limit), 0));
    } else if (!strncmp(input, "levelf ", 6)) {
      FadeTo(upper_limit * max(min(atof(input + 6), 1), 0));
    } else if (!strncmp(input, "pin ", 4)) {
      meter_pin = atoi(input + 4);
    } else if (!strncmp(input, "increment ", 4)) {
      adjust_increment = atoi(input + 10);
    } else if (!strncmp(input, "delay ", 6)) {
      adjust_delay = atoi(input + 6);
    } else if (!strncmp(input, "limit ", 6)) {
      SetUpperLimit(atoi(input + 6));
    } else if (!strcmp(input, "show")) {
      Serial.print("current level: "); Serial.println(current_level);
      Serial.print("meter pin: "); Serial.println(meter_pin);
      Serial.print("upper limit: "); Serial.println(upper_limit);
      Serial.print("adjust increment: "); Serial.println(adjust_increment);
      Serial.print("adjust delay (ms): "); Serial.println(adjust_delay);
    } else {
      Serial.println("** unrecognized command");
    }
    input_p = input;
  }
}

