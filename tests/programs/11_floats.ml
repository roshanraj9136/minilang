#include <iostream>
using namespace std;

int main() {
    float a = 3.14;
    float b = 2.0;
    cout << (a + b) << endl;
    cout << (a - b) << endl;
    cout << (a * b) << endl;
    cout << (a / b) << endl;
    
    // Test implicit promotion
    float c = 5;
    cout << c << endl;
    cout << (c + 1.5) << endl;
    cout << (10.5 - 2) << endl;
    
    // Comparisons
    cout << (a > b) << endl;
    cout << (a == 3.14) << endl;
    return 0;
}
