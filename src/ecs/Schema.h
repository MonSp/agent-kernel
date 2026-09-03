#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace ECS {

// ─── Forward declarations ─────────────────────────────────────────────────────
struct ValidationResult;  // defined in SchemaValidator.h

// ─── FieldType enum ──────────────────────────────────────────────────────────

enum class FieldType : uint8_t {
    Bool = 0,
    Int8, Int16, Int32, Int64,
    Uint8, Uint16, Uint32, Uint64,
    Float32, Float64,
    String,
    Enum,        // named enum with int backing
    Struct,      // nested struct reference
    Array,       // fixed-size array
    Map          // dynamic map (string->value)
};

// ─── FieldConstraint ─────────────────────────────────────────────────────────

struct FieldConstraint {
    float min = -1e30f;
    float max = 1e30f;
    bool required = false;
    std::string defaultExpr;  // human-readable default
};

// ─── FieldDescriptor ─────────────────────────────────────────────────────────

struct FieldDescriptor {
    std::string name;         // field name (e.g. "hp", "ambition")
    FieldType type;           // field type tag
    size_t offset;            // byte offset within component struct
    size_t size;              // byte size of field
    std::string typeName;     // human-readable type name (e.g. "int32_t", "float", "RealmLevel")
    std::string description;  // human-readable description
    FieldConstraint constraint;

    // For Enum type: enum value names
    std::vector<std::pair<int64_t, std::string>> enumValues;

    // For nested Struct: name of referenced schema
    std::string structRef;
};

// ─── ComponentSchema ─────────────────────────────────────────────────────────

struct ComponentSchema {
    std::string name;         // component type name (e.g. "StatsComponent")
    std::string description;  // human-readable description
    std::vector<FieldDescriptor> fields;

    // ── Query helpers ────────────────────────────────────────────────────────

    const FieldDescriptor* getField(const std::string& fieldName) const {
        for (auto& f : fields) {
            if (f.name == fieldName) return &f;
        }
        return nullptr;
    }

    size_t getFieldCount() const { return fields.size(); }

    // ── Builder pattern ──────────────────────────────────────────────────────

    ComponentSchema& addField(const std::string& fieldName, FieldType fieldType,
                              size_t offset, size_t size,
                              const std::string& typeName = "",
                              const std::string& desc = "") {
        FieldDescriptor fd;
        fd.name = fieldName;
        fd.type = fieldType;
        fd.offset = offset;
        fd.size = size;
        fd.typeName = typeName;
        fd.description = desc;
        fields.push_back(std::move(fd));
        return *this;
    }

    ComponentSchema& addFieldWithConstraint(const std::string& fieldName, FieldType fieldType,
                                            size_t offset, size_t size,
                                            float min, float max,
                                            const std::string& desc = "") {
        FieldDescriptor fd;
        fd.name = fieldName;
        fd.type = fieldType;
        fd.offset = offset;
        fd.size = size;
        fd.description = desc;
        fd.constraint.min = min;
        fd.constraint.max = max;
        fd.constraint.required = true;
        fields.push_back(std::move(fd));
        return *this;
    }

    ComponentSchema& addEnumField(const std::string& fieldName, size_t offset, size_t size,
                                  const std::vector<std::pair<int64_t, std::string>>& values,
                                  const std::string& desc = "") {
        FieldDescriptor fd;
        fd.name = fieldName;
        fd.type = FieldType::Enum;
        fd.offset = offset;
        fd.size = size;
        fd.description = desc;
        fd.enumValues = values;
        fields.push_back(std::move(fd));
        return *this;
    }

    // ── JSON Schema export ───────────────────────────────────────────────────

