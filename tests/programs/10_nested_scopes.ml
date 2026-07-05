#include <iostream>
using namespace std;

int main() {
    int x = 1;
    {
        int y = 2;
        cout << (x + y) << endl;
    }
    cout << x << endl;
    return 0;
}
