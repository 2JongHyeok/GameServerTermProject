#include "ObjectManager.h"

inline int ObjectManager::addObject(int x, int y) {
    std::lock_guard<std::mutex> lock(objectsMutex);
    int id = objects.size();
    objects.emplace_back(x, y, id);
    return id;
}

inline GameObject* ObjectManager::getObject(int id) {
    std::lock_guard<std::mutex> lock(objectsMutex);
    if (id >= 0 && id < objects.size()) {
        return &objects[id];
    }
    return nullptr;
}
