#include "set.h"

TSet::TSet(int maxPower) :MaxPower(maxPower), BitField(maxPower)
{
}

TSet::TSet(const TSet& set) : MaxPower(set.MaxPower), BitField(set.BitField)
{
}

TSet::TSet(TBitField& BF) : MaxPower(BF.GetLenght()), BitField(BF)
{
}

int TSet::GetMaxPower() const
{
	return MaxPower;
}

void TSet::InsElem(const int kNum)
{
	BitField.SetBit(kNum);
}

void TSet::DelElem(const int N)
{
	BitField.ClrBit(N);
}

bool TSet::IsMember(const int nN)
{
	return BitField.GetBit(nN);
}

bool TSet::operator==(const TSet& _) const
{
	return BitField == _.BitField;
}

TSet& TSet::operator=(const TSet& set)
{
	BitField = set.BitField;
	MaxPower = set.MaxPower;
	return *this;
}

TSet TSet::operator+(const TSet& set) const
{
	TBitField tmpB = BitField | set.BitField;
	TSet temp(tmpB);
	return temp;
}

TSet TSet::operator*(const TSet& set) const
{
	TBitField tmpB = BitField & set.BitField;
	TSet temp(tmpB);
	return temp;
}

TSet TSet::operator+(const int n)
{
	TBitField bb = TBitField(this->BitField);
	TSet BB = TSet(bb);
	BB.InsElem(n);
	return BB;
}

TSet TSet::operator-(const int n)
{
	TBitField bb = TBitField(this->BitField);
	TSet BB = TSet(bb);
	BB.DelElem(n);
	return BB;
}
TSet TSet::operator~()
{
	TBitField tmpB = ~BitField;
	TSet temp(tmpB);
	return temp;
}

istream& operator>>(istream& istr, TSet& bf)
{
	int temp; char ch;
	do
	{
		istr >> ch;
	} while (ch != '{');
	do
	{
		istr >> temp;
		bf.InsElem(temp);
		do
		{
			istr >> ch;
		} while ((ch != ',') && (ch != '}'));
	} while (ch != '}');
	return istr;
}

ostream& operator<<(ostream& ostr, TSet& bf)
{
	int i, n;
	char ch = ' ';
	ostr << '{';
	n = bf.GetMaxPower();
	for (i = 0; i < n; i++)
	{
		if (bf.IsMember(i))
		{
			ostr << ch << ' ' << i;
			ch = ',';
		}
	}
	ostr << '}';
	return ostr;
}

int main() {
}

