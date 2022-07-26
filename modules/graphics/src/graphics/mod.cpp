#include "graphics/mod.hpp"

#include "core/logger.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

GraphicsModule::GraphicsModule() {
    logger_ = LoggerManager::Get().RegisterLogger("Graphics");
}

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
