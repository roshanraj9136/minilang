#include <iostream>
using namespace std;

int main() {
    int sum = 0;
    int i = 1;
    while (i <= 10) {
        sum = sum + i;
        i = i + 1;
    }
    cout << sum << endl;
    return 0;
}
