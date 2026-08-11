using System.Reflection;
using System.Text;

namespace CorumIntegrationInstaller;

internal static class PrivacyContent
{
    private const string DownloadNoticeResource = "CorumIntegrationInstaller.Privacy.DownloadNotice.md";
    private const string PrivacyPolicyResource = "CorumIntegrationInstaller.Privacy.PrivacyPolicy.md";

    public static string Load()
    {
        var assembly = Assembly.GetExecutingAssembly();
        return string.Join(
            Environment.NewLine + Environment.NewLine + new string('─', 72) + Environment.NewLine + Environment.NewLine,
            ReadResource(assembly, DownloadNoticeResource),
            ReadResource(assembly, PrivacyPolicyResource));
    }

    private static string ReadResource(Assembly assembly, string resourceName)
    {
        using var stream = assembly.GetManifestResourceStream(resourceName)
                           ?? throw new InvalidOperationException($"Missing embedded privacy resource: {resourceName}");
        using var reader = new StreamReader(stream, new UTF8Encoding(false), true);
        return reader.ReadToEnd();
    }
}
