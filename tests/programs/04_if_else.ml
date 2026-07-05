#include <iostream>
using namespace std;

int main() {
    int x = 10;
    if (x > 5) {
        cout << 1 << endl;
    } else {
        cout << 0 << endl;
    }
    if (x < 5) {
        cout << 1 << endl;
    } else if (x == 10) {
        cout << 2 << endl;
    } else {
        cout << 3 << endl;
    }
    return 0;
}
