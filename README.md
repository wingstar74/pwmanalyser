# pwmanalyser
A simple PWM analyser for Arduino Due

The PWM analyser will analyse 6 channels (pin 22,23,24,25,26,27) for there duty cycle and frequency and print the result. All 6 channels is printed on a single line:<br />
[(20.0, 1000.0), (30.0, 800.0), (40.0, 500.0), (50.0, 200.0), (60.0, 850.0), (70.0, 350.0)]

It should work for frequencies between 100Hz to 1000Hz. There is a test pin 2 that will out put a PWM signal with 1000Hz with varying duty cykle that is printed on a new line:<br />
\# (12.0, 1000.0)
