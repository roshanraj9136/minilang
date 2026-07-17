#include <iostream>
using namespace std;

int main() {
    const int X = 10;
    const int Y = X + 5; // Propagates X -> 10, folds 10 + 5 -> 15

    cout << X << endl; // Should output 10
    cout << Y << endl; // Should output 15

    if (true) {
        const int X = 200; // Scoped constant shadowing
        const int Z = X + Y; // Z = 200 + 15 = 215
        cout << X << endl; // Should output 200
        cout << Z << endl; // Should output 215
    }

    cout << X << endl; // Should output 10 (outer X)

    const string G = "hello";
    const string MSG = G + " world"; // Propagates G, folds string concat
    cout << MSG << endl; // Should output "hello world"

    return 0;
}
