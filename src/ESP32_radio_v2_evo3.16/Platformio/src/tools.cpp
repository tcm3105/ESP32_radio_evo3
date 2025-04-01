#include "tools.h"

void Tools::processText(String &text)
{
    for (int i = 0; i < text.length(); i++)
    {
        switch (text[i])
        {
        case (char)0xC2:
            switch (text[i + 1])
            {
            case (char)0xB3:
                break;
            case (char)0x9C:
                break;
            case (char)0x8C:
                break;
            case (char)0xB9:
                break;
            case (char)0x9B:
                break;
            case (char)0xBF:
                break;
            case (char)0x9F:
                break;
            }
            break;
        case (char)0xC3:
            switch (text[i + 1])
            {
            case (char)0xB1:
                break;
            case (char)0xB3:
                break;
            case (char)0xBA:
                break;
            case (char)0xBB:
                break;
            case (char)0x93:
                break;
            }
            break;
        case (char)0xC4:
            switch (text[i + 1])
            {
            case (char)0x85:
                break;
            case (char)0x99:
                break;
            case (char)0x87:
                break;
            case (char)0x84:
                break;
            case (char)0x98:
                break;
            case (char)0x86:
                break;
            }
            break;
        case (char)0xC5:
            switch (text[i + 1])
            {
            case (char)0x82:
                break;
            case (char)0x84:
                break;
            case (char)0x9B:
                break;
            case (char)0xBB:
                break;
            case (char)0xBC:
                break;
            case (char)0x83:
                break;
            case (char)0x9A:
                break;
            case (char)0x81:
                break;
            case (char)0xB9:
                break;
            case (char)0xBA:
                break;
            }
            break;
        }
    }
}

// Funkcja odwracania bitów MSL-LSB <-> LSB-MSB
uint32_t Tools::reverse_bits(uint32_t inval, int bits)
{
  if (bits > 0)
  {
    bits--;
    return reverse_bits(inval >> 1, bits) | ((inval & 1) << bits);
  }
  return 0;
}