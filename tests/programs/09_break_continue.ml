#include <iostream>
using namespace std;

int main() {
    int i = 0;
    while (i < 10) {
        i = i + 1;
        if (i == 5) {
            break;
        }
        cout << i << endl;
    }
    for (int j = 1; j <= 6; j = j + 1) {
        if (j % 2 == 0) {
            continue;
        }
        cout << j << endl;
    }
    return 0;
}
