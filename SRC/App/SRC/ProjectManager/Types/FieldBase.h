#pragma once
#include <expected>
#include <string>

#include <fmt/format.h>

#include <imgui.h>

#include <nlohmann/json.hpp>

#include <Utils/IDAllocator.h>

#include <Defines.h>

enum class EType {
    // Those can also be used as types inside a field (it needs to build on something)
    I8,
    I16,
    I32,
    I64,
    U8,
    U16,
    U32,
    U64,
    F32,
    F64,

    END_SCALAR_POD_TYPES = F64,

    BEGIN_VEC_TYPES = END_SCALAR_POD_TYPES,
    VEC2,
    VEC3,
    VEC4,

    EULER, // Can have different order of components
    COLOR, // Can have different components that are either float or int and such

    QUATERION,

    MATRIX_ROW_MAJOR,
    MATRIX_COLUMN_MAJOR,

    // Can have different sizes, and either be row colum or column row
    END_VEC_TYPES = MATRIX_COLUMN_MAJOR,

    STRUCT_SPECIAL_TYPE = END_VEC_TYPES + 1,

    END_SPECIAL_TYPES = STRUCT_SPECIAL_TYPE,

    COUNT_ALL_TYPES = END_VEC_TYPES + 1,
    COUNT_POD_TYPES = END_SCALAR_POD_TYPES + 1,

    COUNT_VEC_TYPES = END_VEC_TYPES - BEGIN_VEC_TYPES + 1,
    COUNT_SPECIAL_TYPES = END_SPECIAL_TYPES - STRUCT_SPECIAL_TYPE + 1,
};


static std::array DEFAULT_POD_TYPE_NAMES = {"I8",
                                            "I16",
                                            "I32",
                                            "I64",
                                            "U8",
                                            "U16",
                                            "U32",
                                            "U64",
                                            "F32",
                                            "F64",
                                            "VEC2",
                                            "VEC3",
                                            "VEC4",
                                            "EULER",
                                            "COLOR",
                                            "QUATERION",
                                            "MATRIX_ROW_MAJOR",
                                            "MATRIX_COLUMN_MAJOR"};
static_assert(DEFAULT_POD_TYPE_NAMES.size() == static_cast<size_t>(EType::COUNT_ALL_TYPES));

static const char *to_string(const EType type) {
    const auto idx = static_cast<size_t>(type);
    if (idx > DEFAULT_POD_TYPE_NAMES.size()) {
        return "Unknown";
    }
    return DEFAULT_POD_TYPE_NAMES[idx];
}

union StructPodFieldUnion {
    int8_t i8_val;
    int16_t i16_val;
    int32_t i32_val;
    int64_t i64_val;
    uint8_t u8_val;
    uint16_t u16_val;
    uint32_t u32_val;
    uint64_t u64_val;
    float f32_val;
    double f64_val;
};

static size_t pod_type_size(const EType type) {
    switch (type) {
    case EType::I8:
    case EType::U8:
        return 1;
    case EType::I16:
    case EType::U16:
        return 2;
    case EType::I32:
    case EType::U32:
    case EType::F32:
        return 4;
    case EType::I64:
    case EType::U64:
    case EType::F64:
    default:
        return 8;
    }
}

enum class EEulerOrdering { XYZ, XZY, YXZ, YZX, ZXY, ZYX, MAX };

static const char *to_string(const EEulerOrdering ordering) {
    switch (ordering) {
    case EEulerOrdering::XYZ:
        return "XYZ";
    case EEulerOrdering::XZY:
        return "XZY";
    case EEulerOrdering::YXZ:
        return "YXZ";
    case EEulerOrdering::YZX:
        return "YZX";
    case EEulerOrdering::ZXY:
        return "ZXY";
    case EEulerOrdering::ZYX:
        return "ZYX";
    default:
        return "Unknown";
    }
}

enum class EColorType { RGB, RGBA, ARGB, BGRA, ABGR, MAX };

static const char *to_string(const EColorType ordering) {
    switch (ordering) {
    case EColorType::RGB:
        return "RGB";
    case EColorType::RGBA:
        return "RGBA";
    case EColorType::ARGB:
        return "ARGB";
    case EColorType::BGRA:
        return "BGRA";
    case EColorType::ABGR:
        return "ABGR";
    default:
        return "Unknown";
    }
}

enum class EFieldState { UNNAMED, NAMED, EDITING_NAME };

// TODO: Make a dynamic field type register, later for the lua api
enum class EFieldClassType { Base, PodField, VecField, StructField };

size_t default_field_size_by_current_process();

class Struct;
struct FieldBase {
    Struct *parent{};

    std::string name{};
    UniversalId id{};

    uintptr_t offset{};
    uintptr_t address{};

    EFieldState state = EFieldState::UNNAMED;
    EFieldClassType class_type = EFieldClassType::Base;
    EType base_type = EType::I64;

    ImVec2 render_bounds{};

    bool is_pointer_to = false;

    struct RenderData {
        std::vector<char> first_n_bytes{};
        std::vector<uint8_t> hex_data{};
        std::vector<char> edit_data{};

        float x_begin{};
        float x_offset{};

        std::string tmp_name{};

        bool hex_view_is_collapsed = true;

        void resize_buffers(const size_t new_size) {
            if (new_size != first_n_bytes.size()) {
                first_n_bytes.resize(new_size);
            }

            if (new_size != hex_data.size()) {
                hex_data.resize(new_size);
            }

            if (new_size * 3 != edit_data.size()) {
                edit_data.resize(new_size * 3);
            }
        }

    } render_data{};

    bool show_all_values = false;

    bool is_dummy = false;

    ALWAYS_INLINE void set_name(const std::string &_name) { name = render_data.tmp_name = _name; }

    ALWAYS_INLINE void set_default_name() { set_name(fmt::format("unnamed_{}", id)); }

    FieldBase() = default;
    FieldBase(const EFieldClassType _class_type, const EType _base_type) :
        class_type(_class_type), base_type(_base_type) {
    }

    // WARNING: Does not call set_default_name
    void init_base(Struct *parent_struct, UniversalId _id);

    virtual ~FieldBase() = default;

    virtual void update_render_bounds();

    virtual bool is_clipping_needed();

    void render_field_info();

    void render_field_hex_info(bool draw_first_bytes = true);

    void render_field_float_info();

    // Warning: Pushes a style var onto the imgui stack
    void render_field_name(bool is_named_mode, bool do_equal_sign = true, const std::function<void()>& after_type_name = nullptr);

    void render_begin(bool skip_inner_draw_calls = false);

    virtual void render(bool is_offscreen) = 0;

    virtual void render_context_menu_contents();

    void render_end(bool is_offscreen);

    // TODO: Sort this class
    void render_set_type_menu();

    virtual size_t memory_size() = 0;

    virtual std::string type_name();

    virtual void serialize(nlohmann::json &json);

    virtual void deserialize(const nlohmann::json &json);

    virtual void reset_field();

    virtual void copy_data(std::shared_ptr<FieldBase> to_field);

    virtual std::shared_ptr<FieldBase> clone() { return nullptr; }

    [[nodiscard]] uint8_t *data_in_memory_buffer() const;

    [[nodiscard]] ALWAYS_INLINE bool is_render_bounds_empty() const {
        return render_bounds.x == 0 && render_bounds.y == 0;
    }

    ALWAYS_INLINE void grab_render_bounds() {
        render_bounds = ImGui::GetItemRectSize();
        // render_bounds.y += 1.f;
    }

    template <typename... TArgs>
    std::string make_imgui_id(std::string_view other, TArgs &&...args);
};
