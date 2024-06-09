#pragma once
#include "BaseView.h"

enum class EMessageType { ERROR_MSG, WARNING_MSG, INFO_MSG };

struct MessagesView : BaseView {
    MessagesView();

    bool begin() override;

    void render() override;

    static void add(std::string message, EMessageType type = EMessageType::INFO_MSG, const char* function_name = __builtin_FUNCTION(), const int line = __builtin_LINE());
};

REGISTER_VIEW(MessagesView);
