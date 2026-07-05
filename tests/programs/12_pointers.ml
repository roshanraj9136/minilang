#include <iostream>
using namespace std;

void increment(int* ptr) {
    *ptr = *ptr + 1;
}

int main() {
    int x = 42;
    int* p = &x;
    cout << *p << endl;
    
    *p = 100;
    cout << x << endl;
    
    // Pass pointer to a function (pass-by-reference)
    increment(&x);
    cout << x << endl;
    return 0;
}
