package io.aegis.sdk

/**
 * Minimal JSON encoder/decoder so the SDK stays dependency-free.
 * Decodes into Map/List/String/Double/Boolean/null.
 */
internal object Json {

    fun encode(value: Any?): String = when (value) {
        null -> "null"
        is String -> encodeString(value)
        is Boolean -> value.toString()
        is Number -> value.toString()
        is Map<*, *> -> value.entries.filter { it.value != null }
            .joinToString(",", "{", "}") { "${encodeString(it.key.toString())}:${encode(it.value)}" }
        is Iterable<*> -> value.joinToString(",", "[", "]") { encode(it) }
        else -> encodeString(value.toString())
    }

    fun decode(text: String): Any? {
        if (text.isBlank()) return emptyMap<String, Any?>()
        val cursor = Cursor(text)
        return cursor.readValue()
    }

    private fun encodeString(value: String): String {
        val sb = StringBuilder("\"")
        for (c in value) {
            when (c) {
                '"' -> sb.append("\\\"")
                '\\' -> sb.append("\\\\")
                '\n' -> sb.append("\\n")
                '\r' -> sb.append("\\r")
                '\t' -> sb.append("\\t")
                else -> if (c < ' ') sb.append("\\u%04x".format(c.code)) else sb.append(c)
            }
        }
        return sb.append('"').toString()
    }

    private class Cursor(val text: String) {
        var i = 0

        fun readValue(): Any? {
            skip()
            return when {
                i >= text.length -> null
                text[i] == '{' -> readObject()
                text[i] == '[' -> readArray()
                text[i] == '"' -> readString()
                text.startsWith("true", i) -> { i += 4; true }
                text.startsWith("false", i) -> { i += 5; false }
                text.startsWith("null", i) -> { i += 4; null }
                else -> readNumber()
            }
        }

        fun skip() {
            while (i < text.length && text[i].isWhitespace()) i++
        }

        fun readObject(): Map<String, Any?> {
            val out = LinkedHashMap<String, Any?>()
            i++
            skip()
            if (i < text.length && text[i] == '}') { i++; return out }
            while (i < text.length) {
                skip()
                val key = readString()
                skip()
                if (i < text.length && text[i] == ':') i++
                out[key] = readValue()
                skip()
                when {
                    i < text.length && text[i] == ',' -> i++
                    i < text.length && text[i] == '}' -> { i++; break }
                    else -> break
                }
            }
            return out
        }

        fun readArray(): List<Any?> {
            val out = ArrayList<Any?>()
            i++
            skip()
            if (i < text.length && text[i] == ']') { i++; return out }
            while (i < text.length) {
                out.add(readValue())
                skip()
                when {
                    i < text.length && text[i] == ',' -> i++
                    i < text.length && text[i] == ']' -> { i++; break }
                    else -> break
                }
            }
            return out
        }

        fun readString(): String {
            if (text[i] != '"') return ""
            i++
            val sb = StringBuilder()
            while (i < text.length && text[i] != '"') {
                if (text[i] == '\\' && i + 1 < text.length) {
                    i++
                    when (val esc = text[i]) {
                        'n' -> sb.append('\n')
                        't' -> sb.append('\t')
                        'r' -> sb.append('\r')
                        'b' -> sb.append('\b')
                        'u' -> { sb.append(text.substring(i + 1, i + 5).toInt(16).toChar()); i += 4 }
                        else -> sb.append(esc)
                    }
                } else {
                    sb.append(text[i])
                }
                i++
            }
            i++
            return sb.toString()
        }

        fun readNumber(): Double {
            val start = i
            while (i < text.length && (text[i].isDigit() || text[i] in "-+.eE")) i++
            return text.substring(start, i).toDoubleOrNull() ?: 0.0
        }
    }
}

/** Convenience accessors for decoded JSON maps. */
@Suppress("UNCHECKED_CAST")
fun Map<String, Any?>.obj(key: String): Map<String, Any?> = this[key] as? Map<String, Any?> ?: emptyMap()
fun Map<String, Any?>.str(key: String, fallback: String = ""): String = this[key] as? String ?: fallback
fun Map<String, Any?>.bool(key: String, fallback: Boolean = false): Boolean = this[key] as? Boolean ?: fallback