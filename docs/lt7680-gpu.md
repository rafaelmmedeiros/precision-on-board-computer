# LT7680 — capacidades e limites da GPU

Extraído de `CLAUDE.md` §8.1. Consultar quando uma decisão de UI tocar em
primitivas de desenho, modos de cor ou fontes.


**Nativo via ER_TFT (lib/LT7680):**
- `DrawSquare`/`DrawSquare_Fill` — retângulos (outline e preenchido)
- `DrawCircle_Fill`, `Start_Circle_or_Ellipse` — círculos e elipses
- `Start_Line` — linhas arbitrárias
- `Start_Triangle`/`_Fill` — triângulos
- `Start_Circle_Square` — retângulos com cantos arredondados
- `DrawPixel` — único, lento via SPI; usar só em casos pontuais
- BTE (Block Transfer Engine) — copiar/mover regiões da SDRAM com ROP codes e alpha blending; útil pra sprites e composição
- Múltiplas layers (≥11) de 614400 bytes na SDRAM ~8 MB

**Modos de cor:** 16bpp RGB565 (atual, 65K cores), 24bpp RGB888 (16M, 2× memória), 8bpp indexado.

**Texto via CGROM interna:** 8×16, 12×24, 16×32 ASCII (Latin-1/ISO-8859-1/2/3/4), zoom 1-4x independente em W/H, rotação 0° ou 90° (no modo 90° há H-flip embutido — compensado por `VDIR=1`, mount atual).

**Limitações reais:**
- Sem rotação nativa de bitmaps (só texto 0°/90°)
- Sem antialiasing (workaround: supersample + BTE)
- Sem gradientes nativos (BTE alpha blending ou muitos rects)
- SPI a 8 MHz — ~3 µs por registrador, limita ops granulares (mas fill/BTE rodam na GPU sem overhead de pixel)

**Disponível pra adicionar:**
- Chip de flash SPI externo (W25Q64) nos pinos SFI do LT7680 — fontes TrueType pré-rasterizadas, sprites grandes
- Bitmap font em software — sem hardware extra
