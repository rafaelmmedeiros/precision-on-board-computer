# fontgen

Gera cabeçalho + fonte C++ com glyphs bitmap 1 bpp a partir de uma TTF.

## Uso

```bash
# 1. Baixa o release DSEG (uma vez):
curl -sL -o dseg.zip https://github.com/keshikan/DSEG/releases/download/v0.46/fonts-DSEG_v046.zip

# 2. Extrai os pesos que interessam:
python -c "import zipfile; z=zipfile.ZipFile('dseg.zip'); \
  [open(n.split('/')[-1],'wb').write(z.read(n)) for n in z.namelist() \
   if n.endswith('DSEG7Classic-Bold.ttf') or n.endswith('DSEG7Classic-Regular.ttf')]"

# 3. Roda o gerador:
python gen.py
```

Saídas:
- `include/Fonts_DSEG7.h` — declarações `extern const BitmapFont`
- `src/Fonts_DSEG7.cpp` — bitmaps + tabelas (go direto na flash)

## Customização

Abre `gen.py` e edita no topo:
- `FONT_FILE` — caminho da TTF
- `CHARS` — string com os chars a incluir
- `TARGETS` — lista de alturas-alvo (px) para variantes

O gerador usa busca binária no "point size" da PIL pra atingir exatamente
a altura-alvo do dígito "8".
