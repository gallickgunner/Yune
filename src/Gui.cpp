/******************************************************************************
 *  This file is part of Yune".
 *
 *  Copyright (C) 2018 by Umair Ahmed and Syed Moiz Hussain.
 *
 *  "Yune" is a framework for a Physically Based Renderer. It's aimed at young
 *  researchers trying to implement Physically Based Rendering techniques.
 *
 *  "Yune" is a free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  "Yune" is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#include "Gui.h"

namespace yune
{
    nanogui::Screen* Gui::screen = NULL;
    Gui::Gui()
    {
        //ctor
        start_btn = stop_btn = save_img_btn = false;
    }

    Gui::~Gui()
    {
        if(screen)
            delete screen;
    }

    void Gui::setupWidgets(GLFWwindow* window)
    {
        screen = new nanogui::Screen();
        screen->initialize(window, false, true);

        int fb_width, fb_height;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);

        //We don't need to save pointers to nanogui's child widgets such as labels and buttons. They get de-allocated when parent window destroys.
        //Set Options Panel
        panel = new nanogui::Window(screen, "Menu");
        panel->setPosition(Eigen::Vector2i(0, 0));

        nanogui::GroupLayout* panel_layout = new nanogui::GroupLayout();
        panel_layout->setMargin(5);
        panel_layout->setSpacing(1);
        panel->setLayout(panel_layout);

        //Set custom theme, transparency etc.
        panel_theme = new nanogui::Theme(screen->nvgContext());
        panel_theme->mWindowFillFocused = nanogui::Color(73, 170);
        panel_theme->mWindowFillUnfocused = nanogui::Color(70, 170);
        panel_theme->mWindowCornerRadius = 8;
        panel_theme->mWindowPopup = nanogui::Color(73, 190);
        panel_theme->mButtonGradientTopUnfocused = nanogui::Color(73, 145);
        panel_theme->mButtonGradientBotUnfocused = nanogui::Color(73, 145);
        panel_theme->mButtonGradientTopFocused = nanogui::Color(50, 200);
        panel_theme->mButtonGradientBotFocused = nanogui::Color(50, 200);
        panel_theme->mWindowHeaderGradientTop = nanogui::Color(45, 140);
        panel_theme->mWindowHeaderGradientBot = nanogui::Color(45, 140);
        panel->setTheme(panel_theme);

        // Setup buttons on the panel.
        {
            //Setup Start button.
            nanogui::Button* start_btn = new nanogui::Button(panel, "Start");
            start_btn->setTooltip("Start the Rendering process..");
            start_btn->setCallback([&](){ this->start_btn = true; });

            //Setup Stop button. Stop rendering process or close the window altogether, depends on the programmer. Useful to close program when in fullscreen mode.
            nanogui::Button* stop_btn = new nanogui::Button(panel, "Stop");
            stop_btn->setTooltip("Stop the Rendering process..");
            stop_btn->setCallback([&](){ this->stop_btn = true; });

            //Setup Save button. Allow users to save the currently drawn image.
            nanogui::Button* save_btn = new nanogui::Button(panel, "Save", ENTYPO_ICON_EXPORT);
            save_btn->setTooltip("Quick Save an image..");
            save_btn->setCallback([&](){this->save_img_btn = true; });

            // Info button for controls.
            nanogui::Button* info_btn = new nanogui::Button(panel, "Info");
            info_btn->setTooltip("Information on controls..");
            info_btn->setCallback( [&]()
                                  { auto md = new nanogui::MessageDialog(this->screen,
                                                               nanogui::MessageDialog::Type::Information,
                                                               "Controls Information",
                                                               "Hold Space and move mouse to rotate camera. Move with arrow keys. Press \"Stop\" to close window and \"Save\" to save the current frame.");
                                    md->setTheme(this->panel_theme);
                                  });

            //Setup Options popup button. Expose simple options such as sample size and GI check.
            nanogui::PopupButton* popup_btn = new nanogui::PopupButton(panel, "Options");
            nanogui::Popup* popup = popup_btn->popup();

            nanogui::GridLayout* popup_layout = new nanogui::GridLayout(
                                                          nanogui::Orientation::Horizontal, 2,
                                                          nanogui::Alignment::Middle, 15, 5);
            popup_layout->setColAlignment( { nanogui::Alignment::Maximum, nanogui::Alignment::Fill } );
            popup_layout->setSpacing(0, 10);
            popup->setLayout(popup_layout);
            popup->setTheme(panel_theme);

            // Setup widgets inside Popup panel
            {
                // Setup a GI check box.
                {
                    new nanogui::Label(popup, "Global Illumination :", "sans-bold");
                    gi_check = new nanogui::CheckBox(popup, "");
                    gi_check->setChecked(true);
                }

                // Setup a HDR check box. This determines whether to save images in HDR/LDR.
                {
                    new nanogui::Label(popup, "Save HDR Image :", "sans-bold");
                    hdr_check = new nanogui::CheckBox(popup, "");
                    hdr_check->setChecked(false);
                }

                // Int box for Samples at which to save an image.
                {
                    new nanogui::Label(popup, "Save Image at Samples :", "sans-bold");
                    save_img_samples = new nanogui::IntBox<int>(popup, 0);
                    save_img_samples->setEditable(true);
                    save_img_samples->setSpinnable(true);
                    save_img_samples->setFontSize(16);
                    save_img_samples->setFixedSize(Eigen::Vector2i(100,20));
                    save_img_samples->setMinMaxValues(0, std::numeric_limits<int>::max());
                }

                // Text box for Save Image name.
                {
                    new nanogui::Label(popup, "Save Image Filename :", "sans-bold");
                    save_img_name = new nanogui::TextBox(popup, "-");
                    save_img_name->setEditable(true);
                    save_img_name->setFontSize(16);
                    save_img_name->setFixedSize(Eigen::Vector2i(100,20));
                }
            }
        }

        //Setup Profiler window.
        profiler_window = new nanogui::Window(screen, "Profiling");
        nanogui::GridLayout* profiler_layout = new nanogui::GridLayout(
                                                       nanogui::Orientation::Horizontal, 2,
                                                       nanogui::Alignment::Middle, 10, 5);

        profiler_window->setLayout(profiler_layout);
        profiler_window->setTheme(panel_theme);


        //Setup text box to show FPS.
        nanogui::Label* label = new nanogui::Label(profiler_window, "FPS :", "sans-bold");
        label->setFontSize(18);
        profiler.fps = new nanogui::IntBox<int>(profiler_window, 0);
        profiler.fps->setEditable(false);
        profiler.fps->setValue(0);
        profiler.fps->setFontSize(16);
        profiler.fps->setFixedSize(Eigen::Vector2i(100,20));

        //Setup text box to show ms per frame.
        label = new nanogui::Label(profiler_window, "ms/Frame :", "sans-bold");
        label->setFontSize(18);
        profiler.mspf_avg = new nanogui::FloatBox<float>(profiler_window, 0.0f);
        profiler.mspf_avg->setUnits("ms");
        profiler.mspf_avg->setValue(0.0);
        profiler.mspf_avg->setEditable(false);
        profiler.mspf_avg->setFontSize(16);
        profiler.mspf_avg->setFixedSize(Eigen::Vector2i(100,20));
        profiler.mspf_avg->setTooltip("Time taken by 1 frame in ms. Averaged over 1s interval. Slightly more than ms/kernel due to overhead of blit operations.");

        //Setup text box to show rendering kernel execution time averaged over 1 second.
        label = new nanogui::Label(profiler_window, "ms/RK :", "sans-bold");
        label->setFontSize(18);
        profiler.msrk_avg = new nanogui::FloatBox<float>(profiler_window, 0.0f);
        profiler.msrk_avg->setUnits("ms");
        profiler.msrk_avg->setValue(0.0);
        profiler.msrk_avg->setEditable(false);
        profiler.msrk_avg->setFontSize(16);
        profiler.msrk_avg->setFixedSize(Eigen::Vector2i(100,20));
        profiler.msrk_avg->setTooltip("Rendering Kernel execution time in ms. It's averaged over 1 second interval.");

        //Setup text box to show postprocessing kernel execution time averaged over 1 second.
        label = new nanogui::Label(profiler_window, "ms/PPK :", "sans-bold");
        label->setFontSize(18);
        profiler.msppk_avg = new nanogui::FloatBox<float>(profiler_window, 0.0f);
        profiler.msppk_avg->setUnits("ms");
        profiler.msppk_avg->setValue(0.0);
        profiler.msppk_avg->setEditable(false);
        profiler.msppk_avg->setFontSize(16);
        profiler.msppk_avg->setFixedSize(Eigen::Vector2i(100,20));
        profiler.msppk_avg->setTooltip("Rendering Kernel execution time in ms. It's averaged over 1 second interval.");

        //Setup text box to show render time.
        label = new nanogui::Label(profiler_window, "Render Time :", "sans-bold");
        label->setFontSize(18);
        profiler.render_time = new nanogui::IntBox<int>(profiler_window, 0.0f);
        profiler.render_time->setUnits("sec");
        profiler.render_time->setValue(0.0);
        profiler.render_time->setEditable(false);
        profiler.render_time->setFontSize(16);
        profiler.render_time->setFixedSize(Eigen::Vector2i(100,20));
        profiler.render_time->setTooltip("Total Time passed since the start of rendering.");

        //Setup text box to show total samples per pixel.
        label = new nanogui::Label(profiler_window, "SPP :", "sans-bold");
        label->setFontSize(18);
        profiler.tot_samples = new nanogui::IntBox<int>(profiler_window, 0);
        profiler.tot_samples->setUnits("spp");
        profiler.tot_samples->setEditable(false);
        profiler.tot_samples->setValue(0);
        profiler.tot_samples->setFontSize(16);
        profiler.tot_samples->setFixedSize(Eigen::Vector2i(100,20));
        profiler.tot_samples->setTooltip("Total Samples per pixel..");

        screen->performLayout();
        profiler_window->setPosition(Eigen::Vector2i(fb_width - profiler_window->width(), 0));

    }
}
