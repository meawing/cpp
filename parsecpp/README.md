# Parsecpp

- https://serokell.io/blog/parser-combinators-in-haskell#what-is-a-parser-combinator-and-why-use-them%3F

Для начала нужно решить задачу `parsecpp-constants`
---

Предположим мы хотим научиться парсить ipv4 адреса. Какие опции у нас есть?

- Написать руками
- Bison, Yacc, etc.
- Parser combinators

---

## Bison
From https://github.com/vxmdesign/microjson/tree/master

С помощью bison можно сгенерировать кoд на одном из поддерживаемых языков на основе описания грамматики

```
%{
#include <stdio.h>
#include <stdlib.h>

void yyerror(const char *msg);
int yylex(void);

unsigned char octets[4];
%}

%union {
    int num;
}

%token <num> NUMBER
%token DOT

%type <num> octet

%%

input:
    ipv4_address    { printf("IPv4: %d.%d.%d.%d\n", 
                          octets[0], octets[1], octets[2], octets[3]);
                    }
    | error         { yyerror("Invalid IPv4 address"); }
;

ipv4_address:
    octet DOT octet DOT octet DOT octet
    {
        octets[0] = $1;
        octets[1] = $3;
        octets[2] = $5;
        octets[3] = $7;
    }
;

octet:
    NUMBER
    {
        if ($1 < 0 || $1 > 255) {
            yyerror("Octet value out of range (0-255)");
            YYERROR;
        }
        $$ = $1;
    }
;

%%

void yyerror(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
}

int main() {
    return yyparse();
}
```
---

## Parsec

Для haskell, существует библиотека `parsec`, которая предоставляет DSL для описания парсеров внутри языка.

```haskell
import Text.Parsec
import Text.Parsec.String (Parser)
import Control.Applicative ((<$>), (<*>), (<*))
import Data.Char (digitToInt)

segment :: Parser Int
segment = read <$> many1 digit >>= \n ->
  if n >= 0 && n <= 255
    then return n
    else fail "Segment out of range (must be 0-255)"

dot = char '.'

ipv4Parser :: Parser (Int, Int, Int, Int)
ipv4Parser = (,,,) <$> segment <* dot
                   <*> segment <* dot
                   <*> segment <* dot
                   <*> segment
```

Для многих языков существуют аналоги

---

## Parsecpp

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
`parsecpp-constants`

---
### 1. Базовые парсеры
`parsecpp-basic`

---
### 2.Простейшие комбинаторы
`parsecpp-basic-combinators`

---
### 3. Сложные комбинаторы и операторы
`parsecpp-advanced`

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

## Что теперь?
Библиотеку можно развивать и дальше:
- Добавить лексер
- Улучшить обработку и возврат ошибок(вспомните как выглядит синтаксическая ошибка в большинстве современных компиляторов)
- Добавлять статические анализаторы
- Добавлять обертки для автоматической обработки комментариев в коде
- Сделать шаблонный pretty-printer
- ...
