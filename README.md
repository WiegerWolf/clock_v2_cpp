# Deprecated in favour of https://github.com/WiegerWolf/clock_v3_cpp

![image](https://github.com/user-attachments/assets/e7d036ee-261e-4ff3-a300-953261e19f50)

## Deps

```bash
sudo apt update
sudo apt install -y git ssh
git clone git@github.com:WiegerWolf/clock_v2_cpp.git
```

```bash
sudo apt install -y cmake gcc make g++ libsdl2-dev \
    libsdl2-image-dev libsdl2-ttf-dev libcurl4-openssl-dev \
    libssl-dev
```

## Build Steps

Add .env file to the root of the project with the following content:

```bash
CEREBRAS_API_KEY='your_api_key'
```

Then run the following command:

```bash
./build.sh
```

## Deployment

Make sure you have the following structure:

```bash
/home/n
├── digital_clock
├── assets/fonts/*.ttf
```

You get the `./digital_clock` file from the build step.
You get the `assets` folder (with fonts included) from the `git clone` step.

## Autostart

```bash
sudo nano /etc/systemd/system/digital-clock.service
```

`digital-clock.service`:

```
[Unit]
Description=Digital Clock Service
After=network.target

[Service]
Type=simple
User=n
WorkingDirectory=/home/n
ExecStart=/home/n/digital_clock
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable digital-clock.service
sudo systemctl start digital-clock.service
```

## Viewing Logs

The application logs to stdout/stderr, which are automatically captured by systemd. Use `journalctl` to view logs:

```bash
# Follow live logs (like tail -f)
sudo journalctl -u digital-clock.service -f

# View last 100 lines
sudo journalctl -u digital-clock.service -n 100

# View logs with priority/colors
sudo journalctl -u digital-clock.service -f --output=short-precise

# Filter by log level (errors and above)
sudo journalctl -u digital-clock.service -p err

# View logs from specific time
sudo journalctl -u digital-clock.service --since "1 hour ago"
sudo journalctl -u digital-clock.service --since "2025-01-15 12:00:00"

# Search for specific text
sudo journalctl -u digital-clock.service | grep "Background"

# View logs from today
sudo journalctl -u digital-clock.service --since today

# Export logs to file
sudo journalctl -u digital-clock.service > clock_logs.txt
```

**Log Levels:**
- `DEBUG` - Detailed debugging information
- `INFO` - General informational messages
- `WARNING` - Warning messages
- `ERROR` - Error messages (output to stderr)
- `CRITICAL` - Critical errors (output to stderr)
