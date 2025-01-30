import serial
import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import butter, filtfilt
from matplotlib.animation import FuncAnimation

# Serial Port Configuration
SERIAL_PORT = 'COM4'  # Change to your serial port (e.g., '/dev/ttyUSB0' for Linux)
BAUD_RATE = 115200
TIMEOUT = 1  # Timeout in seconds for serial read

# Sampling rate and time vector
sampling_rate = 200  # 200 Hz
duration = 1  # 1 second per update
t = np.arange(0, duration, 1/sampling_rate)

# Function to design a Butterworth filter (both low-pass and high-pass)
def butter_filter(cutoff, fs, filter_type='low', order=4):
    nyquist = 0.5 * fs
    normal_cutoff = cutoff / nyquist
    b, a = butter(order, normal_cutoff, btype=filter_type, analog=False)
    return b, a

# Function to read the data from serial port
def read_serial_data():
    with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=TIMEOUT) as ser:
        line = ser.readline().decode('utf-8').strip()
        if line:
            return np.array([float(x) for x in line.split(',')])
        return None

# Apply high-pass filter
def apply_highpass_filter(data, cutoff=10):
    b_hp, a_hp = butter_filter(cutoff, sampling_rate, filter_type='high')
    return filtfilt(b_hp, a_hp, data)

# Apply low-pass filter
def apply_lowpass_filter(data, cutoff=50):
    b_lp, a_lp = butter_filter(cutoff, sampling_rate, filter_type='low')
    return filtfilt(b_lp, a_lp, data)

# Create the plot for real-time display
fig, axs = plt.subplots(8, 1, figsize=(12, 10), sharex=True)
lines = [ax.plot(t, np.zeros_like(t), label=f'Channel {i+1}')[0] for i, ax in enumerate(axs)]
for i, ax in enumerate(axs):
    ax.set_title(f'Channel {i+1}')
    ax.set_ylim(-10, 10)
    ax.grid(True)

# Real-time update function
def update(frame):
    # Read data from the serial port
    data = read_serial_data()
    
    if data is not None:
        # Apply filters to the data
        highpass_filtered = apply_highpass_filter(data, cutoff=10)
        lowpass_filtered = apply_lowpass_filter(highpass_filtered, cutoff=50)
        
        # Update the plot with filtered data
        for i, line in enumerate(lines):
            line.set_ydata(lowpass_filtered[i] * np.ones_like(t))  # Update each channel with its filtered value
    
    # Save the current figure as an image
    plt.savefig(f'realtime_plot_{frame}.png')

    return lines

# Set up the animation to update the plot
ani = FuncAnimation(fig, update, interval=1000/sampling_rate)

plt.tight_layout()

# Instead of plt.show(), we save the image after every update
# plt.show() will be commented out, as it causes issues in non-interactive environments
# plt.show()