    static std::string escapeJsonString(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:   out += c;      break;
            }
        }
        return out;
    }

    std::string toJsonSchema() const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "  \"type\": \"object\",\n";
        oss << "  \"title\": \"" << escapeJsonString(name) << "\",\n";
        if (!description.empty()) {
            oss << "  \"description\": \"" << escapeJsonString(description) << "\",\n";
        }

        // properties
        oss << "  \"properties\": {\n";
        for (size_t i = 0; i < fields.size(); ++i) {
            const auto& f = fields[i];
            oss << "    \"" << escapeJsonString(f.name) << "\": { ";
            oss << "\"type\": ";
            switch (f.type) {
                case FieldType::Bool:    oss << "\"boolean\""; break;
                case FieldType::Int8:
                case FieldType::Int16:
                case FieldType::Int32:
                case FieldType::Int64:   oss << "\"integer\""; break;
                case FieldType::Uint8:
                case FieldType::Uint16:
                case FieldType::Uint32:
                case FieldType::Uint64:  oss << "\"integer\""; break;
                case FieldType::Float32:
                case FieldType::Float64: oss << "\"number\"";  break;
                case FieldType::String:  oss << "\"string\"";  break;
                case FieldType::Enum: {
                    oss << "\"string\", \"enum\": [";
                    for (size_t j = 0; j < f.enumValues.size(); ++j) {
                        if (j > 0) oss << ", ";
                        oss << "\"" << escapeJsonString(f.enumValues[j].second) << "\"";
                    }
                    oss << "]";
                    break;
                }
                case FieldType::Struct:  oss << "\"object\""; break;
                case FieldType::Array:   oss << "\"array\"";  break;
                case FieldType::Map:     oss << "\"object\""; break;
            }
            // constraints
            if (f.type == FieldType::Float32 || f.type == FieldType::Float64) {
                if (f.constraint.min > -1e29f) {
                    oss << ", \"minimum\": " << f.constraint.min;
                }
                if (f.constraint.max < 1e29f) {
                    oss << ", \"maximum\": " << f.constraint.max;
                }
            }
            if (f.type == FieldType::Int8 || f.type == FieldType::Int16 ||
                f.type == FieldType::Int32 || f.type == FieldType::Int64 ||
                f.type == FieldType::Uint8 || f.type == FieldType::Uint16 ||
                f.type == FieldType::Uint32 || f.type == FieldType::Uint64) {
                if (f.constraint.min > -1e29f) {
                    oss << ", \"minimum\": " << static_cast<int64_t>(f.constraint.min);
                }
                if (f.constraint.max < 1e29f) {
                    oss << ", \"maximum\": " << static_cast<int64_t>(f.constraint.max);
                }
            }
            if (!f.description.empty()) {
                oss << ", \"description\": \"" << escapeJsonString(f.description) << "\"";
            }
            if (!f.structRef.empty()) {
                oss << ", \"$ref\": \"" << escapeJsonString(f.structRef) << "\"";
            }
            oss << " }";
            if (i + 1 < fields.size()) oss << ",";
            oss << "\n";
        }
        oss << "  },\n";

        // required array
        oss << "  \"required\": [";
        bool first = true;
        for (auto& f : fields) {
            if (f.constraint.required) {
                if (!first) oss << ", ";
                oss << "\"" << escapeJsonString(f.name) << "\"";
                first = false;
            }
        }
        oss << "]\n";

        oss << "}";
        return oss.str();
    }

    // ── Validation ─────────────────────────────────────────────────────────

    // Full implementation in SchemaValidator.h (needs complete ValidationResult)
    ValidationResult validate(const void* componentPtr) const;

    // ── Instance → JSON using schema ─────────────────────────────────────────

    std::string instanceToJson(const void* componentPtr) const {
        std::ostringstream oss;
        oss << "{";
        const uint8_t* base = static_cast<const uint8_t*>(componentPtr);

        for (size_t i = 0; i < fields.size(); ++i) {
            const auto& f = fields[i];
            const uint8_t* ptr = base + f.offset;

            oss << "\"" << escapeJsonString(f.name) << "\":";

            switch (f.type) {
                case FieldType::Bool: {
                    bool val;
                    std::memcpy(&val, ptr, sizeof(bool));
                    oss << (val ? "true" : "false");
                    break;
                }
                case FieldType::Int8: {
                    int8_t val;
                    std::memcpy(&val, ptr, sizeof(int8_t));
                    oss << static_cast<int>(val);
                    break;
                }
                case FieldType::Int16: {
                    int16_t val;
                    std::memcpy(&val, ptr, sizeof(int16_t));
                    oss << val;
                    break;
                }
                case FieldType::Int32: {
                    int32_t val;
                    std::memcpy(&val, ptr, sizeof(int32_t));
                    oss << val;
                    break;
                }
                case FieldType::Int64: {
                    int64_t val;
                    std::memcpy(&val, ptr, sizeof(int64_t));
                    oss << val;
                    break;
                }
                case FieldType::Uint8: {
                    uint8_t val;
                    std::memcpy(&val, ptr, sizeof(uint8_t));
                    oss << static_cast<unsigned int>(val);
                    break;
                }
                case FieldType::Uint16: {
                    uint16_t val;
                    std::memcpy(&val, ptr, sizeof(uint16_t));
                    oss << val;
                    break;
                }
                case FieldType::Uint32: {
                    uint32_t val;
                    std::memcpy(&val, ptr, sizeof(uint32_t));
                    oss << val;
                    break;
                }
                case FieldType::Uint64: {
                    uint64_t val;
                    std::memcpy(&val, ptr, sizeof(uint64_t));
                    oss << val;
                    break;
                }
                case FieldType::Float32: {
                    float val;
                    std::memcpy(&val, ptr, sizeof(float));
                    oss << std::setprecision(9) << val;
                    break;
                }
                case FieldType::Float64: {
                    double val;
                    std::memcpy(&val, ptr, sizeof(double));
                    oss << std::setprecision(17) << val;
                    break;
                }
                case FieldType::String: {
                    const std::string* str = reinterpret_cast<const std::string*>(ptr);
                    oss << "\"" << escapeJsonString(*str) << "\"";
                    break;
                }
                case FieldType::Enum: {
                    // Read the raw integer value and look up the name
                    int64_t intVal = 0;
                    std::memcpy(&intVal, ptr, f.size);
                    // Mask to match the backing size (handles signed/unsigned)
                    if (f.size == 1) intVal = static_cast<int64_t>(static_cast<int8_t>(intVal & 0xFF));
                    else if (f.size == 2) intVal = static_cast<int64_t>(static_cast<int16_t>(intVal & 0xFFFF));
                    else if (f.size == 4) intVal = static_cast<int64_t>(static_cast<int32_t>(intVal & 0xFFFFFFFF));

                    std::string enumName = "Unknown";
                    for (auto& ev : f.enumValues) {
                        if (ev.first == intVal) {
                            enumName = ev.second;
                            break;
                        }
                    }
                    oss << "\"" << escapeJsonString(enumName) << "\"";
                    break;
                }
                case FieldType::Struct:
                    oss << "\"{nested}\"";
                    break;
                case FieldType::Array:
                    oss << "\"{array}\"";
                    break;
                case FieldType::Map:
                    oss << "\"{map}\"";
                    break;
            }

            if (i + 1 < fields.size()) oss << ",";
        }

        oss << "}";
        return oss.str();
    }
};

