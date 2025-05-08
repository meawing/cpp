### PSeq
Напишем наконец PSeq.

PSeq будет последовательно вызывать свои парсеры, складывая результат в tuple. Если же возникла ошибка, PSeq вернет ее. Пусть парсеры переданные в PSeq имели типы: `ParseResult<T>, ParseResult<U>, ParseResult<T>`.
Тогда тип PSeq будет `ParserResult<tuple<T, U, T>>`

```c++
    template <typename... Parsers>
    struct PSeq {
        explicit PSeq(Parsers...) {}

        ...
    };
```

Основная сложность будет в пропуске `Unit`. Т.к. мы хотим игнорировать все парсеры возвращающие `Unit`, для парсеров с типами `ParseResult<Unit>, ParseResult<U>, ParseResult<T>`, тип PSeq будет `ParseResult<tuple<U, T>>`.

Если парсер типа `Unit` вернул ошибку, то она будет возвращена парсером `PSeq`, иначе результат `Unit` будет проигнорирован и оставшийся input будет передан следущему парсеру. Вам предстоит придумать как убрать все типы Unit из возвращаемого типа, а также придумать как пропускать результат, и
переходить к следущему парсеру.

```c++
    auto parser0 = PSeq(PChar<'a'>{}, PChar<'b'>{});
    auto parser1 = PSeq(PSkip(PChar<'a'>{}), PChar<'b'>{});

    parser0("abc") // ok -> result = {tuple{'a', 'b'}, "c"}
    parser1("abc") // ok -> result = {'b', "c"}

    parser0("Abc") //  error = expected 'a', got 'A'
    parser1("Abc") //  error = expected 'a', got 'A'
```

### PChoice
Комбинатор PChoice будет вызывать парсеры последовательно на одном же input, пока парсер не завершится успешно. Вслучае если все парсеры вернули ошибку, PChoice тоже вернет ошибку.
Возвращаемым типом для PChoice будет std::variant.
Но есть проблема: для `PSeq`, несколько парсеров одинакового типа хоть и не позволяли нам использовать `std::get<T>(tuple)` для получения значения, но оставляли `std::get<Idx>(tuple)`. Тогда как std::variant, в котором встречаются одинаковые типы пользоваться невозможно. Вашей задачей будет удалить
все дубликаты из списка типов.

```c++
    auto parser0 = PChoice(PChar<'a'>{}, PString<'a', 'b', 'c'>{}) // returns ParseResult<variant<char, string_view>>
    auto parser1 = PChoice(PChar<'a'>{}, PChar<'b'>{}) //returns ParseResult<variant<char>>
```

### Операторы
Чтобы комбинаторами `PSeq` и `PChoice` было удобнее пользоваться, давайте определим для них операторы `+` и `|`

```c++
    auto p0 = ...;
    auto p1 = ...;
    auto p2 = ...;

    auto choice = p0 | p1 | p2 //same as PChoice(p0, p1, p2)
    auto seq = p0 + p1 + p2 //same as PSeq(p0, p1, p2)
```
