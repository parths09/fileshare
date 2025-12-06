#include <GLFW/glfw3.h>
#define GL_SILENCE_DEPRECATION
#include <GL/gl.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <iostream>
#include <chrono>

#include <asio.hpp>
#include<cstring>
#include <csignal>

#include "ImGuiFileDialog.h"  // Your copied file
#include "cancellation.h"
#include "utils.h"
#include "receiver.h"
#include "sender.h"
#include "discovery.h"

using namespace std;

std::atomic<bool> cancel_requested(false);
std::atomic<bool> connection_accepted(false);
bool receiver_active = false;

void signal_handler(int signum) {
    std::cout << "\nTransfer interrupted. Cleaning up...\n";
    if(connection_accepted==false) exit(1);
    cancel_requested = true;
}

static void glfw_error_callback(int, const char* description) {
    std::cerr << "GLFW Error: " << description << std::endl;
}

static bool show_demo_window = true;
static bool show_file_dialog = false;
static ImGuiFileDialog fileDialog;
static std::string selected_file;
static std::vector<Peer> peers = {{"192.168.1.100", "TestPeer1", 12345}, {"192.168.1.101", "TestPeer2", 12345}};




enum class AppMode { SELECT, SENDER, RECEIVER };
AppMode current_mode = AppMode::SELECT;
// std::string selected_file;
std::string save_directory = "/home/parth/Downloads";
// std::vector<Peer> peers;
// bool show_file_dialog = false;
bool show_save_dialog = false;
bool scanning_peers = false;

