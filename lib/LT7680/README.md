# LT7680 driver — P-OBC

Driver oficial da BuyDisplay para o display **ER-TFT4.58-1** (320×960 IPS,
ST7701S) com breakout **LT7680**, 4-wire SPI.

Origem: `extra/ER-TFT4.58-1_ESP32_Tutorial/Display_DMA_Cartoon_Font_test/`.
Arquivos `LCD.h`, `LCD.cpp` copiados verbatim. `LCD_init.h` idem, **com
remapeamento de pinos** (ver abaixo).

## Pinagem (ESP32 WROOM-32)

### HW SPI (LT7680 — framebuffer / GPU)
Definida em `LCD.cpp` e usada pela classe `ER_TFT`.

| ESP32 | Display pin |
|---|---|
| 18 | SCK (pino 8) |
| 23 | MOSI (pino 7) |
| 19 | MISO (pino 6) |
|  5 | CS (pino 5) |
| 16 | RES (pino 11) |
| 3.3V | BL (pino 9) — backlight fixo |
| GND  | pinos 1,2 |
| 5V ou 3.3V | VCC (pinos 3,4) |

### SW SPI (config do ST7701S — só durante init)
Demo original usava GPIO 11/13/12. **GPIO 11 é reservado pelo flash no
WROOM-32** — remapeado para pinos seguros:

| ESP32 | Display pin |
|---|---|
| 17 | CS (pino 16) |
| 14 | SCK (pino 19) |
|  4 | SDI (pino 18) |

Estes pinos só são pulsados uma vez em `ST7701S_Initial()`. Depois não são
tocados novamente.

## Como usar

```cpp
#include "LCD.h"
#include "LCD_init.h"   // **inclua em apenas um .cpp** (ST7701S_Initial é inline no header)

void setup() {
    ER_TFT.Parallel_Init();
    ER_TFT.HW_Reset();
    ER_TFT.System_Check_Temp();
    while (ER_TFT.LCD_StatusRead() & 0x02);
    ER_TFT.initial();
    ST7701S_Initial();
    ER_TFT.Display_ON();

    ER_TFT.Select_Main_Window_16bpp();
    ER_TFT.Main_Image_Start_Address(layer1_start_addr);
    ER_TFT.Main_Image_Width(LCD_XSIZE_TFT);
    ER_TFT.Main_Window_Start_XY(0, 0);
    ER_TFT.Canvas_Image_Start_address(layer1_start_addr);
    ER_TFT.Canvas_image_width(LCD_XSIZE_TFT);
    ER_TFT.Active_Window_XY(0, 0);
    ER_TFT.Active_Window_WH(LCD_XSIZE_TFT, LCD_YSIZE_TFT);

    ER_TFT.DrawSquare_Fill(0, 0, LCD_XSIZE_TFT, LCD_YSIZE_TFT, Red);
}
```

Texto via CGROM interno do LT7680:

```cpp
ER_TFT.Foreground_color_65k(White);
ER_TFT.Background_color_65k(Black);
ER_TFT.CGROM_Select_Internal_CGROM();
ER_TFT.Font_Select_16x32_32x32();  // também: 8x16_16x16, 12x24_24x24
ER_TFT.Font_Width_X2(); ER_TFT.Font_Height_X2();  // opcional: zoom 1..4
ER_TFT.Goto_Text_XY(10, 10);
ER_TFT.Show_String("HELLO");
```
