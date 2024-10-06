#pragma once

#include <vector>
#include <mutex>
#include "GameObject.h"

class ObjectManager {
private:
    std::vector<GameObject> objects;
    std::mutex objectsMutex;

public:
    int addObject(int x, int y);
    GameObject* getObject(int id);
};
