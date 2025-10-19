# Deploying `TokenProvisioningService` to a Raspberry Pi Zero 2W

This document describes the recommended steps to publish and deploy the `TokenProvisioningService` project (targeting .NET 10) to a Raspberry Pi Zero 2W.

## High-level plan
- Cross-publish for the Pi CPU architecture (prefer `linux-arm64` and a 64-bit OS on the Pi).
- Copy published output to the Pi.
- Either install the .NET 10 runtime on the Pi and run framework-dependent, or publish self-contained and run the executable.
- Run the app as a `systemd` service so it starts on boot and restarts on failure.

## Steps

1) Choose OS / architecture
- Use a 64-bit Raspberry Pi OS (or Ubuntu Server 22.04/24.04 for `arm64`). The Pi Zero 2W CPU supports 64-bit and .NET 10 ARM64 has the best support.
- If you must run a 32-bit OS, use the `linux-arm` runtime identifier and verify runtime availability.

2) Publish from your development machine
- Framework-dependent (smaller, requires runtime on Pi):

```bash
dotnet publish ./TokenProvisioningService/TokenProvisioningService.csproj -c Release -r linux-arm64 --self-contained false -o ./publish/linux-arm64
```

- Self-contained (no runtime install on Pi, larger):

```bash
dotnet publish ./TokenProvisioningService/TokenProvisioningService.csproj -c Release -r linux-arm64 --self-contained true -p:PublishTrimmed=false -o ./publish/linux-arm64
```

- Optionally enable trimming with care: `-p:PublishTrimmed=true` (may break reflection-based libs).

3) Install runtime on the Pi (only for framework-dependent)
- On the Pi (arm64), add Microsoft packages and install .NET 10 runtime:

```bash
wget https://packages.microsoft.com/config/debian/12/packages-microsoft-prod.deb -O packages-microsoft-prod.deb
sudo dpkg -i packages-microsoft-prod.deb
sudo apt update
sudo apt install -y dotnet-runtime-10
```

- Verify:

```bash
dotnet --list-runtimes
```

4) Copy published files to the Pi
- From dev machine to the Pi:

```bash
scp -r ./publish/linux-arm64/* pi@raspberrypi:/home/pi/token-provisioning/
```

- On the Pi ensure correct permissions:

```bash
cd /home/pi/token-provisioning
# if self-contained make the executable runnable
chmod +x ./TokenProvisioningService
```

- For framework-dependent ensure `TokenProvisioningService.dll` is present and the `dotnet` runtime is installed.

5) Create a `systemd` service
- Example file `/etc/systemd/system/token-provisioning.service` (adjust paths and user):

```ini
[Unit]
Description=Token Provisioning Service
After=network.target

[Service]
WorkingDirectory=/home/pi/token-provisioning
User=pi
Environment=ASPNETCORE_ENVIRONMENT=Production
Environment=ASPNETCORE_URLS=http://0.0.0.0:12345
# framework-dependent:
#ExecStart=/usr/bin/dotnet /home/pi/token-provisioning/TokenProvisioningService.dll
# OR self-contained:
ExecStart=/home/pi/token-provisioning/TokenProvisioningService
Restart=always
RestartSec=5
KillSignal=SIGINT
TimeoutStopSec=30

[Install]
WantedBy=multi-user.target
```

- Enable and start the service:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now token-provisioning.service
```

- Check logs:

```bash
journalctl -u token-provisioning.service -f
```

6) Network / mDNS notes
- The service advertises mDNS; ensure the Pi’s network allows multicast and `udp/5353` is not blocked.
- If mDNS discovery fails, check other local mDNS services (such as Avahi) and firewall rules (`ufw`/`iptables`).

7) Troubleshooting & tips
- Building on the Pi is slow — cross-publish on a PC/laptop.
- If the app fails due to missing native dependencies, review exception logs. Most managed libraries run fine on ARM64.
- For disk/size constrained setups prefer framework-dependent + runtime on Pi. For fully isolated deployments or differing runtime versions prefer self-contained.
- Use `systemctl status token-provisioning.service` and `journalctl` to get error details.

---

If you want, generate a small deployment script that publishes, copies to the Pi and enables the `systemd` service. Adjust paths and usernames to match your environment.