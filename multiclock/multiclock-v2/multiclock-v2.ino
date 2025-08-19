/*
(1) 74HC595:
    Control pins -> MCU:
        SRCLK -> IO0
        RCLK -> IO1
        SER -> IO3
        QH' -> (2) 74HC595 SER
    Outputs -> Displays:
        QA -> CLR
        QB -> A0
        QC -> A1
        QD -> Display 1 WR
        QE -> Display 2 WR
        QF -> Display 3 WR
        QG -> Display 4 WR
        QH -> Display 5 WR

(2) 74HC595:
    Control pins -> MCU:
        SRCLK -> IO0
        RCLK -> IO1
        SER -> (1) 74HC595 QH'
    Outputs -> Displays
        QA -> D0
        QB -> D1
        QC -> D2
        QD -> D3
        QE -> D4
        QF -> D5
        QG -> D6

GPS -> MCU:
    V -> 3.3V
    G -> GND
    T -> IO3
    R -> IO4

DHT11 -> MCU:
    VCC -> 5V
    GND -> GND
    DATA -> IO5

DS3231M -> MCU:
    VCC -> 5V
    GND -> GND
    VBAT -> CR2032 +
    SDA -> IO6 (pullup with 4.7k -> 3.3V)
    SCL -> IO7 (pullup with 4.7k -> 3.3V)

Buzzer -> MCU:
    + -> IO8
    - -> GND
*/

void setup()
{
}

void loop()
{
}