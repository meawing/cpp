# Ranges

[**Range**](https://en.cppreference.com/w/cpp/ranges/range) - концепт типа,
допускающий итерирование по своим элементам,
предоставляя итератор на начало диапазона
и sentinel, обозначающий конец диапазона.

```c++
template <class T>
concept range = requires(T& t) {
    ranges::begin(t);
    ranges::end(t);
};
```

Примеры:
* std::vector\<int\>
* std::list\<int\>
* int[10]
* std::string

## Алгоритмы

В C++20 для большинства алгоритмов стандартной библиотеки появились
их [constrained](https://en.cppreference.com/w/cpp/algorithm/ranges) версии,
которые имеют ряд новых возможностей:
* Вместо пары iterator-sentinel можно подать range:
```c++
std::vector v = {3, 1, 2};
std::ranges::sort(v);
```
* Поддержка проекций:
```c++
struct X {
    int a;
    int b;
};

std::vector<X> v = GetVector();
std::ranges::sort(v, std::less{}, &X::b);
```
* Новые алгоритмы используют концепты, что улучшает читаемость ошибок компиляции.
* Типы возвращаемых значений большинства алгоритмов были изменены,
чтобы возвращать всю потенциально полезную информацию,
вычисленную во время выполнения алгоритма.

## View

[**View**](https://en.cppreference.com/w/cpp/ranges/view) это range, для которого операции копирования/перемещения выполняются
за константное время.

Примеры:
* std::string_view
* std::views::iota(0, n) - диапазон чисел `[0, n)`

Большинство view в стандартной библиотеке это [адапторы](https://en.cppreference.com/w/cpp/ranges#Range_adaptors) -
легковестные обёртки над какими-то другими
range, меняющие обход нижележащего диапазона.
Поскольку view это range, то над адаптором можно навесить
другой адаптор, организуя конвеер преобразований диапазона.
Способы создать view через адапторы:

```c++
auto view1 = Adaptor1(r, ...);
auto view2 = Adaptor2(...)(r);
auto view3 = r | Adaptor3(...);
auto view4 = r | Adaptor4(...) | Adaptor5(...);
auto view5 = view1 | Adaptor6(...);

auto adaptor = Adaptor7(...) | Adaptor8(...); // в этом задании от вас не требуется
auto view6 = adaptor(r);                      // реализовать такой способ
```

Рассмотрим некоторые примеры стандартных адапторов. Пусть $`r`$ это range,
итерирование по которому продуцирует последовательность $`\{ x_1, x_2, ..., x_n \}`$,
тогда мы можем получить следующие последовательности с помощью адапторов:

* $`\{ x_1, x_2, ..., x_{min\{n, m\}} \}`$
```c++
auto view = std::views::take(r, m);
for (auto& x : view) {
}
```
* $`\{ f(x_1), f(x_2), ..., f(x_n) \}`$
```c++
for (auto&& x : r | std::views::transform(f)) {
}
```
* $`\{ x_n, x_{n-1}, ..., x_1 \}`$
```c++
for (auto& x : std::views::reverse(r)) {
}
```

* $`\{ f(x_n), f(x_{n-1}), ..., f(x_1) \}`$
```c++
for (auto&& x : r | std::views::reverse | std::views::transform(f)) {
}

// или так

for (auto&& x : r | std::views::transform(f) | std::views::reverse) {
}
```

## Задача
Ваша задача реализовать 2 адаптора:

* $`\{ x_1`$.*p, $`x_2`$.*p, ..., $`x_n`$.*p $`\}`$, где
$`p`$ -
[pointer to data member](https://en.cppreference.com/w/cpp/language/pointer#Pointers_to_data_members)

Итерирование по полю структуры
```c++
struct X {
    int a;
    double b;
};

std::vector<X> r = GetVector();
for (double& b : r | Get<&X::b>) {
}
```

* $` \{ x_1, x_{1+s}, x_{1+2s}, ..., x_{1+ks} \} `$, где $` ks < n `$ и $`(k+1)s \ge n`$

Итерирование с шагом $`s`$.
```c++
for (auto& x : r | Stride(s)) {
}
```

## Take view
В файле `take.h` приведён пример реализации адаптора `Take`.
Его методы `begin()` и `end()` возвращают итераторы нижележащего range.
Для `Get` и `Stride` потребуется реализовать собственные типы итераторов.

### Полезные ссылки
* В примере используется [User-defined deduction guide](https://en.cppreference.com/w/cpp/language/class_template_argument_deduction#User-defined_deduction_guides)
* [std::ranges::views::all_t](https://en.cppreference.com/w/cpp/ranges/all_view)
* [std::ranges::view_interface](https://en.cppreference.com/w/cpp/ranges/view_interface)

### Примечание
В этой задаче отключена проверка clang-tidy.
