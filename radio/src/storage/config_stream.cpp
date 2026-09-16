// LeanTX streaming configuration. GPL-2.0-or-later.
#include "config_stream.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

namespace config_stream {
char scalarBuffer[MaxValue];
bool formatInteger(char* output, size_t capacity, int64_t value)
{
  // Unsigned subtraction also handles INT64_MIN without signed overflow.
  uint64_t magnitude = value < 0 ? uint64_t(0) - uint64_t(value) : uint64_t(value);
  char digits[20];
  size_t count = 0;
  do {
    digits[count++] = char('0' + magnitude % 10);
    magnitude /= 10;
  } while (magnitude);
  size_t length = count + (value < 0 ? 1 : 0);
  if (capacity <= length) {
    if (capacity) output[0] = 0;
    return false;
  }
  size_t pos = 0;
  if (value < 0) output[pos++] = '-';
  while (count) output[pos++] = digits[--count];
  output[pos] = 0;
  return true;
}

bool integer(const char* text, int64_t lo, int64_t hi, int64_t& value)
{
  if (!strcmp(text, "true")) text = "1";
  if (!strcmp(text, "false")) text = "0";
  if (!*text || (*text != '-' && !isdigit((unsigned char)*text))) return false;
  errno = 0;
  char* end;
  long long n = strtoll(text, &end, 10);
  if (errno || *end || n < lo || n > hi) return false;
  value = n;
  return true;
}
namespace {
bool escape(const char*& p, uint32_t& code) {
  char c = *p;
  if (!c) return false;
  ++p;
  switch (c) {
    case '0': code = 0; return true;
    case 'a': code = 7; return true;
    case 'b': code = 8; return true;
    case 't': code = 9; return true;
    case 'n': code = 10; return true;
    case 'v': code = 11; return true;
    case 'f': code = 12; return true;
    case 'r': code = 13; return true;
    case 'e': code = 27; return true;
    case 'N': code = 0x85; return true;
    case '_': code = 0xa0; return true;
    case 'L': code = 0x2028; return true;
    case 'P': code = 0x2029; return true;
    case ' ': case '/': case '"': case '\\': code = c; return true;
  }
  unsigned count = c == 'x' ? 2 : c == 'u' ? 4 : c == 'U' ? 8 : 0;
  if (!count) return false;
  code = 0;
  while (count--) {
    c = *p;
    if (!c || !isxdigit((unsigned char)c)) return false;
    ++p;
    code = code * 16 + (c >= '0' && c <= '9' ? c - '0' : tolower((unsigned char)c) - 'a' + 10);
  }
  return code <= 0x10ffff && !(code >= 0xd800 && code <= 0xdfff);
}
}
bool string(const char* in, char* out, size_t cap, size_t& length)
{
  length = 0;
  char quote = (*in == '\'' || *in == '"') ? *in++ : 0;
  auto add = [&](unsigned char c) {
    if ((!c && out) || length == cap) return false;
    if (out) out[length] = c;
    ++length; return true;
  };
  while (*in) {
    unsigned char c = *in++;
    if (quote && c == quote) {
      if (quote == '\'' && *in == '\'') ++in;
      else return !*in;
    } else if (quote == '"' && c == '\\') {
      uint32_t cp;
      if (!escape(in, cp)) return false;
      if (cp < 0x80) { if (!add(cp)) return false; }
      else if (cp < 0x800) { if (!add(0xc0 | (cp >> 6)) || !add(0x80 | (cp & 63))) return false; }
      else if (cp < 0x10000) {
        if (!add(0xe0 | (cp >> 12)) || !add(0x80 | ((cp >> 6) & 63)) || !add(0x80 | (cp & 63))) return false;
      } else if (!add(0xf0 | (cp >> 18)) || !add(0x80 | ((cp >> 12) & 63)) ||
                 !add(0x80 | ((cp >> 6) & 63)) || !add(0x80 | (cp & 63))) return false;
      continue;
    }
    if (!add(c)) return false;
  }
  return !quote;
}

namespace {
bool scalar(char* s) {
  char quote = 0; unsigned brackets = 0;
  if (*s && strchr("!&*|>%@`", *s)) return false;
  for (char* p = s; *p; ++p) {
    if (quote) {
      if (quote == '"' && *p == '\\') {
        const char* next = p + 1; uint32_t cp;
        if (!escape(next, cp)) return false;
        p = const_cast<char*>(next) - 1;
      } else if (*p == quote) {
        if (quote == '\'' && p[1] == '\'') ++p;
        else quote = 0;
      }
    } else if (*p == '#' && (p == s || p[-1] == ' ')) { *p = 0; break; }
    else if ((*p == '"' || *p == '\'') && (p == s || (brackets && (p[-1] == ' ' || p[-1] == '[')))) quote = *p;
    else if (*p == '[') { if (brackets++) return false; }
    else if (*p == ']') { if (!brackets--) return false; }
    else if (*p == '{') { if (p[1] != '}') return false; ++p; }
    else if (*p == ':' && (!p[1] || p[1] == ' ')) return false;
  }
  size_t n = strlen(s);
  while (n && (s[n-1] == ' ' || s[n-1] == '\t')) s[--n] = 0;
  size_t decoded;
  return !quote && !brackets && ((*s != '"' && *s != '\'') || string(s, nullptr, size_t(-1), decoded));
}
}
Result parse(char* data, size_t size, const Document& schema, Workspace& w)
{
  Result result{};
  unsigned depth = 1;
  w.levels[0] = {};
  w.levels[0].node.section = 0;
  if (schema.seen) memset(schema.seen, 0, schema.seenBytes);
  bool documentStart = false, content = false;
  auto fail = [&](const char* error) { result.error = error; return result; };
  char* cursor = data;
  char* end = data + size;
  while (cursor < end) {
    char* line = cursor;
    while (cursor < end && *cursor != '\n') {
      unsigned char c = *cursor++;
      if (!c || (c < 32 && c != '\r' && c != '\t')) return fail("invalid YAML byte");
    }
    if (cursor < end) *cursor++ = 0;
    else *end = 0; // caller provides the terminator byte
    size_t length = strlen(line);
    if (length && line[length-1] == '\r') line[--length] = 0;
    unsigned indent = 0; while (line[indent] == ' ') ++indent;
    char* text = line + indent;
    if (!*text || *text == '#') continue;
    if (*text == '\t') return fail("tabs in YAML indentation");
    if (!strcmp(text, "---")) {
      if (indent || documentStart || content) return fail("multiple YAML documents");
      documentStart = true; continue;
    }
    content = true;
    bool list = *text == '-' && (!text[1] || text[1] == ' ');
    while (depth > 1) {
      auto& top = w.levels[depth-1];
      if (top.pending && (indent > w.levels[depth-2].indent ||
          (list && indent == w.levels[depth-2].indent))) {
        top.indent = indent; top.pending = false;
      }
      bool indentlessEnd = top.kind == 2 && top.indent == w.levels[depth-2].indent && !list && indent == top.indent;
      if (!top.pending && indent >= top.indent && !indentlessEnd) break;
      --depth;
    }
    auto& level = w.levels[depth-1];
    if (indent != level.indent) return fail("unexpected YAML indentation");
    unsigned kind = list ? 2 : 1;
    if (level.kind && level.kind != kind) return fail("mixed YAML map and list");
    level.kind = kind;
    Node parent = level.node;
    if (list) {
      ++text; while (*text == ' ') ++text;
      char key[12]; formatInteger(key, sizeof(key), level.nextIndex++);
      Node item = parent; item.section = -1;
      schema.enter(schema.context, parent, key, "", item, result);
      parent = item;
    }
    char quote = 0; char* colon = nullptr;
    for (char* p = text; *p; ++p) {
      if (quote) {
        if (*p == '\\' && quote == '"' && p[1]) ++p;
        else if (*p == quote) { if (quote == '\'' && p[1] == '\'') ++p; else quote = 0; }
      } else if (*p == '\'' || *p == '"') quote = *p;
      else if (*p == ':' && (!p[1] || p[1] == ' ')) { colon = p; break; }
    }
    if (!colon && !list) return fail("expected YAML mapping");
    Node child = parent; child.section = -1;
    char* value = text;
    char* key = const_cast<char*>("val");
    if (colon) {
      *colon = 0;
      size_t n = strlen(text); while (n && text[n-1] == ' ') text[--n] = 0;
      size_t decoded;
      if (!n || !string(text, text, n, decoded) || !decoded) return fail("invalid YAML key");
      text[decoded] = 0; key = text;
      value = colon + 1; while (*value == ' ') ++value;
    }
    if (!scalar(value)) return fail("unsupported or malformed YAML scalar");
    bool empty = !*value || !strcmp(value, "[]") || !strcmp(value, "{}");
    if (colon || !empty) schema.enter(schema.context, parent, key, empty ? "" : value, child, result);
    else child = parent;
    if (result.error) return result;
    if (list && colon) {
      if (depth == MaxDepth) return fail("configuration nesting limit");
      w.levels[depth++] = {parent, uint16_t(indent + 2), 0, 1, false};
    }
    if (empty && strcmp(value, "[]") && strcmp(value, "{}")) {
      if (depth == MaxDepth) return fail("configuration nesting limit");
      w.levels[depth++] = {child, uint16_t(indent + 1), 0, 0, true};
    }
  }
  return result;
}
bool mark(uint8_t* seen, unsigned id, unsigned bytes, Result& result)
{
  if (id / 8 >= bytes) { result.error = "configuration field limit"; return false; }
  unsigned mask = 1u << (id % 8);
  if (seen[id/8] & mask) { result.error = "duplicate configuration field"; return false; }
  seen[id/8] |= mask; return true;
}
bool Writer::write(const char* s, size_t n) {
  if (!error && (!output.write || !output.write(output.context, s, n))) {
    if (!error) error = "configuration write failed";
  }
  return !error;
}
void Writer::begin(const char* key) {
  if (depth >= MaxDepth || strlen(key) >= sizeof(levels[0].key)) { error = "configuration nesting/key limit"; return; }
  auto& level = levels[depth++]; strcpy(level.key, key); level.emitted = false;
}
void Writer::begin(unsigned index) { char key[12]; formatInteger(key, sizeof(key), index); begin(key); }
void Writer::end() { if (depth) --depth; }
void Writer::value(const char* key, const char* value) {
  auto spaces = [&](unsigned n) { while (n--) write(" ", 1); };
  for (unsigned i = 0; i < depth; ++i) if (!levels[i].emitted) {
    spaces(i * 2); write(levels[i].key, strlen(levels[i].key)); write(":\n", 2); levels[i].emitted = true;
  }
  spaces(depth * 2); write(key, strlen(key)); write(": ", 2); write(value, strlen(value)); write("\n", 1);
}
#if defined(SIMU)
// Compatibility bridge for host tests; firmware always reads/serializes RAM.
Result process(Stream input, Stream output, const Document& schema, Workspace& w, bool apply)
{
  if (!apply) {
    Writer writer{output};
    if (schema.save) schema.save(schema.context, writer, w.field);
    return {writer.error, 0, 0, false};
  }
  size_t size = 0; int c;
  while (input.read && (c = input.read(input.context)) >= 0) {
    if (size == DocumentCapacity) return {"configuration file too large", 0, 0, false};
    w.document[size++] = char(c);
  }
  if (input.read && c == -2) return {"configuration read failed", 0, 0, false};
  w.document[size] = 0;
  return parse(w.document, size, schema, w);
}
#endif
} // namespace config_stream
