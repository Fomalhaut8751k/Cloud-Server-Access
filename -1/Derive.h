#pragma once
#include<iostream>
#include"Base.h"

using namespace std;

class Derive : public Base
{
public:
	Derive();

	virtual Base* getInstance()
	{
		static Derive instance;
		return &instance;
	}

private:
	int _d;
};