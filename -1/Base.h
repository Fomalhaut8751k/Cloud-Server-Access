#pragma once
#include<iostream>

using namespace std;

class Base
{
public:
	virtual Base* getInstance() = 0;

	void show() const;
private:

};