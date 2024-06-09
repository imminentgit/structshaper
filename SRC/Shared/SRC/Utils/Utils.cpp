#ifdef STRUCTSHAPER_IS_WINDOWS
    #include <Windows.h>
    #include <ImageHlp.h>
extern "C" char *__unDName(char*, const char*, int, void*, void*, int);
#elif STRUCTSHAPER_IS_LINUX
#include <codecvt>
#include <locale>
#endif

#include "Utils.h"

namespace Utils {
    std::string wstring_to_string(const std::wstring_view in) {
#ifdef STRUCTSHAPER_IS_LINUX
        return
            std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(in.data(), in.data() + in.size());
#elif STRUCTSHAPER_IS_WINDOWS
        auto len = WideCharToMultiByte(CP_UTF8, 0, in.data(), static_cast<int>(in.size()), nullptr, 0, nullptr,
                                       nullptr);

        if (!len) {
            return {};
        }

        std::string ret{};
        ret.resize(len);

        len = WideCharToMultiByte(CP_UTF8, 0, in.data(), static_cast<int>(in.size()), ret.data(),
                                  static_cast<int>(ret.size()), nullptr, nullptr);

        if (!len) {
            return {};
        }

        if (len < static_cast<int>(ret.size())) {
            ret.resize(len);
        }

        return ret;
#endif
    }

    std::wstring string_to_wstring(const std::string_view in) {
#ifdef STRUCTSHAPER_IS_LINUX
        return
            std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(in.data(), in.data() + in.size());
#elif STRUCTSHAPER_IS_WINDOWS
        auto len = MultiByteToWideChar(CP_UTF8, 0, in.data(), static_cast<int>(in.size()), nullptr, 0);

        if (!len) {
            return {};
        }

        std::wstring ret{};
        ret.resize(len);

        len = MultiByteToWideChar(CP_UTF8, 0, in.data(), static_cast<int>(in.size()), ret.data(),
                                  static_cast<int>(ret.size()));
        if (!len) {
            return {};
        }

        if (len < static_cast<int>(ret.size())) {
            ret.resize(len);
        }

        return ret;
#endif
    }

    EVerifyReason verify_name(const std::string_view name, const size_t min_len) {
        if (name.empty()) {
            return EVerifyReason::EMPTY;
        }

        if (name.size() < min_len) {
            return EVerifyReason::TOO_SHORT;
        }

        for (const auto c : name) {
            if ((!std::isalnum(c) && c != '_') || c == ' ') {
                return EVerifyReason::INVALID_CHARACTERS;
            }
        }

        return EVerifyReason::OK;
    }

    // TODO: Implment demangling locally
    std::string demangle_name(const std::string_view name) {
#ifdef STRUCTSHAPER_IS_WINDOWS
        std::string ret{};
        ret.resize(name.length() + 128);

        auto size = UnDecorateSymbolName(name.data(), ret.data(), ret.size(), UNDNAME_NAME_ONLY);
        if (!size) {
            return {};
        }

        ret.resize(strlen(ret.c_str()));
        return ret;
#endif
        return {};
    }
}