#pragma once

#include "ProgramLibrary.h"

namespace Program {

// Thin alias — execution lives on ProgramLibrary::run.
// Kept as a named type so Game/Company can depend on "ProgramRuntime"
// without pulling the full library API into call sites.
using ProgramRuntime = ProgramLibrary;

}  // namespace Program
