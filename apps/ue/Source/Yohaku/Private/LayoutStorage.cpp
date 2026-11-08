#include "LayoutStorage.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif
namespace Yohaku
{
FString FLayoutStorage::Path()
{
    FString Root;
    if (!FParse::Value(FCommandLine::Get(), TEXT("YohakuLayoutRoot="), Root))
        Root = FString(FPlatformProcess::UserSettingsDir()) / TEXT("Yohaku/Layouts");
    return FPaths::ConvertRelativePathToFull(Root / TEXT("current.yohaku.json"));
}
bool FLayoutStorage::Save(const FString &Json, FString &Error)
{
    const FString Dest = Path(), Temp = Dest + TEXT(".") + FGuid::NewGuid().ToString() + TEXT(".tmp");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Dest), true);
    if (!FFileHelper::SaveStringToFile(Json, *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        Error = TEXT("一時ファイルへ保存できません");
        return false;
    }
    bool Ok = false;
#if PLATFORM_WINDOWS
    Ok = ::MoveFileExW(*Temp, *Dest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    Ok = IFileManager::Get().Move(*Dest, *Temp, true, true) == COPY_OK;
#endif
    if (!Ok)
    {
        IFileManager::Get().Delete(*Temp);
        Error = TEXT("保存ファイルを置換できません。元ファイルは保持しました");
        return false;
    }
    return true;
}
bool FLayoutStorage::Load(FString &Json, FString &Error)
{
    const FString Source = Path();
    const int64 Size = IFileManager::Get().FileSize(*Source);
    if (Size < 0 || Size > 4 * 1024 * 1024 || !FFileHelper::LoadFileToString(Json, *Source))
    {
        Error = TEXT("保存ファイルがないか、読み込めません");
        return false;
    }
    return true;
}
} // namespace Yohaku
