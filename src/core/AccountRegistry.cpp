#include "core/AccountRegistry.h"

#include "services/Gw2ApiClient.h"
#include "core/Locale.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace cm {

namespace {

// Short alias so the wrapped literals stay readable.
using cm::Locale::T;


bool HasPermission(const std::vector<std::string>& permissions, const char* perm) {
    return std::find(permissions.begin(), permissions.end(), perm) != permissions.end();
}

RegisteredAccount AccountFromJson(const nlohmann::json& j) {
    RegisteredAccount account;
    account.tokenId = j.value("tokenId", "");
    account.keyName = j.value("keyName", "");
    account.accountName = j.value("accountName", "");
    account.apiKey = j.value("apiKey", "");
    if (j.contains("permissions") && j["permissions"].is_array()) {
        for (const auto& p : j["permissions"]) {
            if (p.is_string()) account.permissions.push_back(p.get<std::string>());
        }
    }
    return account;
}

nlohmann::json AccountToJson(const RegisteredAccount& account) {
    return {{"tokenId", account.tokenId},
            {"keyName", account.keyName},
            {"accountName", account.accountName},
            {"apiKey", account.apiKey},
            {"permissions", account.permissions}};
}

}  // namespace

void AccountRegistry::Load(const std::string& path) {
    accounts_.clear();
    activeTokenId_.reset();
    std::ifstream in(path);
    if (!in.is_open()) return;
    nlohmann::json j;
    in >> j;
    if (j.contains("activeTokenId") && j["activeTokenId"].is_string()) {
        activeTokenId_ = j["activeTokenId"].get<std::string>();
    }
    if (!j.contains("accounts") || !j["accounts"].is_array()) return;
    for (const auto& entry : j["accounts"]) {
        auto account = AccountFromJson(entry);
        if (!account.tokenId.empty() && !account.apiKey.empty()) {
            accounts_.push_back(std::move(account));
        }
    }
}

void AccountRegistry::Save(const std::string& path) const {
    nlohmann::json j;
    j["accounts"] = nlohmann::json::array();
    for (const auto& account : accounts_) {
        j["accounts"].push_back(AccountToJson(account));
    }
    if (activeTokenId_) {
        j["activeTokenId"] = *activeTokenId_;
    }
    std::ofstream out(path);
    if (!out.is_open()) return;
    out << j.dump(2);
}

std::optional<std::string> AccountRegistry::ActiveApiKey() const {
    if (!activeTokenId_) return std::nullopt;
    for (const auto& account : accounts_) {
        if (account.tokenId == *activeTokenId_) return account.apiKey;
    }
    return std::nullopt;
}

std::optional<std::string> AccountRegistry::ActiveAccountName() const {
    if (!activeTokenId_) return std::nullopt;
    for (const auto& account : accounts_) {
        if (account.tokenId == *activeTokenId_) return account.accountName;
    }
    return std::nullopt;
}

void AccountRegistry::SetActiveTokenId(const std::string& tokenId) {
    activeTokenId_ = tokenId;
}

RegisterKeyResult AccountRegistry::RegisterKey(Gw2ApiClient& client, const std::string& apiKey) {
    RegisterKeyResult result;
    if (apiKey.empty()) {
        result.message = T("API key is empty.");
        return result;
    }
    client.SetApiKey(apiKey);
    const auto tokenInfo = client.FetchTokenInfo();
    if (!tokenInfo.valid) {
        result.message = T("API key validation failed.");
        return result;
    }
    if (!HasPermission(tokenInfo.permissions, "account")) {
        result.message = T("API key must have account permission.");
        return result;
    }
    const auto accountName = client.FetchAccountName();
    if (!accountName || accountName->empty()) {
        result.message = T("Failed to fetch account name.");
        return result;
    }

    RegisteredAccount account;
    account.tokenId = tokenInfo.id;
    account.keyName = tokenInfo.name;
    account.accountName = *accountName;
    account.apiKey = apiKey;
    account.permissions = tokenInfo.permissions;

    accounts_.erase(std::remove_if(accounts_.begin(), accounts_.end(),
                                   [&](const RegisteredAccount& existing) {
                                       return existing.tokenId == account.tokenId;
                                   }),
                    accounts_.end());
    accounts_.push_back(account);
    activeTokenId_ = account.tokenId;

    result.success = true;
    // Translated as a whole sentence so the account name lands in the right
    // place for the active language.
    char registered[256];
    std::snprintf(registered, sizeof(registered), T("Registered %s."), account.accountName.c_str());
    result.message = registered;
    result.account = std::move(account);
    return result;
}

bool AccountRegistry::RemoveKey(const std::string& tokenId) {
    const auto before = accounts_.size();
    accounts_.erase(std::remove_if(accounts_.begin(), accounts_.end(),
                                   [&](const RegisteredAccount& account) {
                                       return account.tokenId == tokenId;
                                   }),
                    accounts_.end());
    if (activeTokenId_ && *activeTokenId_ == tokenId) {
        activeTokenId_ = accounts_.empty() ? std::nullopt : std::optional<std::string>(accounts_.front().tokenId);
    }
    return accounts_.size() != before;
}

}  // namespace cm
