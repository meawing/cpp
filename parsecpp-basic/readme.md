Это вторая часть. Нужно будет реализовать базовые парсеры.

**ВНИМАНИЕ:** код нужно писать в основной задаче `parsecpp/lib`; для проверки этой части напишите что-нибудь в `dummy.txt`.

Начнем с определения базовых парсеров:
### PAnyChar
PAnyChar, будет парсить любой символ, и возвращать ошибку только в случае пустой строки
```c++
    struct PAnyChar {
        static constexpr ParseResult<char> operator()(Input in) noexcept;
    };

```

```c++
    auto parser = PAnyChar{};
    parser("abc") // ok -> result = {'a', "bc"}
    parser("") // error -> EOF
```

### PChar
```c++
    template <char C>
    struct PChar{
        ...
    };
```

Парсер PChar будет принимать на вход только один конкретный символ, иначе возвращать ошибку

```c++
    auto parser = PChar<'a'>{};
    std::string input0 = "abcd";
    std::string input1 = "bcd";
    std::string input2 = "";

    parser("abcd") // ok -> result = {'a', "bcd"};
    parser("bcd") // error -> unexpected symbol 'b';
    parser("") // error -> EOF;
```

### PString
PString будет работать аналогично, но принимать на вход некоторую строку. Тут нам понадобятся compile-time строки из предыдущего пункта.

```c++
    template <char... chars>
    struct PString {
       ...

       explicit PString(SString<chars...>) {};
    };

```

```c++
    auto parser = PString<'a', 'b', 'c'>{};
    auto abc = SString<'a', 'b', 'c'>{};
    auto same_parser = PString{abc};

    parser("abcd") // ok -> result = {"abc", "d"};
    parser("abdasd") // error -> strings do not match;
```

### PEof
Для начала определим новый возвращаемый тип - Unit
```c++
    struct Unit {};
```
Это будет наш аналог `void`, с той лишь разницей, что мы сможем создавать объекты типа `Unit`

PEof - простой парсер он возвращает Unit на пустой строке, и ошибку во всех остальных случаях.
Когда мы дойдем до комбинирования парсеров, PEof позволит нам проверять что input закончился

```c++
    auto parser = PEof{};
    parser("asdf") // error -> not eof
    parser("") -> ok {Unit{}, ""};
```
### PSomeChar
Парсер PSomeChar будет матчить один из символов из списка, например
```c++
    auto num = PSomeChar<'0', '1', ..., '9'>{};
    num('1abc') // ok -> {'1', "abc"}
    num('2abc') // ok -> {'2', "abc"}
    num('abc') // error = expected one of '0', ..., '9', got 'a'
```
- на самом деле `PChar` и `PAnyChar` это частные случаи `PSomeChar`, поэтому их реализация может состоять из одного `using`
