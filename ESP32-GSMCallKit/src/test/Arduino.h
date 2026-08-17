#ifndef ARDUINO_H
#define ARDUINO_H

#include <stdint.h>
#include <stddef.h>

#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstring>

// ============================================================
// Arduino-compatible String replacement
// ============================================================

class String
{
private:

    std::string value;

public:

    String()
        : value()
    {
    }

    String(const char *str)
        : value(str ? str : "")
    {
    }

    String(const std::string &str)
        : value(str)
    {
    }

    size_t length() const
    {
        return value.length();
    }

    const char *c_str() const
    {
        return value.c_str();
    }

    void trim()
    {
        // Remove leading whitespace
        auto first =
            std::find_if(
                value.begin(),
                value.end(),
                [](unsigned char c)
                {
                    return !std::isspace(c);
                }
            );

        // Remove trailing whitespace
        auto last =
            std::find_if(
                value.rbegin(),
                value.rend(),
                [](unsigned char c)
                {
                    return !std::isspace(c);
                }
            ).base();

        if (first < last)
        {
            value =
                std::string(first, last);
        }
        else
        {
            value.clear();
        }
    }

    void toCharArray(
        char *buffer,
        size_t bufferSize
    ) const
    {
        if (buffer == nullptr ||
            bufferSize == 0)
        {
            return;
        }

        size_t len =
            std::min(
                value.length(),
                bufferSize - 1
            );

        std::memcpy(
            buffer,
            value.c_str(),
            len
        );

        buffer[len] = '\0';
    }

    int toInt() const
    {
        try
        {
            return std::stoi(value);
        }
        catch (...)
        {
            return 0;
        }
    }

    bool operator==(
        const char *other
    ) const
    {
        return value ==
               (other ? other : "");
    }

    bool operator!=(
        const char *other
    ) const
    {
        return !(*this == other);
    }
};


// ============================================================
// Fake Arduino Serial
// ============================================================

class FakeSerial
{
private:

    std::string inputBuffer;

public:

    // --------------------------------------------------------
    // Serial initialization
    // --------------------------------------------------------

    void begin(unsigned long baud)
    {
        std::cout
            << "[Serial] begin("
            << baud
            << ")"
            << std::endl;
    }


    // --------------------------------------------------------
    // Check whether input is available
    // --------------------------------------------------------

    bool available() const
    {
        return !inputBuffer.empty();
    }


    // --------------------------------------------------------
    // Read until delimiter
    // --------------------------------------------------------

    String readStringUntil(
        char delimiter
    )
    {
        size_t position =
            inputBuffer.find(delimiter);

        // No delimiter found
        if (position == std::string::npos)
        {
            String result(inputBuffer);

            inputBuffer.clear();

            return result;
        }

        // Extract everything before delimiter
        String result(
            inputBuffer.substr(
                0,
                position
            )
        );

        // Remove extracted data + delimiter
        inputBuffer.erase(
            0,
            position + 1
        );

        return result;
    }


    // --------------------------------------------------------
    // Inject fake user input
    // --------------------------------------------------------

    void injectInput(
        const char *input
    )
    {
        if (input != nullptr)
        {
            inputBuffer += input;
        }
    }


    // --------------------------------------------------------
    // print()
    // --------------------------------------------------------

    void print(
        const char *text
    )
    {
        std::cout << text;
    }

    void print(
        char c
    )
    {
        std::cout << c;
    }

    void print(
        int value
    )
    {
        std::cout << value;
    }

    void print(
        unsigned int value
    )
    {
        std::cout << value;
    }

    void print(
        long value
    )
    {
        std::cout << value;
    }

    void print(
        unsigned long value
    )
    {
        std::cout << value;
    }


    // --------------------------------------------------------
    // println()
    // --------------------------------------------------------

    void println()
    {
        std::cout << std::endl;
    }

    void println(
        const char *text
    )
    {
        std::cout << text
                  << std::endl;
    }

    void println(
        char c
    )
    {
        std::cout << c
                  << std::endl;
    }

    void println(
        int value
    )
    {
        std::cout << value
                  << std::endl;
    }

    void println(
        unsigned int value
    )
    {
        std::cout << value
                  << std::endl;
    }

    void println(
        long value
    )
    {
        std::cout << value
                  << std::endl;
    }

    void println(
        unsigned long value
    )
    {
        std::cout << value
                  << std::endl;
    }
};


// ============================================================
// Global Arduino Serial object
// ============================================================

extern FakeSerial Serial;


#endif
