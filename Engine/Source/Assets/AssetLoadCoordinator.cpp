#include "Assets/AssetLoadCoordinator.h"

namespace Life::Assets
{
    std::atomic<uint64_t> AssetLoadCoordinator::s_Generation{1};
}
