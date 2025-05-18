#pragma once

#include "bit_field.h"

class TSet 
{
private:
	int MaxPower;//максимальный мощность множества 
	TBitField BitField;
public:
	TSet(int maxPower);
	TSet(const TSet& set);//конструктор копирования
	TSet(TBitField& bf);//конструктор преобразования 
	//Доступ к битам
	int GetMaxPower()const;//максимальное мощноесть множества
	void InsElem(const int n);//включить элемнт в множество 
	void DelElem(const int n);//удалить элемнеть в множество
	bool IsMember(const int kSomeNumber);//проверить наличие элементов в множестве
	//операции
	bool operator==(const TSet& set)const;//сравнение 
	TSet& operator=(const TSet& set_);//Присваивание
	TSet operator+(const TSet& set)const;//Объединение
	TSet operator*(const TSet& Set)const;//Пересечение 
	TSet operator+(const int n);//Включение эл-та в мн-ва
	TSet operator-(const int n);//Исключение эл-та из мн-ва
	TSet operator~();//инверсия
	friend istream& operator >>(istream&_ist, TSet& bf_);//опепатор ввода
	friend ostream& operator <<(ostream&o_st, TSet& bf);//оператор вывода
};
