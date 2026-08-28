#include "NebulaForgeAISettings.h"
#include "NebulaForgeAIModels.h"

UNebulaForgeAISettings::UNebulaForgeAISettings()
{
    // Defaults intentionally conservative; mutating permissions stay off
    // until explicitly enabled in Settings (see plan section 3.3).
}

const FNebulaAIProviderProfile* UNebulaForgeAISettings::FindProfile(const FString& ProfileId) const
{
    return const_cast<UNebulaForgeAISettings*>(this)->FindMutableProfile(ProfileId);
}

FNebulaAIProviderProfile* UNebulaForgeAISettings::FindMutableProfile(const FString& ProfileId)
{
    for (FNebulaAIProviderProfile& Profile : ProviderProfiles)
    {
        if (Profile.Id == ProfileId)
        {
            return &Profile;
        }
    }
    return nullptr;
}

const FNebulaAIProviderProfile* UNebulaForgeAISettings::GetActiveProfile() const
{
    if (ActiveProfileId.IsEmpty())
    {
        return nullptr;
    }
    return FindProfile(ActiveProfileId);
}
