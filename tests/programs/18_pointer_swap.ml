#include <iostream>
using namespace std;

void swap(int* p1, int* p2) {
    int temp = *p1;
    *p1 = *p2;
    *p2 = temp;
}

int main() {
    int a = 10;
    int b = 20;
    int* ptrA = &a;
    int* ptrB = &b;
    cout << "Before swap: a = " << a << ", b = " << b << endl;
    swap(ptrA, ptrB);
    cout << "After swap: a = " << a << ", b = " << b << endl;
    return 0;
}
