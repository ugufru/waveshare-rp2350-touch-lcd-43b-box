# waveshare-rp2350-touch-lcd-43b-box

PlatformIO project for the [Waveshare RP2350-Touch-LCD-4.3B](https://www.waveshare.com/rp2350-touch-lcd-4.3b.htm) board in an enclosure with a 4.3" touch screen.

## Board Specs

- **MCU**: RP2350B (dual Cortex-M33/RISC-V, 48 GPIO)
- **Display**: 4.3" 800x480 RGB565 LCD (ST7262 controller, PIO-driven)
- **Touch**: Capacitive GT911 (I2C, up to 5 touch points)
- **PSRAM**: 2MB QSPI (for framebuffer storage — 768KB per frame)
- **Clock**: 240MHz (overclocked from 150MHz default)

## Project Structure

```
platformio/
├── platformio.ini          # PlatformIO build config (earlephilhower core)
├── deploy.sh               # Build & flash script
├── src/
│   └── main.cpp            # Hello world — display text + touch
└── lib/
    ├── bsp/                # Board support (ported from Waveshare SDK demo)
    │   ├── bsp_display.h   # Display interface
    │   ├── bsp_touch.h     # Touch interface
    │   ├── bsp_i2c.*       # I2C1 driver (pins 6/7, 400kHz)
    │   ├── bsp_dma_channel_irq.*  # DMA IRQ multiplexer
    │   ├── bsp_st7262.*    # LCD init + brightness (PWM on pin 40)
    │   ├── bsp_gt911.*     # Touch controller (I2C addr 0x5D)
    │   ├── pio_rgb.*       # PIO RGB parallel driver (pio1 + pio2)
    │   └── pio_rgb.pio     # PIO assembly for HSYNC/VSYNC/DE/RGB
    ├── psramlib/           # PSRAM allocator
    │   ├── psram_tool.*    # PSRAM QSPI init (CS on pin 47)
    │   ├── rp_pico_alloc.* # TLSF-based malloc for PSRAM
    │   └── tlsf/           # TLSF memory allocator
    └── gui_paint/          # Drawing primitives & fonts
        ├── GUI_Paint.*     # Lines, rectangles, circles, text
        └── font*.c         # Font data (8/12/16/20/24pt)
```

## Pin Assignments

| Function | Pin(s) | Notes |
|----------|--------|-------|
| LCD DE | 20 | Data Enable |
| LCD VSYNC | 21 | Vertical sync |
| LCD HSYNC | 22 | Horizontal sync |
| LCD PCLK | 23 | Pixel clock |
| LCD DATA0-15 | 24-39 | 16-bit RGB565 data |
| LCD RST | 19 | Display reset |
| LCD BL | 40 | Backlight PWM |
| LCD EN | 18 | Display enable |
| Touch INT | 16 | GT911 interrupt |
| Touch RST | 17 | GT911 reset |
| I2C1 SDA | 6 | Touch I2C data |
| I2C1 SCL | 7 | Touch I2C clock |
| PSRAM CS | 47 | QSPI chip select |

## Build & Flash

```bash
cd platformio
pio run              # Build
pio run -t upload    # Flash via USB-C
pio device monitor   # Serial monitor (115200 baud)

# Or use the deploy script:
./deploy.sh --monitor
```

## Architecture Notes

The display uses PIO-driven RGB parallel signaling across 2 PIO blocks (pio1 for sync, pio2 for data). DMA continuously transfers framebuffer data from PSRAM through SRAM transfer buffers to the PIO FIFO. The 768KB framebuffer must live in PSRAM since it exceeds the 520KB SRAM.
