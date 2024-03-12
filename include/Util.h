#ifndef _UTIL_H
#define _UTIL_H

#include <iostream>
#include <string>
#include <optional>

namespace klang {

class IndentPrinter {
public:
  IndentPrinter(std::ostream& OS) : OS_(OS), IndentLevel_(0) {}

  void Indent() { IndentLevel_++; }
  void Unindent() { IndentLevel_--; }
  void Print(const char* Str) { OS_ << std::string(IndentLevel_ * 2, ' ') << Str; }
  void Print(const std::string& Str) { OS_ << std::string(IndentLevel_ * 2, ' ') << Str; }
  void PrintLine(const char* Str) { OS_ << std::string(IndentLevel_ * 2, ' ') << Str << std::endl; }
  void PrintLine(const std::string& Str) { OS_ << std::string(IndentLevel_ * 2, ' ') << Str << std::endl; }

  void DoIndent() { OS_ << std::string(IndentLevel_ * 2, ' '); }

  template<typename T>
  IndentPrinter& operator<<(const T& Val) {
    OS_ << Val;
    return *this;
  }

private:
  int IndentLevel_;
  std::ostream& OS_;
};

std::optional<std::string> Unescape(const std::string& Str);
std::string Escape(const std::string& Str);

} // namespace klang

#endif 