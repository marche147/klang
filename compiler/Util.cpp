#include <Util.h>

namespace klang {

static bool IsHexDigit(char C) {
  return (C >= '0' && C <= '9') || (C >= 'a' && C <= 'f') || (C >= 'A' && C <= 'F');
}

static std::optional<int> HexDigitToInt(char C) {
  if(C >= '0' && C <= '9') {
    return C - '0';
  } else if(C >= 'a' && C <= 'f') {
    return C - 'a' + 10;
  } else if(C >= 'A' && C <= 'F') {
    return C - 'A' + 10;
  }
  return std::nullopt;
}

std::optional<std::string> Unescape(const std::string& Str) {
  std::string Result;
  for(size_t I = 0; I < Str.size();) {
    if(Str[I] == '\\') {
      // unterminated escape sequence
      if(I + 1 >= Str.size()) {
        return std::nullopt;
      }

      char ControlChar = Str[I + 1];
      switch(ControlChar) {
        case 'n': { Result += '\n'; I += 2; break; }
        case 't': { Result += '\t'; I += 2; break; }
        case 'r': { Result += '\r'; I += 2; break; }
        case '0': { Result += '\0'; I += 2; break; }
        case '\\': { Result += '\\'; I += 2; break; }
        case '\'': { Result += '\''; I += 2; break; }
        case '\"': { Result += '\"'; I += 2; break; }

        case 'x': {
          if(I + 3 >= Str.size()) {
            return std::nullopt;
          }

          char Hi = Str[I + 2];
          char Low = Str[I + 3];
          auto HiInt = HexDigitToInt(Hi);
          auto LowInt = HexDigitToInt(Low);
          if(!HiInt || !LowInt) {
            return std::nullopt;
          }
          char Char = static_cast<char>((*HiInt << 4) | *LowInt);
          Result += Char;
          I += 4;
          continue;
        }
        default: {
          // invalid control character
          return std::nullopt;
        }
      }
    } else {
      Result += Str[I];
      I++;
    }
  }
  return Result;
}

std::string Escape(const std::string& Str) {
  std::string Result;
  for(size_t I = 0; I < Str.size(); I++) {
    switch(Str[I]) {
      case '\n': Result += "\\n"; break;
      case '\t': Result += "\\t"; break;
      case '\r': Result += "\\r"; break;
      case '\0': Result += "\\0"; break;
      case '\\': Result += "\\\\"; break;
      case '\'': Result += "\\'"; break;
      case '\"': Result += "\\\""; break;
      default: {
        if(Str[I] >= 32 && Str[I] <= 126) {
          Result += Str[I];
        } else {
          Result += "\\x";
          Result += "0123456789abcdef"[Str[I] >> 4];
          Result += "0123456789abcdef"[Str[I] & 0xf];
        }
      }
    }
  }
  return Result;
}

} // namespace klang