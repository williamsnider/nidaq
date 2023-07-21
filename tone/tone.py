import nidaqmx
import numpy as np
import matplotlib.pyplot as plt

# Inputs
FREQUENCY = 500  # FREQUENCY of the tone in Hz
DURATION = 0.25  # DURATION of the tone in seconds
SAMPLE_RATE = 44100  # Sample rate in Hz (you can adjust this if needed)

# Generate tone time array
num_timesteps = int(SAMPLE_RATE * DURATION)
t = np.linspace(0, DURATION, num_timesteps, endpoint=False)
tone = np.sin(2 * np.pi * FREQUENCY * t)  # Sine wave with FREQUENCY Hz
tone_scaled = tone * 2.5  # Scale to -2.5 to +2.5V


with nidaqmx.Task() as task:
    task.ao_channels.add_ao_voltage_chan("Dev2/ao0")
    task.timing.cfg_samp_clk_timing(
        SAMPLE_RATE,
        sample_mode=nidaqmx.constants.AcquisitionType.FINITE,
        samps_per_chan=num_timesteps,
    )  # Hardware-timed

    # Play tone
    task.write(tone_scaled, auto_start=True)

    # Stop the task
    task.wait_until_done(timeout=DURATION + 1)
    task.stop()

# Plot the tone
ax = plt.subplot(111)
ax.plot(t, tone)
ax.set_xlabel("Time (s)")
ax.set_ylabel("Voltage (V)")
plt.show()
