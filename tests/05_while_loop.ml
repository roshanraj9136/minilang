#include <iostream>
using namespace std;

int main() {
    int n = 5;
    int fact = 1;
    while (n > 0) {
        fact = fact * n;
        n = n - 1;
    }
    cout << fact << endl;
    return 0;
}
