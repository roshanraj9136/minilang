#include <iostream>
#include <vector>
using namespace std;

int main() {
    vector<int> arr(5);
    cout << arr.size() << endl;
    
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;
    arr[3] = 40;
    arr[4] = 50;
    
    cout << arr[0] << endl;
    cout << arr[4] << endl;
    
    // Test dynamic element update
    int i = 0;
    while (i < arr.size()) {
        arr[i] = arr[i] + 5;
        i = i + 1;
    }
    
    cout << arr[0] << endl;
    cout << arr[1] << endl;
    cout << arr[2] << endl;
    cout << arr[3] << endl;
    cout << arr[4] << endl;
    return 0;
}
