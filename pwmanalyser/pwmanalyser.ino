/**
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017, Björn Lind
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 * This file is part of the https://github.com/wingstar74/pwmanalyser project.
 */

//Number of channels to analyse
#define NUM_PWM_CHANNELS 6

//The first pin that will be used for pwm analyse. Next pins will be pwm_analyser_start_pin+1, ... , pwm_analyser_start_pin+NUM_PWM_CHANNELS-1
const int pwm_analyser_start_pin = 22;

//The pin used for the pwm generation for test purposes
const byte pwm_pin = 2;

enum state_t {
  first_rising,
  second_falling,
  third_rising,
  finished
};

enum pin_state_t {
  pin_state_low,
  pin_state_high
};

typedef void (*interrupt_change_fn_t)(void);

struct pwm_channel_t {
  int interrupt_pin;
  state_t next_state;
  int number_of_interrupts;
  unsigned long first_rising_time;
  unsigned long second_falling_time;
  unsigned long third_rising_time;
  pin_state_t pin_state;
  interrupt_change_fn_t interrupt_change_cb;
};

volatile struct pwm_channel_t pwm_channels[NUM_PWM_CHANNELS];

int pwm_val = 0;

void initialize_pwm_channel(volatile pwm_channel_t *pwm_channel, int interrupt_pin, interrupt_change_fn_t change) {
  pwm_channel->interrupt_pin = interrupt_pin;
  pinMode(interrupt_pin, INPUT);
  pwm_channel->interrupt_change_cb = change;
}

void rising(const int chan_index) {
  if (pwm_channels[chan_index].next_state == first_rising) {
    pwm_channels[chan_index].first_rising_time = micros();
    pwm_channels[chan_index].next_state = second_falling;
  }
  else if (pwm_channels[chan_index].next_state == third_rising) {
    pwm_channels[chan_index].third_rising_time = micros();
    detachInterrupt(digitalPinToInterrupt(pwm_channels[chan_index].interrupt_pin));
    pwm_channels[chan_index].next_state = finished;
  }
  pwm_channels[chan_index].pin_state = pin_state_high;
}

void falling(const int chan_index) {
  if (pwm_channels[chan_index].next_state == second_falling) {
    pwm_channels[chan_index].second_falling_time = micros();
    pwm_channels[chan_index].next_state = third_rising;
  }
  pwm_channels[chan_index].pin_state = pin_state_low;
}

static volatile int chan_index;

void change_cb_channel() {
  pwm_channels[chan_index].number_of_interrupts++;
  if (digitalRead(pwm_channels[chan_index].interrupt_pin) == HIGH) {
    rising(chan_index);
  } else {
    falling(chan_index);
  }
}

void setup() {
  Serial.begin(250000);
  pinMode(pwm_pin, OUTPUT);
  analogWrite(pwm_pin, pwm_val);

  for (int i=0; i < NUM_PWM_CHANNELS; i++) {
    initialize_pwm_channel(&pwm_channels[i], pwm_analyser_start_pin+i, change_cb_channel);
  }
}

void start_measurement(volatile pwm_channel_t *pwm_channel) {
  pwm_channel->next_state = first_rising;
  pwm_channel->number_of_interrupts = 0;
  attachInterrupt(digitalPinToInterrupt(pwm_channel->interrupt_pin), pwm_channel->interrupt_change_cb, CHANGE);
}

void stop_measurement(volatile pwm_channel_t *pwm_channel) {
  pwm_channel->pin_state = (digitalRead(pwm_channel->interrupt_pin)==HIGH ? pin_state_high : pin_state_low);
  detachInterrupt(digitalPinToInterrupt(pwm_channel->interrupt_pin));
}

#define MAX_ULONG ((unsigned long)0xffffffff)

void print_measurement(volatile pwm_channel_t *pwm_channel) {
  if (pwm_channel->next_state == finished) {
    //Check and fix rollover
    if (pwm_channel->first_rising_time > pwm_channel->third_rising_time) {
      unsigned long comp = (MAX_ULONG - pwm_channel->first_rising_time) + 1;  
      pwm_channel->first_rising_time += comp;
      pwm_channel->second_falling_time += comp;
      pwm_channel->third_rising_time += comp;
    }
    
    unsigned long t = pwm_channel->third_rising_time - pwm_channel->first_rising_time;
    unsigned long freq_dHz = 1000000ul*10 / t;
    unsigned long duty_cycle_dProc = ((pwm_channel->second_falling_time - pwm_channel->first_rising_time)*100*10)/t;
    
    Serial.print("(");
    Serial.print(duty_cycle_dProc/10);
    Serial.print(".");
    Serial.print(duty_cycle_dProc%10);
    Serial.print(", ");
    Serial.print(freq_dHz/10);
    Serial.print(".");
    Serial.print(freq_dHz%10);
    Serial.print(")");
  } else if (pwm_channel->number_of_interrupts == 0) {
    Serial.print("(");
    if (pwm_channel->pin_state == pin_state_high) {
      Serial.print("100.0, 0.0)");
    } else {
      Serial.print("0.0, 0.0)");
    }
  } else {
    Serial.print("(-1.0, -1.0)");
  }
}

void loop() {
  static long counter = 0;

  for (int i=0; i < NUM_PWM_CHANNELS; i++) {
    chan_index = i;
    start_measurement(&pwm_channels[i]);
    delay(20);
    stop_measurement(&pwm_channels[i]);
    delay(1);
  }  

  Serial.print("[");
  for (int i=0; i < NUM_PWM_CHANNELS; i++) {
    print_measurement(&pwm_channels[i]);
    if (i<NUM_PWM_CHANNELS-1) {
       Serial.print(", ");
    }
  }

  Serial.println("]");

  //The PWM generation for test purpose
  counter++;
  
  if (counter>9) {
    counter=0;
    if (pwm_val >= 255) {
      pwm_val = 0;
    }
    else {
      pwm_val++;
    }
    analogWrite(pwm_pin, pwm_val);
    if (pwm_val == 0) {
      Serial.println("# (0.0, 0.0)");
    } else if (pwm_val == 255) {
      Serial.println("# (100.0, 0.0)");
    }
    else {
      Serial.print("# (");
      Serial.print(pwm_val*1000/255/10);
      Serial.print(".");
      Serial.print((pwm_val*1000/255)%10);
      Serial.println(", 1000.0)");
    }
  }
  delay(1);
}

