#include "Encoding.h"

String ucs2ToUtf8(const String &hex)
{
    String out;

    for (int i = 0; i + 3 < hex.length(); i += 4)
    {
        uint16_t c =
            strtol(
                hex.substring(i, i + 4).c_str(),
                nullptr,
                16
            );

        if (c < 0x80)
        {
            out += char(c);
        }
        else if (c < 0x800)
        {
            out += char(0xC0 | (c >> 6));
            out += char(0x80 | (c & 0x3F));
        }
        else
        {
            out += char(0xE0 | (c >> 12));
            out += char(0x80 | ((c >> 6) & 0x3F));
            out += char(0x80 | (c & 0x3F));
        }
    }

    return out;
}


bool isUCS2(const String &s)
{
    if (s.length() == 0 || s.length() % 4 != 0)
        return false;

    for (int i = 0; i < s.length(); i++)
    {
        if (!isxdigit((unsigned char)s[i]))
            return false;
    }

    return true;
}
