# Parsecpp

Для haskell, существует библиотека `parsec`, которая предоставляет DSL для описания парсеров внутри языка.
В этом задании мы напишем аналог `parsec` для c++ и назовем его `parsecpp`

Определим парсер как тип, у которого перегружен operator():
operator() будет возвращать `ParseResult`

```c++
    template <typename T>
    using ParseResult = std::expected<Output<T>, ParseError>;
```
Т.е. парсер принимает на вход некоторый input, и возвращает либо распаршенное значение вместе с оставшимся input-ом, либо ошибку парсинга.

```c++
    using Input = std::string_view;

    template <typename T>
    struct Output {
        T val;
        Input tail;
    }

    template <typename T>
    using TParseResult = std::expected<Output<T>, ParseError>;

    template <typename TParser>
    concept CParser = std::invocable<TParser, Input>;
```

```c++
    struct ParseError {
        std::string error_message;
        std::string_view rest;
    };

```

Для простоты определим ParseError, как структуру состоящую из произвольного сообщения об ошибке, и оставшегося input-а

---

### Его можно использовать так:

```c++
auto line = readline();
auto result = parser(line);
if (result.has_value()) {
    //Parsed sucessefully
} else {
    //Report error
}

```

---
### Parsecpp будет предоставлять небольшие строительные блоки для построения собственных парсеров
Отметим, что парсеры не будут хранить никакой-инфромации в полях структур. Любой парсер построенный из строительных блоков будет однозначно задаваться его типом. Это позволит нам на этапе компиляции анализировать построеные парсеры, или как-то их преобразовывать, не боясь потерять информацию.

---

### 0. Значения как типы
`integral_constants and string_constant`

---
### 1. Базовые парсеры
`basic_parser`

---
### 2.Простейшие комбинаторы
`basic_combinators`

---
### 3. Сложные комбинаторы и операторы
`advanced_combinators`

---

Теперь мы можем написать парсер Ipv4 адреса:
```c++
    std::expected<uint8_t, ParseError> ParseByte(std::deque<char> d) {
        assert(d.size() <= 3);
        assert(d.size() >= 1);

        if (d.size() == 1) {
            assert(c >= '0' && c <= '9');
            return c - '0';
        }

        uint16_t result = 0;

        for (char c : d) {
            assert(c >= '0' && c <= '9');
            if (result == 0 && c == '0') {
                return std::unexpected("sequence starts with 0");
            }

            result *= 10;
            result += (c - '0');
        }

        if (result > 255) {
            return std::unexpected("too large, does not fit in byte");
        }

        return uint8_t(result);
    }

    auto num = PSomeChar<'0', ..., '9'>{};
    auto byte = PApply(ParseByte, PMany(num, 1_const, 3_const));
    auto skipDot = PSkip(PChar<'.'>{})
    auto ipParser = byte + skipDot
                  + byte + skipDot
                  + byte + skipDot
                  + byte + PEof{};

    ipParser("192.168.0.1") //ok -> result = {tuple<uint8_t, uint8_t, uint8_t, uint8_t>{192, 168, 0, 1}. ""};
    ipParser("192.257.0.1") //error too large, does not fit in byte
```
