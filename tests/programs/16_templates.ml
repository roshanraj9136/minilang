#include <iostream>
#include <vector>
#include <queue>
#include <utility>

using namespace std;

int main() {
    std::pair<int, int> p(3, 4);
    cout << "p = (" << p.first << ", " << p.second << ")" << std::endl;

    vector<int> v;
    v.push_back(10);
    std::vector<int> v2;
    v2.push_back(20);
    v2.push_back(30);
    cout << "v size = " << (v.size() + v2.size()) << ", elements: ";
    for (int i = 0; i < v.size(); i++) {
        cout << v[i] << " ";
    }
    for (int i = 0; i < v2.size(); i++) {
        cout << v2[i] << " ";
    }
    cout << std::endl;

    std::queue<int> q;
    q.push(100);
    q.push(200);
    cout << "q size = " << q.size() << ", front = " << q.front() << endl;
    q.pop();
    cout << "q empty = " << q.empty() << ", new front = " << q.front() << endl;

    std::vector<pair<int, int>> adj;
    adj.push_back(std::pair<int, int>(1, 10));
    adj.push_back(pair<int, int>(2, 20));
    std::cout << "adj[0] = (" << adj[0].first << ", " << adj[0].second << ")" << endl;

    return 0;
}
