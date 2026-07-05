#include <iostream>
using namespace std;

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

int main() {
    cout << add(3, 4) << endl;
    cout << multiply(5, 6) << endl;
    cout << add(multiply(2, 3), 4) << endl;
    return 0;
}