// ─── SchemaRegistry — global singleton ───────────────────────────────────────

class SchemaRegistry {
public:
    static SchemaRegistry& instance() {
        static SchemaRegistry inst;
        return inst;
    }

    void registerSchema(const std::string& name, ComponentSchema schema) {
        schemas_[name] = std::move(schema);
    }

    const ComponentSchema* getSchema(const std::string& name) const {
        auto it = schemas_.find(name);
        return (it != schemas_.end()) ? &it->second : nullptr;
    }

    std::vector<std::string> getAllSchemaNames() const {
        std::vector<std::string> names;
        names.reserve(schemas_.size());
        for (auto& kv : schemas_) {
            names.push_back(kv.first);
        }
        return names;
    }

    size_t getSchemaCount() const {
        return schemas_.size();
    }

    // Export all schemas as a JSON object with schema names as keys
    std::string exportAllJsonSchemas() const {
        std::ostringstream oss;
        oss << "{\n";
        size_t idx = 0;
        for (auto& kv : schemas_) {
            oss << "  \"" << ComponentSchema::escapeJsonString(kv.first) << "\": "
                << kv.second.toJsonSchema();
            if (idx + 1 < schemas_.size()) oss << ",";
            oss << "\n";
            ++idx;
        }
        oss << "}";
        return oss.str();
    }

private:
    SchemaRegistry() = default;
    std::unordered_map<std::string, ComponentSchema> schemas_;
};

} // namespace ECS
