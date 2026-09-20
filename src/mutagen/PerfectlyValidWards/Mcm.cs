using BMK.Mutagen.Skyrim;
using Mutagen.Bethesda.Skyrim;

namespace PerfectlyValidWards.Generator;

internal static class Mcm
{
    public static void AddQuest(SkyrimMod mod)
    {
        McmQuest.Add(
            mod,
            new McmQuestOptions
            {
                EditorId = "PerfectlyValidWards_MCMQuest",
                DisplayName = "Perfectly Valid Wards",
                ConfigScriptName = "PerfectlyValidWards_MCM",
                ModName = "Perfectly Valid Wards",
            }
        );
    }
}
