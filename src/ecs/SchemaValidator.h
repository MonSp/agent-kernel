#pragma once

#include "Schema.h"
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <cstdint>

namespace ECS {

// ─── ValidationViolation ──────────────────────────────────────────────────────

struct ValidationViolation {
    std::string fieldName;
    std::string constraint;     // e.g. "min=0", "max=100", "required"
    std::string actualValue;    // stringified actual value
    std::string message;        // human-readable message
};

// ─── ValidationResult ─────────────────────────────────────────────────────────

struct ValidationResult {
    bool valid = true;
    std::vector<ValidationViolation> violations;

    std::string toJson() const {
        std::ostringstream oss;
        oss << "{\"valid\":" << (valid ? "true" : "false");
        oss << ",\"violations\":[";
        for (size_t i = 0; i < violations.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "{\"fieldName\":\"" << ComponentSchema::escapeJsonString(violations[i].fieldName) << "\"";
            oss << ",\"constraint\":\"" << ComponentSchema::escapeJsonString(violations[i].constraint) << "\"";
            oss << ",\"actualValue\":\"" << ComponentSchema::escapeJsonString(violations[i].actualValue) << "\"";
            oss << ",\"message\":\"" << ComponentSchema::escapeJsonString(violations[i].message) << "\"";
            oss << "}";
        }
        oss << "]}";
        return oss.str();
    }
};

// ─── ComponentSchema::validate() ──────────────────────────────────────────────
// Defined here (not in Schema.h) because it needs the full ValidationResult.

inline ValidationResult ComponentSchema::validate(const void* ptr) const {
    ValidationResult result;
    const uint8_t* base = static_cast<const uint8_t*>(ptr);

    for (const auto& field : fields) {
        const uint8_t* fieldPtr = base + field.offset;

        // ── Check required (for string fields: non-empty) ─────────────────────
        if (field.constraint.required && field.type == FieldType::String) {
            const std::string* s = reinterpret_cast<const std::string*>(fieldPtr);
            if (s->empty()) {
                result.valid = false;
                result.violations.push_back({
                    field.name,
                    "required",
                    "",
                    "Field is required but empty"
                });
            }
        }

        // ── Check min/max for floating-point fields ───────────────────────────
        if (field.type == FieldType::Float32 || field.type == FieldType::Float64) {
            float val;
            if (field.type == FieldType::Float32) {
                std::memcpy(&val, fieldPtr, sizeof(float));
            } else {
                double d;
                std::memcpy(&d, fieldPtr, sizeof(double));
                val = static_cast<float>(d);
            }

            if (val < field.constraint.min) {
                result.valid = false;
                result.violations.push_back({
                    field.name,
                    "min=" + std::to_string(field.constraint.min),
                    std::to_string(val),
                    "Value below minimum"
                });
            }
            if (val > field.constraint.max) {
                result.valid = false;
                result.violations.push_back({
                    field.name,
                    "max=" + std::to_string(field.constraint.max),
                    std::to_string(val),
                    "Value above maximum"
                });
            }
        }

        // ── Check min/max for signed integer fields ───────────────────────────
        if (field.type == FieldType::Int8 || field.type == FieldType::Int16 ||
            field.type == FieldType::Int32 || field.type == FieldType::Int64) {
            int64_t val = 0;
            std::memcpy(&val, fieldPtr, field.size);
            // Sign-extend based on size
            if (field.size == 1) val = static_cast<int64_t>(static_cast<int8_t>(val & 0xFF));
            else if (field.size == 2) val = static_cast<int64_t>(static_cast<int16_t>(val & 0xFFFF));
            else if (field.size == 4) val = static_cast<int64_t>(static_cast<int32_t>(val & 0xFFFFFFFF));

            float fval = static_cast<float>(val);
            if (fval < field.constraint.min) {
                result.valid = false;
                result.violations.push_back({
                    field.name,
                    "min=" + std::to_string(static_cast<int64_t>(field.constraint.min)),
                    std::to_string(val),
                    "Value below minimum"
                });
            }
            if (fval > field.constraint.max) {
                result.valid = false;
                result.violations.push_back({
                    field.name,
                    "max=" + std::to_string(static_cast<int64_t>(field.constraint.max)),
                    std::to_string(val),
                    "Value above maximum"
                });
            }
        }

        // ── Check min/max for unsigned integer fields ─────────────────────────
        if (field.type == FieldType::Uint8 || field.type == FieldType::Uint16 ||
            field.type == FieldType::Uint32 || field.type == FieldType::Uint64) {
            uint64_t val = 0;
            std::memcpy(&val, fieldPtr, field.size);

            float fval = static_cast<float>(val);
            if (fval < field.constraint.min) {
                result.valid = false;
                result.violations.push_back({
                    field.name,
                    "min=" + std::to_string(static_cast<uint64_t>(field.constraint.min)),
                    std::to_string(val),
                    "Value below minimum"
                });
            }
            if (fval > field.constraint.max) {
                result.valid = false;
                result.violations.push_back({
                    field.name,
                    "max=" + std::to_string(static_cast<uint64_t>(field.constraint.max)),
                    std::to_string(val),
                    "Value above maximum"
                });
            }
        }
    }

    return result;
}

// ─── Free-function helper (for GenericComponentStore use) ─────────────────────

inline ValidationResult validateComponent(const ComponentSchema& schema, const void* ptr) {
    return schema.validate(ptr);
}

} // namespace ECS
