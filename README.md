# README #

Simple custom MSP430F5529 bootloader for mspgcc.

## How to set things up? A short demo
Download, build, connect your MSP430F5529-LP, go to Debug directory and issue:

`mspdebug tilib`
and while in mspdebug:

`read ../setup.read`

This will erase the flash, program the bootloader and put two applications onto the flash: blinking-red-LED as a primary/working app and blinking-green-LED onto the download area. Anytime the MCU is reset it will start the bootloader and the bootloader will start blinking-red-LED application. This is because the image status flag is zero indicating no actions needed (`BL_IMAGE_NONE`).

Start with `run`, the red LED starts blinking.

Now we are ready to tell the bootloader that there is a new image in the download area so to reflash the main application with a new one. Abort program execution with *Ctrl+C* and type:

`read ../download.read`

This will set image status flag to `BL_IMAGE_DOWNLOAD`.

Right now upon power up the bootloader reads the `image status flag = BL_IMAGE_DOWNLOAD` and will start the reflash procedure. It will first backup the existing image in flash bank B, then erase application, copy image from the download area to application area and finally start the new application. At the end it will set up `image status flag = BL_IMAGE_PENDING_VALIDATION` which means that the new image must validate itself (set `image status flag = BL_IMAGE_VALIDATED`). If it doesn't and the bootloader starts with `image status flag = BL_IMAGE_PENDING_VALIDATION` the bootloader assumes the image is broken and will start recovering from the backup area. After that it will set `image status flag = BL_IMAGE_RECOVERED`.

Typing `run` starts the reflashing process; it will take a couple of seconds and end up with a green LED blinking. That means the reflashing process went smoothly. Be careful - at this point `image status flag = BL_IMAGE_PENDING_VALIDATION` so if you restart the MCU it will recover the application.

## How it works?
Let's define two main items: a bootloader and the application. The bootloader is persistent through reflashing iterations while the application is what gets changed.

The bootloader uses all flash available on the MSP430F5529 microcontroller and divides it into three logical areas: application, backup and download regions. They are mapped onto the flash banks A/B, C and D and are defined aside to all other critical parameters in bootloader.h.
The bootloader's actions are driven by "image status" flag that is toggled by the bootloader and the application. This flag is stored in flash information memory.

As per current implementation the bootloader fits into `0x4400-0x53FF` with its reset vector residing at `0xFFFE-0xFFFF` and expects the application to start from 0x5400.
The main MCU's vector table (`0xFF80-0xFFFD`) is available for the application, however the application's reset vector is stored at `0xFF7E` instead of `0xFFFE` (the MCU must run a bootloader at each power up).

The bootloader expects the download image to start at `0x14400` and its vector table at `0x1C380`. While reflashing (`image status flag = BL_IMAGE_DOWNLOAD`) the bootloader takes care of preserving the bootloader's reset vector and storing the application's reset vector at `0xFF7E` so the only thing your application needs to do is to put the image to `0x14400 - 0x1C37F` and the vector table to `0x1C380 - 0x1C3FF`.

They say a picture is worth a thousand words, so here it comes:

![memmap.png](https://bitbucket.org/repo/4MXppE/images/2425935257-memmap.png)

Now, looking at the .read files you can deduct the rest. Else, let me know to enhance this readme.