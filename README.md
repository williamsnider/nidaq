# nidaq
Controlling a NI-DAQ board with c++

### Description
This repository demonstrates how to interact with a NI-DAQ board using c++.

In `camera_pulse.cpp`, I use a counter output to send a train of pulses at a specified frequency and duty cycle. In my experimental setup, I use this as a hardware trigger to a network of FLIR Blackfly S cameras.

In `send_timestamp_as_bitcode`, I use a hardware-timed digital output channel to send a bitcode (conveying a timestamp). In my experimental setup, I use this to synchronize data obtained on one computer (controlling a robotic arm) to an Intan board.

### Compilation 
Ensure NIDAQmx has been installed.

For Ubuntu 20.04, compile using:
```
g++ send_timestamp_as_bitcode.cpp /usr/lib/x86_64-linux-gnu/libnidaqmx.so -pthread -o send_timestamp_as_bitcode

g++ camera_pulse.cpp /usr/lib/x86_64-linux-gnu/libnidaqmx.so -o camera_pulse
```

### Usage
Run by executing:
```
./camera_pulse

./send_timestamp_as_bitcode
```
