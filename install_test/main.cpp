#include <rjk/duck.hpp>

#include <cstddef>
#include <string>
#include <vector>

struct Container {
    auto size() const -> std::size_t;
    auto empty() const -> bool;
};

int main() {
    rjk::duck<Container> c{std::vector<int>{1, 2, 3}};
    if (c.size() != 3) {
        return 1;
    }

    c = std::string{"hello"};
    if (c.size() != 5 || c.empty()) {
        return 1;
    }

    return 0;
}
