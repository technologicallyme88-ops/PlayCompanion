#pragma once

class GfxRenderer;
class MappedInputManager;

namespace wake_resume {

bool open(const char* activityName, GfxRenderer& renderer, MappedInputManager& mappedInput);

}  // namespace wake_resume
