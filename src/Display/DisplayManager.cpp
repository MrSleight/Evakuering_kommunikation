#include <Arduino.h>
#include <U8g2lib.h>
#include <stdio.h>

#include "DisplayManager.h"

// OLED-displayens I2C-pinnar
namespace
{

    constexpr uint8_t OLED_SCL = 22;
    constexpr uint8_t OLED_SDA = 21;

    // SH1107G Seeed OLED 128x128
    // Clock = GPIO22
    // Data  = GPIO21
    U8G2_SH1107_SEEED_128X128_1_HW_I2C oled(
        U8G2_R0,
        U8X8_PIN_NONE,
        OLED_SCL,
        OLED_SDA);

    // Sparar aktuell information
    bool currentDirectionRight = false;
    uint8_t currentNodeNumber = 1;

    // Ritar hela skärmens innehåll
    void updateScreen()
    {

        oled.firstPage();

        do
        {

            // Visa nodnummer uppe till höger
            char nodeText[12];

            snprintf(
                nodeText,
                sizeof(nodeText),
                "NOD: %u",
                static_cast<unsigned>(currentNodeNumber));

            oled.setFont(u8g2_font_6x12_tf);
            oled.drawStr(78, 12, nodeText);

            // Visa rubriken
            oled.setFont(u8g2_font_ncenB14_tr);
            oled.drawStr(23, 42, "UTGANG:");

            // Rita den horisontella delen av pilen
            oled.drawHLine(28, 78, 72);

            // Rita pilspetsen
            if (currentDirectionRight == true)
            {

                // Högerpil
                oled.drawLine(100, 78, 86, 65);
                oled.drawLine(100, 78, 86, 91);
            }
            else
            {

                // Vänsterpil
                oled.drawLine(28, 78, 42, 65);
                oled.drawLine(28, 78, 42, 91);
            }

        } while (oled.nextPage());
    }
}

// Intilize display
void displayInitialize()
{
    beginDisplay();

    displaySetDirection(false);
    displaySetNodeNumber(3);
}

// STARTA DISPLAYEN
void beginDisplay()
{

    oled.begin();

    // Rita första skärmbilden
    updateScreen();
}

// ÄNDRA RIKTNING
void displaySetDirection(bool directionRight)
{

    currentDirectionRight = directionRight;

    updateScreen();
}

// ÄNDRA NODNUMMER
void displaySetNodeNumber(uint8_t nodeNumber)
{

    // Godkänn bara nummer 1, 2 eller 3
    if (nodeNumber < 1 || nodeNumber > 3)
    {

        return;
    }

    currentNodeNumber = nodeNumber;

    updateScreen();
}
