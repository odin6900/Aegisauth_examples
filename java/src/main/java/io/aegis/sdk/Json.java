package io.aegis.sdk;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Minimal dependency-free JSON reader/writer used by the SDK. */
final class Json {
    private final String source;
    private int index;

    private Json(String source) {
        this.source = source;
    }

    static Object parse(String text) {
        Json parser = new Json(text == null || text.isBlank() ? "{}" : text);
        parser.skipWhitespace();
        Object value = parser.readValue();
        return value;
    }

    @SuppressWarnings("unchecked")
    static Map<String, Object> parseObject(String text) {
        Object value = parse(text);
        return value instanceof Map ? (Map<String, Object>) value : new LinkedHashMap<>();
    }

    static String write(Object value) {
        StringBuilder out = new StringBuilder();
        writeValue(value, out);
        return out.toString();
    }

    private static void writeValue(Object value, StringBuilder out) {
        if (value == null) {
            out.append("null");
        } else if (value instanceof String string) {
            writeString(string, out);
        } else if (value instanceof Number || value instanceof Boolean) {
            out.append(value);
        } else if (value instanceof Map<?, ?> map) {
            out.append('{');
            boolean first = true;
            for (Map.Entry<?, ?> entry : map.entrySet()) {
                if (entry.getValue() == null) continue;
                if (!first) out.append(',');
                first = false;
                writeString(String.valueOf(entry.getKey()), out);
                out.append(':');
                writeValue(entry.getValue(), out);
            }
            out.append('}');
        } else if (value instanceof Iterable<?> items) {
            out.append('[');
            boolean first = true;
            for (Object item : items) {
                if (!first) out.append(',');
                first = false;
                writeValue(item, out);
            }
            out.append(']');
        } else {
            writeString(String.valueOf(value), out);
        }
    }

    private static void writeString(String value, StringBuilder out) {
        out.append('"');
        for (int i = 0; i < value.length(); i++) {
            char c = value.charAt(i);
            switch (c) {
                case '"' -> out.append("\\\"");
                case '\\' -> out.append("\\\\");
                case '\n' -> out.append("\\n");
                case '\r' -> out.append("\\r");
                case '\t' -> out.append("\\t");
                default -> {
                    if (c < 0x20) {
                        out.append(String.format("\\u%04x", (int) c));
                    } else {
                        out.append(c);
                    }
                }
            }
        }
        out.append('"');
    }

    private Object readValue() {
        skipWhitespace();
        if (index >= source.length()) return null;
        char c = source.charAt(index);
        return switch (c) {
            case '{' -> readObject();
            case '[' -> readArray();
            case '"' -> readString();
            case 't' -> readLiteral("true", Boolean.TRUE);
            case 'f' -> readLiteral("false", Boolean.FALSE);
            case 'n' -> readLiteral("null", null);
            default -> readNumber();
        };
    }

    private Map<String, Object> readObject() {
        Map<String, Object> map = new LinkedHashMap<>();
        index++; // {
        skipWhitespace();
        if (peek() == '}') { index++; return map; }
        while (index < source.length()) {
            skipWhitespace();
            String key = readString();
            skipWhitespace();
            index++; // :
            map.put(key, readValue());
            skipWhitespace();
            char c = peek();
            index++;
            if (c == '}') break;
        }
        return map;
    }

    private List<Object> readArray() {
        List<Object> list = new ArrayList<>();
        index++; // [
        skipWhitespace();
        if (peek() == ']') { index++; return list; }
        while (index < source.length()) {
            list.add(readValue());
            skipWhitespace();
            char c = peek();
            index++;
            if (c == ']') break;
        }
        return list;
    }

    private String readString() {
        StringBuilder out = new StringBuilder();
        index++; // opening quote
        while (index < source.length()) {
            char c = source.charAt(index++);
            if (c == '"') break;
            if (c == '\\') {
                char escaped = source.charAt(index++);
                switch (escaped) {
                    case 'n' -> out.append('\n');
                    case 'r' -> out.append('\r');
                    case 't' -> out.append('\t');
                    case 'b' -> out.append('\b');
                    case 'f' -> out.append('\f');
                    case 'u' -> {
                        out.append((char) Integer.parseInt(source.substring(index, index + 4), 16));
                        index += 4;
                    }
                    default -> out.append(escaped);
                }
            } else {
                out.append(c);
            }
        }
        return out.toString();
    }

    private Object readLiteral(String literal, Object value) {
        index += literal.length();
        return value;
    }

    private Object readNumber() {
        int start = index;
        while (index < source.length() && "-+.eE0123456789".indexOf(source.charAt(index)) >= 0) index++;
        String raw = source.substring(start, index);
        if (raw.contains(".") || raw.contains("e") || raw.contains("E")) return Double.parseDouble(raw);
        return Long.parseLong(raw);
    }

    private char peek() {
        return index < source.length() ? source.charAt(index) : '\0';
    }

    private void skipWhitespace() {
        while (index < source.length() && Character.isWhitespace(source.charAt(index))) index++;
    }
}