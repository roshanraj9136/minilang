#include <iostream>
#include <string>
using namespace std;

void print_test(string s) {
    cout << s << endl;
}

int add(int a, int b) {
    return a + b;
}

int main() {
    print_test("--- C++ Syntax and Feature Test ---");

    // Variable & pointer declaration
    int x = 10;
    int y = 20;
    cout << add(x, y) << endl; // 30

    // Compound assignment
    x += 5;
    cout << x << endl; // 15

    // Pre/post increment
    cout << x++ << endl; // 15 (prints original value)
    cout << x << endl;   // 16
    cout << ++x << endl; // 17 (prints incremented value)

    // Bitwise operators
    int a = 5;  // 0101
    int b = 3;  // 0011
    cout << (a & b) << endl; // 1
    cout << (a | b) << endl; // 7
    cout << (a ^ b) << endl; // 6
    cout << (~a) << endl;    // -6
    cout << (a << 1) << endl; // 10

    // Ternary operator
    string msg = (a > b) ? "a is greater" : "b is greater";
    print_test(msg); // a is greater

    // Const declaration
    const int limit = 100;
    cout << limit << endl; // 100

    // Typecast
    float f = 5.5;
    int i = f as int;
    cout << i << endl; // 5

    // Sizeof & Typeof
    cout << sizeof(int) << endl; // 8
    print_test(typeof(f)); // float

    // Do-while loop
    int count = 0;
    do {
        count++;
    } while (count < 3);
    cout << count << endl; // 3

    // Switch case
    int val = 2;
    switch (val) {
        case 1:
            print_test("one");
        case 2:
            print_test("two");
        case 3:
            print_test("three");
        default:
            print_test("other");
    }
    return 0;
}
