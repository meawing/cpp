#include <iostream>
#include <cstdint>

#define TELEM uint32_t
using namespace std;

class TBitField
{
private:
	int BitLen;//длина битового поля
	int MemLen;//кол-во элементов Mem для представления битового поля
	TELEM* pMem;//Память для представления битового поля
public:
	TBitField();
	TBitField(int len);
	TBitField(const TBitField& bf);
	~TBitField();
	//Доступ к битам
	void SetBit(const int kNumber);//установить бит
	void ClrBit(const int n);//очистить бит 
	int GetBit(const int kNum);// const;получить значения бита
	int GetMemIndex(const int n);//Индекс Mem для бита n
	TELEM GetMemMask(const int n);//Битовая маска для бита n 
	int GetLenght();//Получить длину кода
	//битовые операции
	bool operator ==(const TBitField& bf)const;//сравнение
	TBitField& operator=(const TBitField& bf);//присваивание
	TBitField operator | (const TBitField & bf)const;//операция или
	TBitField operator &(const TBitField& bf)const;//операция и
	TBitField operator ~();//отрицание 
	friend istream&operator>> ( istream& istr, TBitField& bf);
	friend ostream&operator<<(ostream& ostr, TBitField& bf);
};
