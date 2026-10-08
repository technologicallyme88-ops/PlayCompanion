#pragma once

namespace farm {

// Installs the firmware's crop-art pack only when its version changes. The
// bytes live in flash and are streamed directly to SD without a heap buffer.
bool ensureCropAssets();

}  // namespace farm
