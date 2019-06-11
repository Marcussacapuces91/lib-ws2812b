/**
 * Copyright 2019 par Marc SIBERT
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file ws2812b.h Lib ws2812b
 * @license Apache-2.0
 * @copyright 2019 par Marc SIBERT
 * @author Marc SIBERT <marc@sibert.fr>
 */

 /**
 * Classe modélisant une ligne de LEDs RGB WS2812.
 * 
 * @tparam PIN est le bit du PORT B utilisé pour commander la ligne de LEDs
 * @tparam LEN est la longueur de la ligne de LEDs
 */
template<byte PIN = 1, byte LEN = 1, byte ORDER = 0>
class WS2812b {

public:
/**
 * Structure regroupant les valeurs Red, Green, Blue.
 */
  struct rgb_t {
    byte r;
    byte g;
    byte b;
  };

/**
 * Définition de l'ordre des composantes suivant le type de contrôleur.
 */
  enum order_t {
    RGB = 0,
    GRB = 1
  };
  
/**
 * À appeler à l'initialisation de l'application.
 * 
 * Initialise le port de sortie.
 */
  void setup() {
    DDRB |= _BV(PIN);  
  }

/**
 * Définit le triplet de la LED indiquée.
 * 
 * @warning Il s'agit d'une copie en mémoire. Pour être effectivement appliquée, il faut utiliser la méthode flush()
 * 
 * @param pos Position de la LED dans la ligne [0..n-1]
 * @param rgb Une référence sur les valeurs à appliquer à cette LED
 */
  void setRGB(const byte pos, const rgb_t& rgb) {
    if (pos < LEN) data[pos] = rgb;
  }

/**
 * Définit le triplet de la LED indiquée.
 * 
 * @param pos Position de la LED dans la ligne [0..n-1]
 * @param r Intensité de la LED rouge [0..255]
 * @param g Intensité de la LED verte [0..255]
 * @param b Intensité de la LED bleue [0..255]
 */
  void setRGB(const byte pos, const byte r, const byte g, const byte b) {
    setRGB(pos, rgb_t({r, g, b}) );
  }

/**
 * Définit le triplet de la LED indiquée à partir de l'espace de couleurs HSV
 * 
 * @param pos Position de la LED dans la ligne [0..n-1]
 * @param h Teinte ou couleur pure sur le disque de 360° [0..1535]
 * @param s Saturation (puretée de la couleur) [0..255]
 * @param v Luminosité globale [0..255]
 *
 * @source http://www.vagrearg.org/content/hsvrgb
 */
  void setHSV(const byte pos, const unsigned h, const byte s, const byte v) {
// Luminosité nul --> Noir.
    if (!v) {
      setRGB(pos, 0, 0, 0);
      return;
    }
// Saturation nulle --> Gris
    if (!s) {
      setRGB(pos, v, v, v);
      return;
    }

    const uint8_t sextant = (((h % (6*256)) >> 8) > 5) ? 5 : (h % (6*256)) >> 8;

    // Perform actual calculations
    /*
     * Bottom level: v * (1.0 - s)
     * --> (v * (255 - s) + error_corr + 1) / 256
     */
    uint16_t ww = v * (255 - s); // We don't use ~s to prevent size-promotion side effects
    ww += 1;    // Error correction
    ww += ww >> 8;    // Error correction
    const byte c = byte(ww >> 8);
  
    const uint8_t h_fraction = h & 0xff;  // 0...255
  
    if(!(sextant & 1)) {
      // *r = ...slope_up...;
      uint32_t d = v * (uint32_t)((255 << 8) - (uint16_t)(s * (256 - h_fraction)));
      d += d >> 8;  // Error correction
      d += v;   // Error correction
      switch (sextant) {
        case 0 : 
          setRGB(pos, v, d >> 16, c);
          break;
        case 2 : 
          setRGB(pos, c, v, d >> 16);
          break;
        case 4 : 
          setRGB(pos, d >> 16, c, v);
          break;
      }
    } else {
      // *r = ...slope_down...;
      uint32_t d = v * (uint32_t)((255 << 8) - (uint16_t)(s * h_fraction));
      d += d >> 8;  // Error correction
      d += v;   // Error correction
      switch (sextant) {
        case 1 : 
          setRGB(pos, d >> 16, v, c);
          break;
        case 3 : 
          setRGB(pos, c, d >> 16, v);
          break;
        case 5 : 
          setRGB(pos, v, c, d >> 16);
          break;
      }
    }
  }

/**
 * Applique l'ensemble des triplets mémorisés sur la ligne de LEDs.
 * 
 * @warning pendant la transmission, les interruptions sont désactivées.
 */
  void flush() const {
    PORTB = 0;
    delayMicroseconds(50);
    for (unsigned i = 0; i < LEN; ++i) { 
      if (ORDER == RGB) {
        sendRGB(data[i]);
      } else if (ORDER == GRB) {
        sendGRB(data[i]);
      }
    }
  }

protected:
#define NOP1 "nop\n\t" 
#define NOP2 NOP1 NOP1 
#define NOP5 NOP2 NOP2 NOP1

/**
 * Transmet un bit modulé sur la ligne pré-définie.
 * 
 * @warning Cette méthode est définie pour une fréquence de processeur de 16 MHz.
 * 
 * @param b Un booléen indiqauant l'état du bit à transmettre.
 */
  inline 
  void sendBit(const bool b) const {
    PORTB |= _BV(PIN);
    __asm__(NOP2);
    PORTB &= b ? 0x0FF : !_BV(PIN);
    __asm__(NOP5);
    PORTB &= !_BV(PIN);
  }

/**
 * Transmet le triplet de couleurs vers la ligne.
 * 
 * @warning Cette méthode désactive ponctuellement les interruptions.
 * 
 * @param rgb Un triplet de couleurs.
 */
  void sendRGB(const rgb_t& rgb) const {
    cli();
    sendBit(0x80 & rgb.r); 
    sendBit(0x40 & rgb.r); 
    sendBit(0x20 & rgb.r); 
    sendBit(0x10 & rgb.r); 
    sendBit(0x08 & rgb.r); 
    sendBit(0x04 & rgb.r); 
    sendBit(0x02 & rgb.r); 
    sendBit(0x01 & rgb.r); 
    sei();
  
    cli();
    sendBit(0x80 & rgb.g); 
    sendBit(0x40 & rgb.g); 
    sendBit(0x20 & rgb.g); 
    sendBit(0x10 & rgb.g); 
    sendBit(0x08 & rgb.g); 
    sendBit(0x04 & rgb.g); 
    sendBit(0x02 & rgb.g); 
    sendBit(0x01 & rgb.g); 
    sei();
  
    cli();
    sendBit(0x80 & rgb.b); 
    sendBit(0x40 & rgb.b); 
    sendBit(0x20 & rgb.b); 
    sendBit(0x10 & rgb.b); 
    sendBit(0x08 & rgb.b); 
    sendBit(0x04 & rgb.b); 
    sendBit(0x02 & rgb.b); 
    sendBit(0x01 & rgb.b); 
    sei();
  }
  
/**
 * Transmet le triplet de couleurs vers la ligne.
 * 
 * @warning Cette méthode désactive ponctuellement les interruptions.
 * 
 * @param rgb Un triplet de couleurs.
 */
  void sendGRB(const rgb_t& rgb) const {
    cli();
    sendBit(0x80 & rgb.g); 
    sendBit(0x40 & rgb.g); 
    sendBit(0x20 & rgb.g); 
    sendBit(0x10 & rgb.g); 
    sendBit(0x08 & rgb.g); 
    sendBit(0x04 & rgb.g); 
    sendBit(0x02 & rgb.g); 
    sendBit(0x01 & rgb.g); 
    sei();
  
    cli();
    sendBit(0x80 & rgb.r); 
    sendBit(0x40 & rgb.r); 
    sendBit(0x20 & rgb.r); 
    sendBit(0x10 & rgb.r); 
    sendBit(0x08 & rgb.r); 
    sendBit(0x04 & rgb.r); 
    sendBit(0x02 & rgb.r); 
    sendBit(0x01 & rgb.r); 
    sei();
  
    cli();
    sendBit(0x80 & rgb.b); 
    sendBit(0x40 & rgb.b); 
    sendBit(0x20 & rgb.b); 
    sendBit(0x10 & rgb.b); 
    sendBit(0x08 & rgb.b); 
    sendBit(0x04 & rgb.b); 
    sendBit(0x02 & rgb.b); 
    sendBit(0x01 & rgb.b); 
    sei();
  }
  
private:
/// Zone mémoire concervant les triplets de couleur de chaque LED de la ligne.
  rgb_t data[LEN];

};