int main() {

    // Setup signal handling for Ctrl+C
    std::signal(SIGINT, signal_handler);

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return -1;

    // GL 3.0 + GLSL 130
    // glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    // glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(900, 700, "LocalShare GUI", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    static std::unique_ptr<std::thread> beacon_thread;
    static asio::io_context io_context;

    // popups for sent
    bool show_success_popup = false;
    bool show_error_popup = false;

    // Main loop
    // Main loop (REPLACE your entire while loop):
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // === MODERN STYLE SETUP (Add once before main loop) ===
        static bool style_initialized = false;
        if (!style_initialized) {
            ImGuiStyle& style = ImGui::GetStyle();
            style.WindowRounding = 12.0f;
            style.FrameRounding = 8.0f;
            style.PopupRounding = 8.0f;
            style.ScrollbarRounding = 12.0f;
            style.GrabRounding = 8.0f;
            style.TabRounding = 8.0f;
            style.WindowTitleAlign = ImVec2(0.5f, 0.5f);  // Center title
            
            style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
            style.Colors[ImGuiCol_TitleBg] = ImVec4(0.20f, 0.25f, 0.35f, 1.00f);
            style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.25f, 0.30f, 0.45f, 1.00f);
            style.Colors[ImGuiCol_Button] = ImVec4(0.25f, 0.35f, 0.60f, 1.00f);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.45f, 0.70f, 1.00f);
            style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.30f, 0.55f, 1.00f);
            style.Colors[ImGuiCol_Header] = ImVec4(0.25f, 0.35f, 0.60f, 1.00f);
            style.Colors[ImGuiCol_Separator] = ImVec4(0.40f, 0.45f, 0.55f, 0.60f);
            style_initialized = true;
        }

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        ImVec2 center = ImVec2(display_w / 2.0f, display_h / 2.0f);

        // === MODE SELECTION (CENTERED) ===
        if (current_mode == AppMode::SELECT) {
            ImGui::SetNextWindowPos({center.x - 350, center.y - 250}, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize({500, 300}, ImGuiCond_FirstUseEver);
            ImGui::Begin("LocalShare", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Choose operating mode:");
            ImGui::Separator();
            ImGui::Spacing();
            
            if (ImGui::Button("📤 Sender Mode", {220, 70})) {
                current_mode = AppMode::SENDER;
                peers.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("📥 Receiver Mode", {220, 70})) {
                current_mode = AppMode::RECEIVER;
            }
            
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Sender: Select file → Discover peers → Send");
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Receiver: Choose save folder → Wait for files");
            
            ImGui::End();
        }

        // === SENDER MODE (CENTERED) ===
        else if (current_mode == AppMode::SENDER) {
            ImGui::SetNextWindowPos({center.x - 350, center.y - 250}, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize({700, 500}, ImGuiCond_FirstUseEver);
            ImGui::Begin("Sender Mode", nullptr);
            
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Selected file: %s", selected_file.c_str());
            if (ImGui::Button("📁 Browse Files", {150, 0})) {
                show_file_dialog = true;
            }
            
            if (show_file_dialog) {
                fileDialog.OpenDialog("SelectFile", "Choose File",".*,.txt,.pdf,.zip",
                IGFD::FileDialogConfig{});
                // Display dialog
                if (fileDialog.Display("SelectFile", ImGuiWindowFlags_NoCollapse, ImVec2(700, 400)) ||
        fileDialog.Display("SelectFolder", ImGuiWindowFlags_NoCollapse, ImVec2(700, 400))) {
                    if (fileDialog.IsOk()) {
                        selected_file = fileDialog.GetFilePathName();
                        fileDialog.Close();
                        show_file_dialog = false;
                    }
                }
            }

            
            ImGui::SeparatorText("Discovered Peers");
            if (ImGui::Button("🔍 Discover Peers", {150, 0}) && !selected_file.empty()) {
                scanning_peers = true;
                std::cout << "Scanning for peers..." << std::endl;
                peers = {{"192.168.1.100", "TestPeer1", 12345}, {"192.168.1.101", "TestPeer2", 12345}};
                peers = discover_peers(io_context);
                if(peers.empty()){
                    cout<<"No peers found"<<endl;
                }
                else{
                    cout<<"Peers found"<<endl;
                }
                scanning_peers = false;
            }

            // Success popup
            if (ImGui::BeginPopupModal("Sent Success", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("File sent successfully!");
                ImGui::Spacing();
                if (ImGui::Button("OK", {120, 0})) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            // Success popup
            if (ImGui::BeginPopupModal("File Transfer failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("File transfered failed!");
                ImGui::Spacing();
                if (ImGui::Button("OK", {120, 0})) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            
            if (!peers.empty()) {
                ImGui::BeginChild("PeersList", ImVec2(0, 200), true);
                for (size_t i = 0; i < peers.size(); ++i) {
                    auto& p = peers[i];
                    std::string label = p.hostname + " (" + p.ip + ":" + std::to_string(p.tcp_port) + ")";
                    ImGui::Text("%s", label.c_str());
                    ImGui::SameLine(350);
                    if (ImGui::Button(("Send##" + std::to_string(i)).c_str(), {80, 25})) {
                        std::cout << "Sending " << selected_file << " to " << p.ip << ":" << p.tcp_port << std::endl;
                        int success = Sender2(p.ip,std::to_string(p.tcp_port),selected_file);
                        if(success==0){
                            show_success_popup = true;
                        }
                        else{
                            show_error_popup = true;
                        }
                    }
                }
                ImGui::EndChild();
            }
            if(show_success_popup){
                show_success_popup=false;
                ImGui::OpenPopup("Sent Success");
            }
            if(show_error_popup){
                show_error_popup=false;
                ImGui::OpenPopup("File Transfer failed");
            }

           
            
            if (ImGui::Button("← Back", {100, 0})) {
                current_mode = AppMode::SELECT;
            }
            ImGui::End();
        }

        // === RECEIVER MODE (CENTERED) ===
        else if (current_mode == AppMode::RECEIVER) {
            ImGui::SetNextWindowPos({center.x - 300, center.y - 200}, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize({700, 700}, ImGuiCond_FirstUseEver);
            ImGui::Begin("Receiver Mode", nullptr);


            // START BEACON THREAD on mode enter
            receiver_active = true;
            static bool beacon_started = false;
            if (!beacon_started) {
                beacon_started = true;
                beacon_thread = std::make_unique<std::thread>([&]() {
                    // beacon_thread_func(io_context,TCP_PORT, get_hostname());
                    beacon_thread_func(TCP_PORT, get_hostname());
                });
                std::cout << "✅ Beacon started - announcing on UDP 5353" << std::endl;
            }
            
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Save to: %s", save_directory.c_str());
            if (ImGui::Button("📁 Choose Save Folder", {160, 0})) {
                show_save_dialog = !show_save_dialog;
            }
            
            if (show_save_dialog) {
                fileDialog.OpenDialog("SelectFolder", "Choose Save Folder", 
                            "",IGFD::FileDialogConfig{});


                if (fileDialog.Display("SelectFolder", ImGuiWindowFlags_NoCollapse, ImVec2(700, 400))) {
                    if (fileDialog.IsOk()) {
                        save_directory = fileDialog.GetFilePathName();
                        fileDialog.Close();
                        show_save_dialog=false;
                    }
                }
            }
            if (ImGui::Button("📁 Receive", {160, 0}) && !save_directory.empty()) {
                int success = Receiver2(save_directory);
                if (success == 0) {
                    ImGui::OpenPopup("Receive Success");
                }
                else{
                    ImGui::OpenPopup("File Transfer failed");
                }
            }
            // Success popup
            if (ImGui::BeginPopupModal("Receive Success", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Files received successfully!");
                ImGui::Spacing();
                if (ImGui::Button("OK", {120, 0})) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            // Success popup
            if (ImGui::BeginPopupModal("File Transfer failed", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("File transfered failed!");
                ImGui::Spacing();
                if (ImGui::Button("OK", {120, 0})) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }


            

            ImGui::SeparatorText("Status");
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), "✅ Listening on port %d", TCP_PORT);
            ImGui::Text("📁 Save directory: %s", save_directory.c_str());
            ImGui::ProgressBar(0.0f, ImVec2(-1, 20), "Waiting for files...");
            
            if (ImGui::Button("← Back", {100, 0})) {
                receiver_active = false;  // Signal stop
                current_mode = AppMode::SELECT;
                beacon_started = false;

                if (beacon_thread) {
                    beacon_thread->join();  // Wait for clean shutdown
                    beacon_thread.reset();
                }
            }
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.08f, 0.10f, 0.15f, 1.00f);  // Modern dark blue-gray
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
