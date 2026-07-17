#include <iostream>
using namespace std;

int main() {
    // Int arithmetic folding
    int i1 = 5 + 10 * 2;
    int i2 = 20 / 4 - 1;
    int i3 = 10 % 3;
    int i4 = 1 << 4;
    int i5 = 32 >> 2;
    int i6 = 12 & 7;
    int i7 = 8 | 3;
    int i8 = 10 ^ 6;

    // Float arithmetic folding
    float f1 = 2.5 + 1.5 * 2.0;
    float f2 = 5.0 - 1.0;

    // Bool folding
    bool b1 = true && false;
    bool b2 = true || false;
    bool b3 = 5 > 3;
    bool b4 = 2.0 == 2.0;

    // String concat folding
    string s = "hello " + "world";

    // Unary folding
    int u1 = -5;
    int u2 = ~0;
    bool u3 = !true;

    // Ternary folding
    int t1 = true ? 100 : 200;
    int t2 = false ? 100 : 200;

    // Cast folding
    int c1 = 3.14 as int;
    float c2 = 5 as float;
    bool c3 = 0 as bool;

    cout << i1 << endl;
    cout << i2 << endl;
    cout << i3 << endl;
    cout << i4 << endl;
    cout << i5 << endl;
    cout << i6 << endl;
    cout << i7 << endl;
    cout << i8 << endl;
    cout << f1 << endl;
    cout << f2 << endl;
    cout << b1 << endl;
    cout << b2 << endl;
    cout << b3 << endl;
    cout << b4 << endl;
    cout << s << endl;
    cout << u1 << endl;
    cout << u2 << endl;
    cout << u3 << endl;
    cout << t1 << endl;
    cout << t2 << endl;
    cout << c1 << endl;
    cout << c2 << endl;
    cout << c3 << endl;

    return 0;
}
