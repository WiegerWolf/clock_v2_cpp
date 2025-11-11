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
