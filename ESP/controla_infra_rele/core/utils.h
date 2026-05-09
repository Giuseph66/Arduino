#pragma once

String jsonEscape(const String& input) {
  String out;
  out.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == '"' || c == '\\') out += '\\';
    if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else out += c;
  }
  return out;
}

String uint64ToHex(uint64_t value) {
  if (value == 0) return "";
  char buffer[19];
  snprintf(buffer, sizeof(buffer), "0x%llX", (unsigned long long)value);
  return String(buffer);
}

String getBodyValue(const String& key) {
  if (server.hasArg(key)) return server.arg(key);
  String body = server.arg("plain");
  String needle = "\"" + key + "\"";
  int keyPos = body.indexOf(needle);
  if (keyPos < 0) return "";
  int colon = body.indexOf(':', keyPos + needle.length());
  int firstQuote = body.indexOf('"', colon + 1);
  int secondQuote = body.indexOf('"', firstQuote + 1);
  if (colon < 0 || firstQuote < 0 || secondQuote < 0) return "";
  return body.substring(firstQuote + 1, secondQuote);
}

