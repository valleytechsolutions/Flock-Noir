# Synthetic JPEG fixtures

64×48 block gradients with deterministic pixel noise, generated using Pillow
11.3.0/libjpeg at quality 82. No photographs or device captures are included.
Each `.luma` file contains the independent decoder's 8×6 grayscale BOX averages.
Tests allow three levels of rounding/color-conversion error against JPEG Y DC.

Cases: grayscale, 4:4:4, 4:2:2, 4:2:0, optimized Huffman tables, restart markers
every two MCUs, and progressive JPEG (rejected). Every truncated prefix and a
deterministic mutation corpus are also checked. Fixtures require no image
library at test time.
