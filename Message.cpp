//  Library for sending and receiving structured SIGFOX messages
#include <stdlib.h>
#include "SIGFOX.h"
#include "Message.h"

//  Encode each letter (lowercase only) in 5 bits:
//  0 = End of name/value or can't be encoded.
//  1 = a, 2 = b, ..., 26 = z,
//  27 = 0, 28 = 1, ..., 31 = 4
//  5 to 9 cannot be encoded.

static const uint8_t firstLetter = 1;
static const uint8_t firstDigit = 27;

static uint8_t encodeLetter(char ch) {
  //  Convert character ch to the 5-bit equivalent.
  //  Convert to lowercase.
  if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
  if (ch >= 'a' && ch <= 'z') return ch - 'a' + firstLetter;
  //  For 1, 2, ... return the digit
  if (ch >= '0' && ch <= '4') return (ch - '0') + 27;
  //  Can't encode.
  return 0;
}

static char decodeLetter(uint8_t code) {
  //  Convert the 5-bit code to a letter.
  if (code == 0) return 0;
  if (code >= firstLetter && code < firstDigit) return code - firstLetter + 'a';
  if (code >= firstDigit) return code - firstDigit + '0';
  return 0;
}

Message::Message(Radiocrafts &transceiver) {
  //  Construct a message for Radiocrafts.
  radiocrafts = &transceiver;
}

Message::Message(Akeru &transceiver) {
  //  Construct a message for Akeru.
  akeru = &transceiver;
}

bool Message::addField(const String name, int value) {
  //  Add an integer field scaled by 10.  2 bytes.
  int val = value * 10;
  return addIntField(name, val);
}

bool Message::addField(const String name, float value) {
  //  Add a float field with 1 decimal place.  2 bytes.
  int val = value * 10;
  return addIntField(name, val);
}

bool Message::addField(const String name, double value) {
  //  Add a double field with 1 decimal place.  2 bytes.
  int val = value * 10;
  return addIntField(name, val);
}

bool Message::addIntField(const String name, int value) {
  //  Add an int field that is already scaled.  2 bytes for name, 2 bytes for value.
  if (encodedMessage.length() + (4 * 2) > MAX_BYTES_PER_MESSAGE * 2) {
    String err = String("****ERROR: Message too long, already ") + (encodedMessage.length() / 2) +
      " bytes";
    radiocrafts ? radiocrafts->echo(err) : akeru->echo(err);
    return false;
  }
  addName(name);
  encodedMessage.concat(radiocrafts ?
    radiocrafts->toHex(value) :
    akeru->toHex(value));
  return true;
}

bool Message::addField(const String name, const String value) {
  //  Add a string field with max 3 chars.  2 bytes for name, 2 bytes for value.
  if (encodedMessage.length() + (4 * 2) > MAX_BYTES_PER_MESSAGE * 2) {
    String err = String("****ERROR: Message too long, already ") + (encodedMessage.length() / 2) +
      " bytes";
    radiocrafts ? radiocrafts->echo(err) : akeru->echo(err);
    return false;
  }
  addName(name);
  addName(value);
  return true;
}

bool Message::addName(const String name) {
  //  Add the encoded field name with 3 letters.
  //  1 header bit + 5 bits for each letter, total 16 bits.
  //  TODO: Assert name has 3 letters.
  //  TODO: Assert encodedMessage is less than 12 bytes.
  //  Convert 3 letters to 3 bytes.
  uint8_t buffer[] = {0, 0, 0};
  for (int i = 0; i <= 2 && i <= name.length(); i++) {
    //  5 bits for each letter.
    char ch = name.charAt(i);
    buffer[i] = encodeLetter(ch);
  }
  //  [x000] [0011] [1112] [2222]
  //  [x012] [3401] [2340] [1234]
  unsigned int result =
      (buffer[0] << 10) +
      (buffer[1] << 5) +
      (buffer[2]);
  encodedMessage.concat(radiocrafts ?
                        radiocrafts->toHex(result) :
                        akeru->toHex(result));
  return true;
}

bool Message::send() {
  //  Send the encoded message to SIGFOX.
  String msg = getEncodedMessage();
  if (msg.length() == 0) {
    String err = "****ERROR: Nothing to send";
    radiocrafts ? radiocrafts->echo(err) : akeru->echo(err);
    return false;
  }
  if (msg.length() >= MAX_BYTES_PER_MESSAGE * 2) {
    String err = String("****ERROR: Message too long, already ") + (encodedMessage.length() / 2) + " bytes";
    radiocrafts ? radiocrafts->echo(err) : akeru->echo(err);
    return false;
  }
  return radiocrafts ? radiocrafts->sendMessage(msg) :
      akeru->sendMessage(msg);
}

String Message::getEncodedMessage() {
  //  Return the encoded message to be transmitted.
  return encodedMessage;
}

static uint8_t hexDigitToDecimal(char ch) {
  //  Convert 0..9, a..f, A..F to decimal.
  if (ch >= '0' && ch <= '9') return (uint8_t) ch - '0';
  if (ch >= 'a' && ch <= 'z') return (uint8_t) ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'Z') return (uint8_t) ch - 'A' + 10;
  return 0;
}

String Message::decodeMessage(String msg) {
  //  Decode the encoded message.
  //  2 bytes name, 2 bytes float * 10, 2 bytes name, 2 bytes float * 10, ...
  String result = "{";
  for (int i = 0; i < msg.length(); i = i + 8) {
    String name = msg.substring(i, i + 4);
    String val = msg.substring(i + 4, i + 8);
    unsigned long name2 =
      (hexDigitToDecimal(name.charAt(2)) << 12) +
      (hexDigitToDecimal(name.charAt(3)) << 8) +
      (hexDigitToDecimal(name.charAt(0)) << 4) +
      hexDigitToDecimal(name.charAt(1));
    unsigned long val2 =
      (hexDigitToDecimal(val.charAt(2)) << 12) +
      (hexDigitToDecimal(val.charAt(3)) << 8) +
      (hexDigitToDecimal(val.charAt(0)) << 4) +
      hexDigitToDecimal(val.charAt(1));
    if (i > 0) result.concat(',');
    result.concat('"');
    //  Decode name.
    char name3[] = {0, 0, 0, 0};
    for (int j = 0; j < 3; j++) {
      uint8_t code = name2 & 31;
      char ch = decodeLetter(code);
      if (ch > 0) name3[2 - j] = ch;
      name2 = name2 >> 5;
    }
    name3[3] = 0;
    result.concat(name3);
    //  Decode value.
    result.concat("\":"); result.concat((int)(val2 / 10));
    result.concat("."); result.concat((int)(val2 % 10));
  }
  result.concat("}");
  return result;
}
