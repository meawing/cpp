#pragma once

#include <iostream>
#include <vector>
#include <string>

class Test {
public:
	Test() = default;

	int Method() const {
		return 0;
	}

	static const double Static_Method() {
		return 0;
	}
private:
	int field_;
	double bad_bield;
	int64_t reallyBad_;

	static const int kGood = 5;
	static const int kBad_ = 3;
	static int bad_val;

	int privateMethod() {
		return 0;
	}
};

struct JustAStruct {
	int ok_field;
	int bad_field_;
	const int kGood = 3;

	static int static_ok;

	static int var_;

	size_t size() const;
};
