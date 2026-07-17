#include <iostream>
using namespace std;

int main() {
    // 1. If (true) - Should eliminate else branch
    if (true) {
        cout << "if_true_branch" << endl;
    } else {
        cout << "if_true_else_unreachable" << endl;
    }

    // 2. If (false) - Should eliminate then branch
    if (false) {
        cout << "if_false_then_unreachable" << endl;
    } else {
        cout << "if_false_else_branch" << endl;
    }

    // 3. While (false) - Loop should be completely eliminated
    while (false) {
        cout << "while_false_unreachable" << endl;
    }

    // 4. Block unreachable statements after Return
    cout << "before_return" << endl;
    return 0;

    cout << "after_return_unreachable" << endl;
}
