
uint16_t RGB16(uint8_t r, uint8_t g, uint8_t b) {
 return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
}

// Display Colors
uint16_t BLACK	=	RGB16(0, 0, 0);
uint16_t RED	=	RGB16(255, 0, 0);
uint16_t GREEN	=	RGB16(0, 255, 0);
uint16_t BLUE 	=	RGB16(0, 0, 255);
uint16_t YELLOW	=	RGB16(255, 255, 0);
uint16_t CYAN 	=	RGB16(0, 255, 255);
uint16_t PINK	=	RGB16(255, 0, 255);
uint16_t WHITE	=	RGB16(255, 255, 255);
uint16_t ORANGE	=	RGB16(255, 128, 0);
uint16_t PURPLE	=	RGB16(128, 0, 255);
uint16_t DAYBLUE=	RGB16(0, 128, 255);
uint16_t LIME 	=	RGB16(128, 255, 0);
uint16_t DARKGREEN	=	RGB16(0, 150, 0);
uint16_t DARKRED    =	RGB16(150, 0, 0);
uint16_t DARKBLUE   =	RGB16(0, 0, 150);
uint16_t DARKYELLOW	=	RGB16(150, 150, 0);
uint16_t DARKPURPLE	=	RGB16(80, 0, 150);
uint16_t DARKMAGENTA=	RGB16(150, 0, 150);
uint16_t DARKCYAN   =	RGB16(0, 150, 150);
uint16_t DARKORANGE =	RGB16(150, 75, 0);

int hexcolorToInt(char upper, char lower)
{
int uVal = (int)upper;
int lVal = (int)lower;
uVal = uVal > 64 ? uVal - 55 : uVal - 48;
uVal = uVal << 4;
lVal = lVal > 64 ? lVal - 55 : lVal - 48;
// Serial.println(uVal+lVal);
return uVal + lVal;
}

uint16_t hex2rgb(char* str) {
 String hexvalue = (String)str;
 hexvalue.toUpperCase();
 char c[7];
 hexvalue.toCharArray(c, 8);
 int red = hexcolorToInt(c[1], c[2]);
 int green = hexcolorToInt(c[3], c[4]);
 int blue = hexcolorToInt(c[5], c[6]);
 return RGB16(red, green, blue);
}

/*
uint16_t hex2rgb(char* hex) {
    Serial.println((String)"hex: " + hex);
    uint32_t rgb=strtoul(hex+1,NULL,16);
    Serial.println((String)"rgb: " + rgb);
    byte r=(byte)(rgb>>16);
    byte g=(byte)(r>>8);
    byte b=(byte)rgb;
    Serial.println((String)"r: " + r + " g: " + g + " b: " + b);
    return RGB16(r, g, b);
}
*/
uint16_t hsv2rgb(uint8_t hsvr)
{
    RgbColor rgb;
    unsigned char region, remainder, p, q, t;
    HsvColor hsv;
    hsv.h = hsvr;
    hsv.s = 255;
    hsv.v = 255;
    region = hsv.h / 43;
    remainder = (hsv.h - (region * 43)) * 6; 
    p = (hsv.v * (255 - hsv.s)) >> 8;
    q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
    t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;
    switch (region)
    {
        case 0:
            rgb.r = hsv.v; rgb.g = t; rgb.b = p;
            break;
        case 1:
            rgb.r = q; rgb.g = hsv.v; rgb.b = p;
            break;
        case 2:
            rgb.r = p; rgb.g = hsv.v; rgb.b = t;
            break;
        case 3:
            rgb.r = p; rgb.g = q; rgb.b = hsv.v;
            break;
        case 4:
            rgb.r = t; rgb.g = p; rgb.b = hsv.v;
            break;
        default:
            rgb.r = hsv.v; rgb.g = p; rgb.b = q;
            break;
    }
    return RGB16(rgb.r, rgb.g, rgb.b);
}