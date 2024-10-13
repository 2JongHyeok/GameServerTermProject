#include "Grid.h"

inline Grid::Grid() : cols(W_WIDTH / CELL_SIZE), rows(W_HEIGHT / CELL_SIZE) {
    cells.resize(rows, std::vector<std::unordered_set<int>>(cols));
    cellMutexes.reserve(rows);
    for (int i = 0; i < rows; ++i) {
        cellMutexes.emplace_back(cols);
    }
}

inline void Grid::addObject(const GameObject& obj) {
    int cellX = obj.x_ / CELL_SIZE;
    int cellY = obj.y_ / CELL_SIZE;
    if (cellX >= 0 && cellX < cols && cellY >= 0 && cellY < rows) {
        std::unique_lock<std::shared_mutex> lock(cellMutexes[cellY][cellX]);
        cells[cellY][cellX].insert(obj.id_);
    }
}

inline void Grid::removeObject(const GameObject& obj) {
    int cellX = obj.x_ / CELL_SIZE;
    int cellY = obj.y_ / CELL_SIZE;
    if (cellX >= 0 && cellX < cols && cellY >= 0 && cellY < rows) {
        std::unique_lock<std::shared_mutex> lock(cellMutexes[cellY][cellX]);
        cells[cellY][cellX].erase(obj.id_);
    }
}

inline void Grid::updateObject(const GameObject& oldPos, const GameObject& newPos) {
    int oldCellX = oldPos.x_ / CELL_SIZE;
    int oldCellY = oldPos.y_ / CELL_SIZE;
    int newCellX = newPos.x_ / CELL_SIZE;
    int newCellY = newPos.y_ / CELL_SIZE;

    if (oldCellX == newCellX && oldCellY == newCellY) {
        return;
    }

    std::unique_lock<std::shared_mutex> lockOld(cellMutexes[oldCellY][oldCellX], std::defer_lock);
    std::unique_lock<std::shared_mutex> lockNew(cellMutexes[newCellY][newCellX], std::defer_lock);
    std::lock(lockOld, lockNew);

    cells[oldCellY][oldCellX].erase(oldPos.id_);
    cells[newCellY][newCellX].insert(newPos.id_);
}

inline std::unordered_set<int> Grid::getNearbyObjects(const GameObject& obj) {
    std::unordered_set<int> nearby;
    int cellX = obj.x_ / CELL_SIZE;
    int cellY = obj.y_ / CELL_SIZE;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int nx = cellX + dx;
            int ny = cellY + dy;
            if (nx >= 0 && nx < cols && ny >= 0 && ny < rows) {
                std::shared_lock<std::shared_mutex> lock(cellMutexes[ny][nx]);
                for (int p : cells[ny][nx]) {
                    nearby.insert(p);
                }
            }
        }
    }

    return nearby;
}
