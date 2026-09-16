// LeanTX streaming configuration. GPL-2.0-or-later.
#include "config_stream.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

namespace config_stream {
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
struct Engine {
  Stream in, out;
  const Schema& schema;
  Workspace& w;
  Result result{};
  unsigned depth = 0;
  bool apply;
  bool ended = false;
  int blockIndent = -1;
  int suppressIndent = -1;

  bool fail(const char* error) { result.error = error; return false; }
  bool write(const char* s, size_t n) {
    return !out.write || out.write(out.context, s, n) || fail("configuration write failed");
  }
  bool write(const char* s) { return write(s, strlen(s)); }
  bool spaces(unsigned n) {
    while (n--) if (!write(" ", 1)) return false;
    return true;
  }
  bool seen(unsigned i) const { return w.seen[i / 8] & (1 << (i % 8)); }
  void mark(unsigned i) { w.seen[i / 8] |= 1 << (i % 8); }
  bool get(unsigned i, bool values = true) {
    memset(&w.field, 0, sizeof(w.field));
    w.field.required = true;
    auto describe = !values && schema.describe ? schema.describe : schema.field;
    bool found = describe(schema.context, i, w.field);
    if (!found && w.field.path[0]) fail("invalid runtime configuration value");
    return found;
  }
  // Emit missing descendants grouped by path. Recursion uses only path offsets;
  // the fixed path buffer and explicit depth bound limit temporary state.
  bool missing(unsigned indent, unsigned nesting = 0) {
    if (nesting >= 4) return fail("configuration nesting limit");
    size_t prefix = strlen(w.path);
    for (unsigned i = 0; i < schema.count; ++i) {
      if (seen(i) || !get(i, false) || !w.field.available || !w.field.required) continue;
      const char* tail = w.field.path;
      if (prefix) {
        if (strncmp(tail, w.path, prefix) || tail[prefix] != '/') continue;
        tail += prefix + 1;
      }
      result.missing = true;
      if (!out.write) continue;
      const char* slash = strchr(tail, '/');
      if (!spaces(indent)) return false;
      if (!slash) {
        if (!get(i)) return fail("invalid runtime configuration value");
        if (!write(tail) || !write(": ") || !write(w.field.value) || !write("\n")) return false;
        mark(i);
      } else {
        size_t len = slash - tail;
        if (!write(tail, len) || !write(":\n")) return false;
        if (prefix + (prefix ? 1 : 0) + len >= MaxPath) return fail("configuration path limit");
        size_t p = prefix;
        if (p) w.path[p++] = '/';
        memcpy(w.path + p, tail, len);
        w.path[p + len] = 0;
        if (!missing(indent + 2, nesting + 1)) return false;
        w.path[prefix] = 0;
      }
    }
    return true;
  }
  bool pop() {
    auto level = w.levels[depth - 1];
    if (suppressIndent < 0 && level.kind != 2 && !missing(level.indent)) return false;
    --depth;
    w.path[depth ? w.levels[depth - 1].pathLength : 0] = 0;
    return true;
  }
  bool flowToken(const char*& p, unsigned depth, bool key = false) {
    while (*p == ' ') ++p;
    if (depth >= MaxDepth) return fail("configuration flow nesting limit");
    if (*p == '[' || *p == '{') {
      if (key) return fail("complex YAML keys are unsupported");
      bool map = *p++ == '{'; char end = map ? '}' : ']';
      while (*p == ' ') ++p;
      if (*p == end) { ++p; return true; }
      for (;;) {
        if (map) {
          if (!flowToken(p, depth + 1, true)) return false;
          while (*p == ' ') ++p;
          if (*p++ != ':') return fail("expected flow mapping colon");
          while (*p == ' ') ++p;
        }
        if (!map || (*p != ',' && *p != end))
          if (!flowToken(p, depth + 1)) return false;
        while (*p == ' ') ++p;
        if (*p == end) { ++p; return true; }
        if (*p++ != ',') return fail("expected flow collection separator");
        while (*p == ' ') ++p;
        if (*p == end) { ++p; return true; }
      }
    }
    if (*p == '\'' || *p == '"') {
      char q = *p++;
      while (*p) {
        if (*p == q) {
          ++p;
          if (q == '\'' && *p == q) { ++p; continue; }
          return true;
        }
        if (*p++ == '\\' && q == '"') {
          if (!*p) return fail("unterminated flow escape");
          ++p;
        }
      }
      return fail("unterminated flow string");
    }
    const char* start = p;
    while (*p && !strchr(",[]{}", *p) && !(key && *p == ':')) {
      if (!key && *p == ':' && (p[1] == ' ' || !p[1])) return fail("invalid flow scalar colon");
      ++p;
    }
    const char* end = p;
    while (end > start && end[-1] == ' ') --end;
    return end > start || fail("empty flow value");
  }
  // Validate a single-line scalar/flow collection without retaining its values.
  // YAML aliases/tags/directives and multiline flow are deliberately rejected:
  // they cannot be safely merged without retaining document-wide state.
  bool value(char* s, bool& block) {
    char brackets[MaxDepth]; unsigned n = 0; char quote = 0;
    block = false;
    if (*s == '|' || *s == '>') {
      const char* p = s + 1;
      if (*p == '+' || *p == '-') ++p;
      if (*p >= '1' && *p <= '9') ++p;
      while (*p == ' ') ++p;
      if (*p && *p != '#') return fail("invalid block scalar");
      block = true; return true;
    }
    if (*s != '[' && *s != '{' && *s != '\'' && *s != '"') {
      if (*s && strchr("!&*", *s)) return fail("YAML anchors and tags are unsupported");
      if (*s && (strchr("]},%@`", *s) || (strchr("-?:", *s) && (!s[1] || s[1] == ' '))))
        return fail("invalid plain scalar indicator");
      for (char* p = s; *p; ++p) {
        if (*p == '#' && (p == s || p[-1] == ' ' || p[-1] == '\t')) { *p = 0; break; }
        if (*p == ':' && (!p[1] || p[1] == ' ' || p[1] == '\t')) return fail("invalid scalar colon");
      }
      size_t len = strlen(s);
      while (len && (s[len - 1] == ' ' || s[len - 1] == '\t')) s[--len] = 0;
      return true;
    }
    for (char* p = s; *p; ++p) {
      char c = *p;
      if (quote) {
        if (quote == '"' && c == '\\') {
          const char* next = p + 1; uint32_t cp;
          if (!escape(next, cp)) return fail("invalid YAML escape");
          p = const_cast<char*>(next) - 1;
        }
        else if (c == quote) {
          if (quote == '\'' && p[1] == '\'') ++p;
          else quote = 0;
        }
      } else if (c == '#' && (p == s || p[-1] == ' ')) { *p = 0; break; }
      else if ((c == '\'' || c == '"') && (p == s || strchr(" [{,:", p[-1]))) quote = c;
      else if ((c == '&' || c == '*' || c == '!') && (p == s || strchr(" [{,: ", p[-1]))) return fail("YAML anchors and tags are unsupported");
      else if (c == '[' || c == '{') {
        if (n == MaxDepth) return fail("configuration flow nesting limit");
        brackets[n++] = c;
      } else if (c == ']' || c == '}') {
        if (!n || brackets[--n] != (c == ']' ? '[' : '{')) return fail("unbalanced YAML collection");
      } else if (!n && c == ':' && (!p[1] || p[1] == ' ')) return fail("invalid scalar colon");
    }
    if (quote || n) return fail("unterminated YAML value");
    if (*s == '[' || *s == '{') {
      const char* p = s;
      if (!flowToken(p, 0)) return false;
      while (*p == ' ') ++p;
      if (*p) return fail("trailing flow content");
    }
    size_t len = strlen(s);
    while (len && s[len - 1] == ' ') s[--len] = 0;
    size_t decoded;
    if ((*s == '\'' || *s == '"') && !string(s, nullptr, size_t(-1), decoded))
      return fail("invalid quoted scalar");
    return true;
  }
  bool line(size_t rawLen) {
    unsigned indent = 0;
    while (w.line[indent] == ' ') ++indent;
    char* text = w.line + indent;
    if (!*text || *text == '#') return write(w.line, rawLen) && write("\n");
    if (blockIndent >= 0 && indent > (unsigned)blockIndent)
      return suppressIndent >= 0 || (write(w.line, rawLen) && write("\n"));
    blockIndent = -1;
    if (*text == '\t') return fail("tabs in YAML indentation");
    if (!strcmp(text, "---")) {
      if (depth || ended) return fail("multiple YAML documents");
      ended = true; // only permit one document start
      return write(w.line, rawLen) && write("\n");
    }
    if (*text == '%' || !strcmp(text, "...")) return fail("unsupported YAML document marker");
    if (suppressIndent >= 0 && indent <= (unsigned)suppressIndent) suppressIndent = -1;
    const bool startsList = *text == '-' && (!text[1] || text[1] == ' ');
    while (depth) {
      const auto& top = w.levels[depth - 1];
      bool endIndentlessList = depth > 1 && top.kind == 2 &&
          top.indent == w.levels[depth - 2].indent && indent == top.indent && !startsList;
      if (indent >= top.indent && !endIndentlessList) break;
      if (!pop()) return false;
    }
    if (!depth) {
      if (indent) return fail("indented YAML root");
      w.levels[depth++] = {0, 0, 0};
    }
    if (indent != w.levels[depth - 1].indent) return fail("unexpected YAML indentation");
    bool list = startsList;
    auto& level = w.levels[depth - 1];
    unsigned kind = list ? 2 : 1;
    if (level.kind && level.kind != kind) return fail("mixed YAML map and list");
    level.kind = kind;
    size_t prefix = strlen(w.path);
    if (list) {
      ++text; while (*text == ' ') ++text;
      // Unknown list items are opaque to the schema but still structurally checked.
      if (prefix + 2 >= MaxPath) return fail("configuration path limit");
      strcat(w.path, "/@");
    }
    char* colon = nullptr;
    char quote = 0;
    for (char* p = text; *p; ++p) {
      if (quote) {
        if (quote == '"' && *p == '\\') { if (!*++p) break; }
        else if (*p == quote) {
          if (quote == '\'' && p[1] == '\'') ++p;
          else quote = 0;
        }
      }
      else if (*p == '\'' || *p == '"') quote = *p;
      else if (*p == ':' && (!p[1] || p[1] == ' ')) { colon = p; break; }
      else if (*p == '[' || *p == '{') break;
    }
    if (!colon && !list) return fail("expected YAML mapping");
    char* scalar = text;
    if (colon && list) {
      if (depth == MaxDepth) return fail("configuration nesting limit");
      w.levels[depth++] = {uint16_t(indent + 2), uint16_t(strlen(w.path)), 1};
      prefix = strlen(w.path);
    }
    if (colon) {
      size_t keyLen = colon - text;
      while (keyLen && text[keyLen - 1] == ' ') --keyLen;
      if (!keyLen) return fail("empty YAML key");
      size_t p = strlen(w.path);
      if (p + 2 >= MaxPath) return fail("configuration path limit");
      if (p) w.path[p++] = '/';
      char saved = text[keyLen]; text[keyLen] = 0;
      size_t decodedLength = 0;
      bool validKey = string(text, w.path + p, MaxPath - p - 1, decodedLength);
      text[keyLen] = saved;
      if (!validKey || !decodedLength) return fail("invalid YAML key or path limit");
      // '/' in unknown keys cannot impersonate a schema path.
      for (size_t j = 0; j < decodedLength; ++j)
        if (w.path[p + j] == '/') w.path[p + j] = '\x01';
      w.path[p + decodedLength] = 0;
      scalar = colon + 1; while (*scalar == ' ') ++scalar;
    }
    // Preserve the source before stripping comments for typed parsing.
    int field = -1;
    if (!list) {
      if (schema.resolve) {
        int id = schema.resolve(schema.context, w.path);
        if (id >= 0 && unsigned(id) < schema.count && get(id, out.write != nullptr)) field = id;
      } else {
        for (unsigned i = 0; i < schema.count; ++i)
          if (get(i) && !strcmp(w.field.path, w.path)) { field = i; break; }
      }
    }
    bool replace = field >= 0 && w.field.available;
    if (field >= 0) {
      if (seen(field)) return fail("duplicate known configuration field");
      mark(field);
    } else if (colon && !schema.known(schema.context, w.path)) ++result.unknown;
    if (suppressIndent < 0) {
      if (replace && out.write) {
        if (!spaces(indent) || !write(w.line + indent, colon - (w.line + indent)) || !write(": ") || !write(w.field.value) || !write("\n")) return false;
      } else if (!write(w.line, rawLen) || !write("\n")) return false;
    }
    bool block;
    if (!value(scalar, block)) return false;
    if (field < 0 && *scalar && !list && schema.known(schema.context, w.path)) {
      size_t plen = strlen(w.path);
      for (unsigned i = 0; i < schema.count; ++i) {
        if (get(i, false) && w.field.available && !strncmp(w.field.path, w.path, plen) && w.field.path[plen] == '/')
          return fail("expected configuration mapping");
      }
    }
    if (replace && apply && !schema.set(schema.context, field, scalar)) ++result.invalid;
    if (block) blockIndent = indent;
    if (!*scalar || (!colon && list && !*text)) {
      if (depth == MaxDepth) return fail("configuration nesting limit");
      // Child indentation is learned from the next nonempty line.
      w.levels[depth++] = {uint16_t(indent + (list && colon ? 3 : 1)), uint16_t(strlen(w.path)), 0};
      if (replace && out.write) suppressIndent = indent;
    } else {
      if (replace && block && out.write) suppressIndent = indent;
      w.path[prefix] = 0;
    }
    return true;
  }
  Result run() {
    memset(&w, 0, sizeof(w));
    if (schema.count > MaxFields) { fail("configuration schema capacity"); return result; }
    for (;;) {
      size_t n = 0; int c = -1;
      while (in.read && (c = in.read(in.context)) >= 0 && c != '\n') {
        if (c == '\r') continue;
        if (!c || (c < 32 && c != '\t')) { fail("invalid YAML byte"); return result; }
        if (n + 1 == MaxLine) { fail("configuration line limit"); return result; }
        w.line[n++] = c;
      }
      if (c == -2) { fail("configuration read failed"); return result; }
      if (!n && c == -1) break;
      w.line[n] = 0;
      unsigned indent = 0; while (w.line[indent] == ' ') ++indent;
      if (depth && !w.levels[depth - 1].kind && * (w.line + indent) && w.line[indent] != '#') {
        auto& pending = w.levels[depth - 1];
        bool indentlessList = depth > 1 && indent == w.levels[depth - 2].indent &&
            w.line[indent] == '-' && (!w.line[indent + 1] || w.line[indent + 1] == ' ');
        if (indent >= pending.indent || indentlessList) pending.indent = indent;
      }
      if (!line(n)) return result;
      if (c == -1) break;
    }
    while (depth) if (!pop()) return result;
    w.path[0] = 0;
    missing(0);
    return result;
  }
};
}
Result process(Stream in, Stream out, const Schema& schema, Workspace& w, bool apply)
{
  Engine engine{in, out, schema, w, {}, 0, apply};
  return engine.run();
}
}
