#include "ui/OptionsApiTab.h"

#include "core/AccountRegistry.h"
#include "core/Branding.h"
#include "services/Gw2ApiClient.h"
#include "ui/OptionsUiKit.h"
#include "core/Locale.h"

#include <imgui.h>
#include <shellapi.h>

namespace cm {
namespace OptionsApiTab {
namespace {

// Short alias so the wrapped literals stay readable.
using cm::Locale::T;


char g_apiKeyBuffer[128] = "";
std::string g_statusMessage;

void OpenExternalUrl(const char* url) {
    if (!url || url[0] == '\0') return;
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

}  // namespace

void RenderApiKeyManagement(AppState& state) {
    using namespace OptionsUiKit;
    ImGui::TextWrapped(
        T("Required to share marker sets with the community library. The key needs account permission."));
    ImGui::InputText(T("API key"), g_apiKeyBuffer, sizeof(g_apiKeyBuffer), ImGuiInputTextFlags_Password);
    if (ImGui::Button(T("Register key"))) {
        Gw2ApiClient client;
        const auto result = state.accountRegistry.RegisterKey(client, g_apiKeyBuffer);
        g_statusMessage = result.message;
        if (result.success) {
            state.accountRegistry.Save(state.apiAccountsPath());
            g_apiKeyBuffer[0] = '\0';
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(T("Clear active key"))) {
        if (const auto active = state.accountRegistry.ActiveApiKey()) {
            for (const auto& account : state.accountRegistry.Accounts()) {
                if (account.apiKey == *active) {
                    state.accountRegistry.RemoveKey(account.tokenId);
                    break;
                }
            }
            state.accountRegistry.Save(state.apiAccountsPath());
            g_statusMessage = T("Removed active API key.");
        }
    }
    if (!g_statusMessage.empty()) {
        ImGui::TextWrapped("%s", g_statusMessage.c_str());
    }
    if (const auto accountName = state.accountRegistry.ActiveAccountName()) {
        ImGui::Text(T("Active account: %s"), accountName->c_str());
    } else {
        ImGui::TextDisabled(T("No API key registered."));
    }
}

void Render(AppState& state) {
    using namespace OptionsUiKit;
    SectionHeading(T("GW2 API key"));
    RenderApiKeyManagement(state);

    SectionHeading(T("Share with community"));
    ImGui::TextWrapped(
        T("Use Share with community in the Community Library tab when an API key is registered."));
    if (ImGui::Button(T("Open web library"))) {
        const std::string url = state.moduleManifest.Get().Absolute(state.moduleManifest.Get().libraryUrl);
        OpenExternalUrl(url.c_str());
    }
}

}  // namespace OptionsApiTab
}  // namespace cm
