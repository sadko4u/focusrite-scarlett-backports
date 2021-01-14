# Focusrite Scarlett Gen3 backports

This is backport of [Geoffrey D. Bennet's](https://github.com/geoffreybennett)
[Linux kernel driver](https://github.com/geoffreybennett/scarlett-gen2)
that adds support of Focusrite Scarlett Gen3 devices formed as a single patch to the
Linux Kernel 5.3.18 currently used in [openSUSE Leap 15.2](https://www.opensuse.org/).

The supported list of Gen3 devices:

* Focusrite Scarlett Solo
* Focusrite Scarlett 2i2
* Focusrite Scarlett 4i4
* Focusrite Scarlett 8i6
* Focusrite Scarlett 18i8
* Focusrite Scarlett 18i20

Provided patches:

* ```opensuse-leap-linux-5.3.18-scarlett-gen3.patch``` - patch for the standard desktop openSUSE Leap 5.3.18 kernel.
* ```opensuse-leap-linux-5.3.18-scarlett-gen3-rt.patch``` - patch for the realtime (RT) openSUSE Leap 5.3.18 kernel.
* ```vanilla-linux-5.9.0-scarlettt-gen3.patch``` - patch for the original (vanilla) Linux kernel 5.9.0

## Installation

The kernel should be built from sources, so here you are all on your own.

The following manual is written for openSUSE Leap 15.2 users and tries to provide the
information for average user which is not familiar with building kernel from source.

Below we will assume that we apply patch to the openSUSE RT kernel which is more suitable for the audio production.

### Preparing the system

First of all, the following packages should be installed:

* ncurses-devel
* pkg-config
* make
* gcc
* kernel-source for non-realtime kernel
* kernel-source-rt for realtime kernel

This may be obtained by the following command by the ```root``` user:
```
zypper in gcc make pkg-config ncurses-devel kernel-source kernel-source-rt
```

### Patching the kernel

To apply patch, we can do the following steps:

Copy patch to ```/usr/src``` directory:
```bash
cp opensuse-leap-linux-5.3.18-scarlett-gen3-rt.patch /usr/src
```

It is highly recommended to make a copy of the kernel to not to clash with the factory kernel:
```bash
cd /usr/src
cp -r linux-`uname -r` opensuse-leap-linux-5.3.18-custom-rt
```

Further we assume that kernel source is located in the ```opensuse-leap-linux-5.3.18-custom-rt``` directory.

Then we can apply the patch to the kernel:
```bash
cd /usr/src/opensuse-leap-linux-5.3.18-custom-rt
patch -p1 < ../opensuse-leap-linux-5.3.18-scarlett-gen3-rt.patch
```

### Configuring the kernel

Copy your current configuration file to the kernel source tree:
```bash
cp /boot/config-`uname -r` ./.config
```

Optionally, you may configure your custom kernel to not to clash with standard kernel, run menuconfig:
```bash
make menuconfig
```

Then go to ```General setup``` and modify the parameter ```Local version -> append to kernel release``` to match the linux kernel
directory name. For ```opensuse-leap-linux-5.3.18-custom-rt``` the value should be ```-custom-rt```.

Save configuration and leave.

### Building and installing kernel

Issue the following commands:

```bash
make
```

After the successful build, install the kernel by issuing commands:
```bash
make modules_install
make install
```

This will install kernel modules, initrd and kernel. Additionally, it will add the kernel as a boot option to GRUB configuraton.

### Configuring audio module

You need to enable the driver at module load time with the ```device_setup=1``` option to insmod/modprobe. Otherwise the driver won't start
after the device is connected to the computer. You need to create additional ```modprobe``` file which properly configures the driver:

```bash
echo "options snd_usb_audio device_setup=1,1,1,1" > /etc/modprobe.d/scarlett-gen3.conf
echo "options snd_usb_audio vid=0x1235 pid=0x8210 device_setup=1" >> /etc/modprobe.d/scarlett-gen3.conf
echo "options snd_usb_audio vid=0x1235 pid=0x8211 device_setup=1" >> /etc/modprobe.d/scarlett-gen3.conf
echo "options snd_usb_audio vid=0x1235 pid=0x8212 device_setup=1" >> /etc/modprobe.d/scarlett-gen3.conf
echo "options snd_usb_audio vid=0x1235 pid=0x8213 device_setup=1" >> /etc/modprobe.d/scarlett-gen3.conf
echo "options snd_usb_audio vid=0x1235 pid=0x8214 device_setup=1" >> /etc/modprobe.d/scarlett-gen3.conf
echo "options snd_usb_audio vid=0x1235 pid=0x8215 device_setup=1" >> /etc/modprobe.d/scarlett-gen3.conf
```

These options will enable driver for all supported Gen3 devices.

Enabling MSD (Mass Storage Device) mode seems to me to be something that you would never want to do, so once you disable it the
control gets hidden (use device_setup=3 if you really want to make it reappear).

Some devices like Scarlett 2i2 may become suspended by the ```TLP``` system tool. To deny automatic suspend of USB device,
you need to edit it's configuration file and add list of USB identifiers of the devices to the ```USB_BLACKLIST``` parameter, 
for example:

```
echo 'USB_BLACKLIST="1235:8210 1235:8211 1235:8212 1235:8213 1235:8214 1235:8215"' >> /etc/default/tlp
```

### Booting new kernel

Restart operating system. Once Grub provides boot menu, select your custom kernel and boot it.

Connect the device and turn it on. All should work fine.

## Additional information about Scarlett devices

This ALSA mixer gives access to (model-dependent):
* input, output, mixer-matrix muxes
* 18x10 mixer-matrix gain stages
* gain/volume controls
* level meters
* line/inst level, pad, and air controls
* enable/disable MSD mode
* main/alt speaker switching

The overall block scheme of the device:

```
    /--------------\    18chn            20chn     /--------------\
    | Hardware  in +--+------\    /-------------+--+ ALSA PCM out |
    \--------------/  |      |    |             |  \--------------/
                      |      |    |    /-----\  |
                      |      |    |    |     |  |
                      |      v    v    v     |  |
                      |   +---------------+  |  |
                      |    \ Matrix  Mux /   |  |
                      |     +-----+-----+    |  |
                      |           |          |  |
                      |           |18chn     |  |
                      |           |          |  |
                      |           |     10chn|  |
                      |           v          |  |
                      |     +------------+   |  |
                      |     | Mixer      |   |  |
                      |     |     Matrix |   |  |
                      |     |            |   |  |
                      |     | 18x10 Gain |   |  |
                      |     |   stages   |   |  |
                      |     +-----+------+   |  |
                      |           |          |  |
                      |18chn      |10chn     |  |20chn
                      |           |          |  |
                      |           +----------/  |
                      |           |             |
                      v           v             v
                      ===========================
               +---------------+       +--—------------+
                \ Output  Mux /         \ Capture Mux /
                 +---+---+---+           +-----+-----+
                     |   |                     |
                10chn|   |                     |18chn
                     |   |                     |
  /--------------\   |   |                     |   /--------------\
  | S/PDIF, ADAT |<--/   |10chn                \-->| ALSA PCM in  |
  | Hardware out |       |                         \--------------/
  \--------------/       |
                         v
                  +-------------+    Software gain per channel.
                  | Master Gain |<-- 18i20 only: Switch per channel
                  +------+------+    to select HW or SW gain control.
                         |
                         |10chn
  /--------------\       |
  | Analogue     |<------/
  | Hardware out |
  \--------------/
```

Gen 3 devices have a Mass Storage Device (MSD) mode where a small
disk with registration and driver download information is presented
to the host. To access the full functionality of the device without
proprietary software, MSD mode can be disabled by:
* holding down the 48V button for five seconds while powering on the device, or
* using this driver and alsamixer to change the "MSD Mode" setting to Off, waiting
  two seconds, then power-cycling the device
