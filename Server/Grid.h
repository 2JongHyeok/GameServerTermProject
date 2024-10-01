#pragma once
#include "protocol.h"
#include "GameObject.h"
#include <vector>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>

constexpr int W_WIDTH = 2000;
constexpr int W_HEIGHT = 2000;
const int CELL_SIZE = 10;

class Grid
{
private:
    std::vector<std::vector<std::unordered_set<int>>> cells;
    std::vector<std::vector<std::shared_mutex>> cellMutexes;
    int cols, rows;

public:
    Grid();
    void addObject(const GameObject& obj);
    void removeObject(const GameObject& obj);
    void updateObject(const GameObject& oldPos, const GameObject& newPos);
    std::vector<int> getNearbyObjects(const GameObject& obj);
};

