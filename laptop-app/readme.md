1. **QNX Pi terminal — Ethernet static IP:**

```bash
ifconfig cgem0 down
ifconfig cgem0 192.168.1.200 netmask 255.255.255.0 up
route add default 192.168.1.1
```

**QNX Pi terminal — check it worked:**

```bash
ifconfig cgem0
ping 192.168.1.3
```

------

**QNX Pi terminal — WiFi connect:**

```bash
cat > /etc/wpa_supplicant.conf << 'EOF'
network={
    ssid="Zee"  
    key_mgmt=WPA-PSK
    psk="12345678"
}
EOF
wpa_supplicant -i bcm0 -c /etc/wpa_supplicant.conf -B
sleep 3
ifconfig bcm0 192.168.1.201 netmask 255.255.255.0 up
route add default 192.168.1.1
EOF
wpa_supplicant -i bcm0 -c /etc/wpa_supplicant.conf -B
```

**QNX Pi terminal — WiFi check:**

```bash
ifconfig bcm0
ping 192.168.1.3
```









# python

### First test with a small dummy file (don't use the full rootfs yet)

On your **laptop**:

```bash
mkdir -p /home/zee/ITI_Files/linux/ota_update/laptop-app/python
cd /home/zee/ITI_Files/linux/ota_update/laptop-app/python

# Create a 10MB test file (not the real rootfs)
dd if=/dev/urandom of=test_image.img bs=1M count=10
sha256sum test_image.img   # note this hash

# Send to QNX (your QNX WiFi IP is 192.168.97.91)
python3 send_update.py test_image.img 192.168.1.100 
```

On **QNX**

```

```

 you should see:

```
[RX] Laptop connected.
[RX] Header: version=1.0.0-test size=10485760 sha256=<hash>
[RX] Received 10485760 / 10485760 bytes (100%)
[RX] Image fully received.
[VERIFY] Computing SHA256 of staged image...
[VERIFY] Expected: <hash>
[VERIFY] Computed: <hash>
[VERIFY] SHA256 OK
[OTA-GW] Image verified OK — version=1.0.0-test
[OTA-GW] TODO Phase 2: forward to Yocto via SOME/IP
[OTA-GW] Waiting for laptop...
```

### Confirm the file landed correctly on QNX

```bash
# On QNX after the transfer:
ls -lh /var/ota_staging/update.img
sha256sum /var/ota_staging/update.img
# Should match what the laptop printed
```