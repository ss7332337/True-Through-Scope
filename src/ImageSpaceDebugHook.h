#pragma once

namespace ThroughScope
{
    // 注册 ImageSpace 调试 Hook
    // 在 RenderDoc 中可以看到各个 ImageSpaceEffect 的执行位置
    void RegisterImageSpaceDebugHooks();
}
