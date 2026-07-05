#pragma once

#include <utility>
#include <vector>

namespace maffe {

class UnionFind {
public:
    explicit UnionFind(const int n) : e(n, -1) {}

    [[nodiscard]] int find(int x) {
        int root = x;
        while (e[root] >= 0)
            root = e[root];
        while (x != root) {
            const int parent = e[x];
            e[x] = root;
            x = parent;
        }
        return root;
    }

    [[nodiscard]] bool same_set(const int a, const int b) {
        return find(a) == find(b);
    }

    bool join(int a, int b) {
        a = find(a);
        b = find(b);
        if (a == b)
            return false;
        if (e[a] > e[b])
            std::swap(a, b);
        e[a] += e[b];
        e[b] = a;
        return true;
    }

private:
    std::vector<int> e;
};

} // namespace maffe
