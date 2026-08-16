#ifndef ENCODING_H
#define ENCODING_H

#include <Arduino.h>

String ucs2ToUtf8(const String &hex);
bool isUCS2(const String &s);

#endif
