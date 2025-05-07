Для работы с типами вам скорее всего захочется определить структуры. Это позволит работать с типами как с объектами, и сильно упростит написание сложных комбинаторов
```c++
    template <typename T>
    SType {};

    template <typename... T>
    STypes {};
```

### PSkip
Иногда нам нужно "проглотить" какую-то часть инпута и проигнорировать результат.
Например, комбинатор PSeq который мы напишем позже будет последовательно вызывать все парсеры, складывая результат в tuple:
```c++
    auto parser = PSeq(PAnyChar{}, PChar<'.'>{}, PAnyChar{});

    parser('a.bc'); // ok -> result = {tuple{'a', '.', 'b'}, "c"};
```
Но допустим нас не интересует `.` между символами, мы только хотим проверить что она существует. Для этого воспользуемся комбинатором PSkip

```c++
    template <typename Parser>
    struct PSkip {
        explicit PSkip(Parser) {}

        ParseResult<Unit> operator()(Input in) ...
    };


    auto parser = PSeq(PAnyChar{}, PSkip(PChar<'.'>{}), PAnyChar{});

    parser('a.bc'); // ok -> result = {tuple{'a', Unit{}, 'b'}, "c"};
```
На самом деле комбинатор PSeq который мы напишем, будет игнорировать все значения типа Unit, поэтому результат будет выглядеть так:

```c++
    parser('a.bc'); // ok -> result = {tuple{'a', 'b'}, "c"};
    parser('a,bc'); // ok -> error = expected '.', got ',';
```

### PMaybe
Иногда мы не хотим кидать ошибку, а хотим сделать парсер либо возвращающий результат либо нет
```c++
    template <typename Parser>
    struct PMaybe {
    };
```

Для парсера возвращающего `ParseResult<T>`, комбинатор `PMaybe` будет конструировать парсер возвращающий
`ParseResult<std::optional<T>>`, и игнорирующий ошибки.

Например, вот так мы можем распарсить число от -9 до 9:
```c++
    auto num = PSomeChar<'0', ..., '9'>{};
    auto parser = PSeq(PMaybe(PChar<'-'>{}), num);
    parse('-1') // ok -> {tuple{optional<char>{'-'}, '1'}, ""}
    parse('1') // ok -> {tuple{optional<char>{}, '1'}, ""}
```

### PMany
С помощью PMany мы сможем парсить произвольное количество вхождений какого-то парсера, и складывать результат в какой-нибудь контейнер, допустим `std::deque`

Для удобства нам понадобится `SSizet`

```c++
    template <typename Parser, size_t Min, size_t Max>
    struct PMany {
        constexpr PMany(Parser, SSizet<Min>{}, SSizet<Max>{});
    };
```

```c++
    auto num = PSomeChar<'0', ..., '9'>{};
    auto parser = PMany(num, 2_const, 4_const); //accept "00" to "9999"
    parser("abc") // error -> not enough num
    parser("1abc") // error -> not enough num
    parser("12abc") // ok -> {std::deque{'1', '2'}, "abc"};
    parser("1234abc") // ok -> {std::deque{'1', '2', '3', '4'}, "abc"};
    parser("12345abc") // ok -> {std::deque{'1', '2', '3', '4'}, "5abc"};
```
PMany будет пытаться прочитать новый элемент пока их количество меньше `Max`, или пока не вернулась ошибка
Если не удалось прочитать хотя бы `Min` элементов, PMany завершается с ошибкой

### PApply
Чтобы не таскать с собой артефакты парсинга, мы можем захотеть создать какой-то объект из распаршенной структуры
```c++
    template <typename Applier, typename Parser>
    struct PApply{
        constexpr PApply(Applier, Parser) {}
        ...
    };
```
Пусть `Parser` возвращает `ParseResult<T>`, тогда `Applier` - функтор принимающий объект типа T, и возвращающий `std::expected<U, std::string>`, т.е. либо у нас получилось успешно создать объект типа U из T, либо создание завершилось с ошибкой. `PApply` должен превращать `ParseResult<T>` в
`ParseResult<U>` и прокидывать дальше оставшийся инпут, а также прокидывать сообщение об ошибке в ParseError
```c++

std::expected<int, std::string> makeInt(tuple<optional<char>, deque<char>> num) {
    //parse int here
}

auto num = ...;
auto parser = PSeq(PMaybe(PChar<'-'>{}), num);
auto intParser = PApply(makeInt, parser);

parser("123") //ok -> result = {tuple{optional{}, deque{'1', '2', '3'}}, ""}
intParser("123") //ok -> result = {123, ""}

parser("-123") //ok -> result = {tuple{optional{'-'}, deque{'1', '2', '3'}}, ""}
intParser("-123") //ok -> result = {-123, ""}

```


При этом мы не будем сохранять объект типа `Applier`, когда нам понадобится его вызвать, мы создадим объект на месте, например так:
```c++
    ...
    Applier{}(parseResult)
    ...
```
Так мы сможем гарантировать что поведение парсера зависит только от его типа, а не от сохраненных полей. Соотвественно любой `Applier` который мы передадим не должен хранить состояние, т.е. не должен отличаться по семантике от свободной функции
