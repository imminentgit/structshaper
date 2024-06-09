#include "ProjectBackgroundView.h"

#include "Popups/CreateStructPopup.h"

ProjectBackgroundView::ProjectBackgroundView() : BaseBackgroundView("ProjectBackgroundView",
                                                                    EViewType::BACKGROUND_VIEW) {
    should_render = false;
    invisible_covering_window = true;
}

void ProjectBackgroundView::render() {
    using namespace ImGui::Helpers;

    aligned_avail(CENTER_BOTH, [] {
        ImGui::TextUnformatted("This project is empty.");
    });

    if (aligned_avail(CENTER_HORIZONTAL, [] {
        return link_text("Do you want to create a new struct?");
    })) {
        PopupView::show_popup(popup_CreateStructPopup);
    }
}
