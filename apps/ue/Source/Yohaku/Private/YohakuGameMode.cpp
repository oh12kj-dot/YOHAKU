#include "YohakuGameMode.h"
#include "RoomController.h"
AYohakuGameMode::AYohakuGameMode()
{
    PlayerControllerClass = ARoomController::StaticClass();
    DefaultPawnClass = nullptr;
}
