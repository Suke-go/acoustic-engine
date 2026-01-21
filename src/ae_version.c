/**
 * @file ae_version.c
 * @brief Version API implementation
 */

#include "ae_internal.h"

static const char *g_version_string = "0.2.0";

/**
 * Get version as packed integer (major.minor.patch)
 */
AE_API uint32_t ae_get_version(void) { return AE_VERSION; }

/**
 * Get version as string
 */
AE_API const char *ae_get_version_string(void) { return g_version_string; }

/**
 * Check ABI compatibility
 * Compatible if major version matches and expected minor <= current minor
 */
AE_API bool ae_check_abi_compatibility(uint32_t expected_version) {
  uint32_t expected_major = (expected_version >> 16) & 0xFF;
  uint32_t expected_minor = (expected_version >> 8) & 0xFF;

  uint32_t current_major = AE_VERSION_MAJOR;
  uint32_t current_minor = AE_VERSION_MINOR;

  /* Major version must match */
  if (expected_major != current_major) {
    return false;
  }

  /* Expected minor must be <= current (backward compatible) */
  if (expected_minor > current_minor) {
    return false;
  }

  return true;
}
