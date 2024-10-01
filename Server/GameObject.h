#pragma once

#include <atomic>

class GameObject
{
public:
    std::atomic<int> x;
    std::atomic<int> y;
    int id;

    GameObject(int x, int y, int id) : x(x), y(y), id(id) {}
};

