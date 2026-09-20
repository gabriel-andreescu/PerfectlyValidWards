using System.Drawing;
using Mutagen.Bethesda;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Plugins.Allocators;
using Mutagen.Bethesda.Plugins.Records;
using Mutagen.Bethesda.Skyrim;

namespace PerfectlyValidWards.Generator;

internal static class Program
{
    private static void Main(string[] args)
    {
        var output = Path.GetFullPath(args[0]);
        var mod = new SkyrimMod("PerfectlyValidWards.esp", SkyrimRelease.SkyrimSE);
        mod.ModHeader.Author = "DEFAULT";
        mod.ModHeader.Flags = SkyrimModHeader.HeaderFlag.Small;
        mod.ModHeader.INTV = 1;
        using var allocator = new TextFileFormKeyAllocator(mod, args[1])
        {
            CommitOnDispose = false,
        };
        mod.SetAllocator(allocator);
        BuildPlugin(mod);

        Directory.CreateDirectory(Path.Combine(output, "main"));
        mod.WriteToBinary(Path.Combine(output, "main", "PerfectlyValidWards.esp"));

        Mcm.AddQuest(mod);
        Directory.CreateDirectory(Path.Combine(output, "mcm"));
        mod.WriteToBinary(Path.Combine(output, "mcm", "PerfectlyValidWards.esp"));

        allocator.Commit();
    }

    private static void BuildPlugin(SkyrimMod mod)
    {
        mod.ModHeader.MasterReferences.Add(
            new MasterReference { Master = ModKey.FromNameAndExtension("Skyrim.esm"), FileSize = 0 }
        );

        var impact = mod.Impacts.AddNew("_GZ_WPNArrowVsWardImpact");
        impact.AngleThreshold = 15;
        impact.Duration = 0.25F;
        impact.Orientation = Impact.OrientationType.SurfaceNormal;
        impact.PlacementRadius = 16;
        impact.Result = Impact.ResultType.Bounce;
        impact.SoundLevel = SoundLevel.Normal;
        impact.Decal = new Decal
        {
            Color = Color.FromArgb(0, 255, 255, 255),
            Depth = 32,
            MaxHeight = 32,
            MaxWidth = 32,
            MinHeight = 8,
            MinWidth = 8,
            ParallaxPasses = 4,
            ParallaxScale = 1,
            Shininess = 4,
            Unknown = 15709,
        };

        var topic = new DialogTopic(FormKey.Factory("013EBB:Skyrim.esm"), SkyrimRelease.SkyrimSE)
        {
            Category = DialogTopic.CategoryEnum.Combat,
            Priority = 50,
            Quest = { FormKey = FormKey.Factory("013EB3:Skyrim.esm") },
            Subtype = DialogTopic.SubtypeEnum.Hit,
            SubtypeName = "HIT_",
            Timestamp = 13016,
            Version2 = 3,
        };

        var response = new DialogResponses(mod, "_GZ_PlaceholderGruntTopic")
        {
            FavorLevel = FavorLevel.None,
            Flags = new DialogResponseFlags { Flags = DialogResponses.Flag.Random },
        };
        response.Responses.Add(
            new DialogResponse
            {
                ResponseNumber = 1,
                Text = "Agh!",
                Emotion = Emotion.Puzzled,
                EmotionValue = 50,
                Flags = DialogResponse.Flag.UseEmotionAnimation,
                ScriptNotes = " reacting to being struck by a weapon or arrow, short grunt of pain",
            }
        );
        response.Conditions.Add(
            new ConditionFloat
            {
                CompareOperator = CompareOperator.LessThanOrEqualTo,
                ComparisonValue = 0,
                Data = new GetActorValueConditionData
                {
                    ActorValue = ActorValue.WardPower,
                    RunOnType = Condition.RunOnType.Subject,
                },
            }
        );
        topic.Responses.Add(response);
        mod.DialogTopics.Add(topic);
    }
}
