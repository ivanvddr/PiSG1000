# PiSG1000
PiSG1000 is a Sega SG-1000 emulator developed for the Raspberry Pi Pico (RP2040) using the Arduino IDE environment.
The project integrates support for the ILI9341 SPI display, game loading from SD or flash and i2s audio output.

# 🚀 Features
  -  Core: Emulation of Z80 CPU, VDP video, PSG (SN76489) audio.
  -  Display: ILI9341 320×240 SPI support.
  -  Storage: Loading .sg files and .jpg previews from flash via LittleFS or MicroSD.
  -  Audio: i2s sound generation.
  -  Input: One/Dual joystick ports (via GPIO).


# 🛠️ Hardware Requirements
To build the project, you will need the following components:

 - Microcontroller: Raspberry Pi Pico (RP2040).
       <br/><img width="300" height="300" alt="pico" src="https://github.com/user-attachments/assets/57d94fe2-ba95-4041-8d19-b6f1106df1b2" />
     
 - Display: 2.8" ILI9341 SPI (320×240) with integrated SD reader.
       <br/><img width="640" height="200" alt="tft-display-2 8-spi" src="https://github.com/user-attachments/assets/6522d432-fa3d-4541-953e-193235030034" />

 - Audio:
    - MAX98357 amplifier.
          <br/><img width="231" height="231" alt="image" src="https://github.com/user-attachments/assets/7d0264ac-8058-4daf-ae16-1d583d8d29f8" />

    - 3W / 4–8Ω speaker
          <br/><img width="300" height="300" alt="speaker" src="https://github.com/user-attachments/assets/8fcebdf6-4df0-4d92-b6e8-447f233d5c6f" />

 - Input:
    - 2× directional joystick boards with 5‑way switch + 2 buttons (Joystick 1 and 2) + some other button if you want full compatibility.
          <br/><br/><img width="300" height="161" alt="COM-5WS-01" src="https://github.com/user-attachments/assets/ae02c59a-e821-4228-8e59-e08bd2625be7" />



# 🔌 Wiring
The Raspberry Pi Pico is the core of the system. Below are the detailed connections, divided by module.

## 1. ILI9341 Display & SD Card

| Component | Module Pin | Pico Pin (GPIO) | Note |
|----------|-------------|------------------|------|
| Display  | VCC         | VSYS             | Powered directly from Pico |
|          | GND         | GND              | |
|          | SDO (MISO)  | GP16             | |
|          | LED         | 3V3              | |
|          | SCK         | GP18             | |
|          | SDI (MOSI)  | GP19             | |
|          | DC          | GP21             | |
|          | RESET       | GP20             | |
|          | CS          | GP17             | |
| SD Card  | SD_CS       | GP13             | |
|          | SD_SCK      | GP10             | |
|          | SD_MOSI     | GP11             | |
|          | SD_MISO     | GP12             | |

## 2. Audio (i2s Amplifier)

| Component | Module Pin | Pico Pin (GPIO) | Note |
|----------|-------------|------------------|------|
| Ampl.    | VCC         | VBUS             | 5V |
|          | GND         | GND              | |
|          | BCLK        | GP26             | |
|          | LRC         | GP27             | |
|          | DIN         | GP28             | |


## 3. Joysticks

Joysticks use internal pull‑up resistors.
Each button closes the contact to GND.

| Joystick 1 GPIO | Function | Joystick 2 GPIO | Function |
|-----------------|----------|-----------------|----------|
| GP2             | UP       | GP1              | UP |
| GP3             | DOWN     | GP16             | DOWN |
| GP4             | LEFT     | GP9             | LEFT |
| GP5             | RIGHT    | GP8             | RIGHT |
| GP6             | BTN1     | GP22             | BTN1 |
| GP7             | BTN2     | GP0             | BTN2 |


# 📝 Technical Notes

- Power:<br/>
    Pico powered via USB.<br/>
    Audio amplifier requires 5V<br/>

- TFT_eSPI Configuration:<br/> 
    Replace User_Setup.h with:<br/>
        - Driver: ILI9341<br/>
        - SPI Frequency: 80MHz<br/>
        - DMA: Enabled<br/>

If you notice graphical glitches, reduce SPI frequency to 40–50MHz.

# 📂 Software Setup

The project is developed for Arduino IDE using the RP2040 core.<br/> 

## Required Libraries

Make sure you have installed:<br/> 
    - TFT_eSPI (configured for ILI9341 on Pico)<br/> 
    - TJpg_Decoder (just for game previews, you can disable this function)<br/> 
    - LittleFS<br/> 

## File System Structure

The emulator looks for files inside the /PiSMS directory.<br/>
Organize your FS like this:<br/>

        PiSG1000/
        ├─ games/
        │   ├─ game1.sg
        │   ├─ game1.jpg
        │   ├─ gamex.sg
        │   ├─ gamex.jpg
        │   └─ noImage.jpg
        
Images must be 200x150px jpg no progressive format.<br/>

## Using LittleFS Instead of a Physical SD Card

Instead of using a physical SD card, you can store the entire PiSG1000 folder directly inside the Pico’s onboard flash memory.<br/>
To do this, use the LittleFS Upload Tool for Arduino IDE, which uploads the contents of your local /data folder into the Pico’s flash filesystem.<br/>
Official documentation:<br/>
Arduino-Pico LittleFS guide:<br/>  
https://arduino-pico.readthedocs.io/en/latest/filesystems.html (arduino-pico.readthedocs.io in Bing)<br/>
LittleFS Upload Tool (arduino-littlefs-upload):<br/> 
https://github.com/earlephilhower/arduino-littlefs-upload (github.com in Bing)<br/>

# 🎮 Usage

1. **Startup**: On boot, the main menu “SGMenu” appears.<br/>
2. **Navigation**: Use the joysticks to move between options:<br/>
   **GAMES BROWSER**: Scroll through the game list with the joystick; press Fire to load the selected game.<br/>
   **OPTIONS**: not yet implemented.<br/>

# ⚡Performance & Overclock

To achieve acceptable emulation speed, the Raspberry Pi Pico is pushed beyond its factory specifications.<br/>
In configs.h:<br/>

            #define PICO_CLOCK_KHZ 300000 // Overclock to 300MHz
            
According to available documentation, 300MHz is generally achievable without special precautions.<br/>
This frequency allows:<br/>
    - SPI bus at 80MHz for the display<br/>
    - Real‑time processing CPU cycles and audio<br/>
You may still notice slowdowns in some games, graphical glitches, or audio ticks due to timing desynchronization.<br/>

# ⚠️ Disclaimer

This project is provided “as is”, for hobby and experimental use.<br/>
Overclock: May reduce microcontroller lifespan or cause thermal instability.<br/>
Hardware: The author is not responsible for damage to components (Pico, display, speaker), overheating, or malfunctions caused by incorrect wiring or use of the provided software.<br/>
Risks: Any hardware or software modification is performed at the user’s own risk. 
