#pragma once

// Unity configuration for ASAP project.
// We rely on PlatformIO's Unity integration to define UNITY_INCLUDE_CONFIG_H
// via CPPDEFINES, so we do not redefine it here to avoid macro redefinition
// warnings. This header is intentionally minimal; extend as needed.

#define UNITY_OUTPUT_COLOR
#define UNITY_EXCLUDE_SETJMP_H

