#pragma once

#include <atomic>

class GameObject
{
public:
    std::atomic<int> x_;
    std::atomic<int> y_;
    int id_;

    GameObject(int x, int y, int id) : x_(x), y_(y), id_(id) {}
    GameObject() {}
};

