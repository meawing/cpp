В этой задаче вам предстоит реализовать типы - обертки над size_t и строковыми литералами, а также операторы и user-defined литералы для них

Для всех операторов сравнения вместо переменных типа `bool` нужно использовать разные типы - `std::true_type` для `true` и `std::false_type` для false, это нужно, т.к. сейчас мы будем работать не с объектами а с типами

```c++
template <size_t N>
struct SSizet {
    static constexpr size_t value = N;
};

auto one = SSizet<1>{};
auto two = SSizet<2>{};
auto three = two + one;
auto four = 4_const;
```
- user defined литерал для `Ssizet` должен иметь переменное количество шаблонных параметров типа `char`
- вам нужно распарсить символы, и вернуть объект типа SSizet

```c++
template <char... C>
consteval /*???*/ operator ""_const();
// 123_const // operator<'1', '2', '3'>""_const();
```

```c++
template <char... C>
struct SString {
    static constexpr std::string_view operator()();
};

auto hello0 = SString<'h', 'e', 'l', 'l', 'o'>{};
auto space = SString<' '>{};
auto world = "world"_cs;
auto hello_world = hello + space + world;
std::cout << hello_world() << std::endl;
```
- для строк нужно реализовать операторы `==` и `!=`, а также оператор сложения, который будет склеивать строки

```c++
    __extension__ template <std::same_as<char> CharType, CharType ...S>
    constexpr SString<S...> operator""_cs() noexcept {
        return {};
    }
```
Такой литерал для compile-time строк - это не часть стандарта, а `gnu-extension`.
Комитет считает что такой способ представления compile-time строк слишком неэффективен во время компиляции, потому что каждый символ такой строки - это шаблонный параметр, для которого компилятор будет создавать довольно большой объект. С помощью такого литерала можно легко создавать очень длинные
compile-time строки. В будущих стандартах возможно появится более эффективный способ представления compile-time строк, а пока будем пользоваться расширением gnu.
