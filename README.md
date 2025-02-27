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

```bash
mkdir build
cd build
cmake .. 
make
```

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
