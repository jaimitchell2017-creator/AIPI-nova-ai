# Nova for XORIGIN AIPI Lite

Phase 1: clock + weather desk display. Built in the cloud by GitHub, flashed from your browser.

## Setup (one time)
1. Create a free GitHub account, then a **public** repository (e.g. `nova-aipi-lite`).
2. Upload everything in this folder to the repository (use "Add file -> Upload files").
   Make sure `.github/workflows/build.yml` ends up in the repo. If your upload skips the
   hidden `.github` folder, use "Add file -> Create new file", type the name
   `.github/workflows/build.yml`, and paste the file contents.
3. In the repo go to **Settings -> Pages -> Build and deployment -> Source: GitHub Actions**.
4. Open the **Actions** tab. The build starts by itself (first run takes several minutes).
   Wait for a green tick. If it goes red, open the run, copy the error text, and send it to Claude.
5. Your flasher page is at `https://YOUR-USERNAME.github.io/REPO-NAME/`.

## Flashing
1. Open the flasher page in Chrome or Edge.
2. Plug the AIPI Lite in with a USB-C data cable and click **Install Nova**.
3. If it will not connect: remove the 4 screws on the back, open the cover carefully
   (the screen ribbon and speaker wires are attached), and hold the small BOOT button next to
   the ESP32-S3 module while plugging it in. Then try again.
4. To get the stock firmware back later, flash the official XiaoZhi firmware for the AIPI Lite
   (the xiaozhi-esp32 project added support for this board).

## First boot
- A test pattern shows for 2.5 seconds: white border, red/green/blue/yellow corner squares.
- Then join the Wi-Fi `Nova-Setup` on your phone. Enter your home Wi-Fi, city, 12/24h, and C/F.

## If the screen looks wrong
Edit `src/config.h`, commit, wait for the green tick, and flash again:
- picture shifted or cropped -> change `LCD_OFFSET_X` / `LCD_OFFSET_Y`
- colours look negative -> `LCD_INVERT`
- red and blue swapped -> `LCD_RGB_ORDER`
- upside down or sideways -> `LCD_ROTATION`

## Pins used (community-verified, see config.h)
Display: BL 3, DC 7, CS 15, SCLK 16, MOSI 17, RST 18. Button: GPIO42.
