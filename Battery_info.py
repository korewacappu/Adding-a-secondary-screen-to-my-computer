# Reads battery status on Endeavour OS and sends it to the Pico over serial
# Install dependency: sudo pacman -S python-pyserial
# Keep in mind that this only works on Linux system. If your also using Linux, make sure that your PORT = "" is set correctly

import serial
import time
import os
import glob

# --- Config ---
PORT      = '/dev/ttyACM0'
BAUD      = 115200
INTERVAL  = 1  # seconds between updates, I find one to be fast enough

def get_battery():
    """Read battery info from /sys/class/power_supply — works on all Linux."""
    # Find battery directory (usually BAT0 or BAT1)
    batteries = glob.glob('/sys/class/power_supply/BAT*')
    if not batteries:
        return None, None

    bat_path = batteries[0]

    # Read percentage
    try:
        with open(os.path.join(bat_path, 'capacity'), 'r') as f:
            percent = int(f.read().strip())
    except:
        percent = 0

    # Read charging status
    try:
        with open(os.path.join(bat_path, 'status'), 'r') as f:
            status = f.read().strip().lower()
            # Possible values: Charging, Discharging, Full, Unknown
            if status == 'full':
                charging = True
                percent  = 100
            elif status == 'charging':
                charging = True
            else:
                charging = False
    except:
        charging = False

    return percent, charging

def main():
    print(f"Connecting to Pico on {PORT}...")

    while True:
        try:
            with serial.Serial(PORT, BAUD, timeout=2) as ser:
                print("Connected. Sending battery data...")
                time.sleep(2)  # give Pico time to boot

                while True:
                    percent, charging = get_battery()

                    if percent is not None:
                        status_str = "charging" if charging else "discharging"
                        msg = f"BAT:{percent}:{status_str}\n"
                        ser.write(msg.encode())
                        print(f"Sent: {msg.strip()}")
                    else:
                        print("No battery found — are you on AC only?")

                    time.sleep(INTERVAL)

        except serial.SerialException as e:
            print(f"Serial error: {e} — retrying in 5s...")
            time.sleep(5)
        except KeyboardInterrupt:
            print("Stopped.")
            break

if __name__ == '__main__':
    main()
