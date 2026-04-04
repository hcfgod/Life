#include "TestSupport.h"

using namespace Life::Tests;

TEST_CASE("InputSystem tracks raw device state and action phases")
{
    Life::InputSystem input;

    auto actions = Life::CreateRef<Life::InputActionAsset>();
    Life::InputActionMap& gameplayMap = actions->AddMap("Gameplay");

    Life::InputAction& quitAction = gameplayMap.AddAction("Quit", Life::InputActionValueType::Button);
    quitAction.AddBinding(Life::KeyboardButtonBinding{ Life::KeyCodes::Space });

    Life::InputAction& lookAction = gameplayMap.AddAction("Look", Life::InputActionValueType::Axis2D);
    lookAction.AddBinding(Life::MouseDeltaBinding{ 1.0f, false });

    input.SetProjectActionAsset(actions);

    SDL_Event keyDown{};
    keyDown.type = SDL_EVENT_KEY_DOWN;
    keyDown.key.scancode = SDL_SCANCODE_SPACE;
    keyDown.key.down = true;
    keyDown.key.repeat = false;

    SDL_Event mouseMotion{};
    mouseMotion.type = SDL_EVENT_MOUSE_MOTION;
    mouseMotion.motion.x = 100.0f;
    mouseMotion.motion.y = 200.0f;
    mouseMotion.motion.xrel = 3.0f;
    mouseMotion.motion.yrel = -4.0f;

    input.OnSdlEvent(keyDown);
    input.OnSdlEvent(mouseMotion);
    input.UpdateActions();

    CHECK(input.IsKeyDown(Life::KeyCodes::Space));
    CHECK(input.WasKeyPressedThisFrame(Life::KeyCodes::Space));
    CHECK(input.WasActionStartedThisFrame("Gameplay", "Quit"));
    CHECK(input.IsActionPressed("Gameplay", "Quit"));

    const Life::InputVector2 mousePosition = input.GetMousePosition();
    CHECK(mousePosition.x == doctest::Approx(100.0f));
    CHECK(mousePosition.y == doctest::Approx(200.0f));

    const Life::InputVector2 lookDelta = input.ReadActionAxis2D("Gameplay", "Look");
    CHECK(lookDelta.x == doctest::Approx(3.0f));
    CHECK(lookDelta.y == doctest::Approx(-4.0f));

    input.EndFrame();
    CHECK_FALSE(input.WasKeyPressedThisFrame(Life::KeyCodes::Space));
    CHECK(input.GetMouseDelta().x == doctest::Approx(0.0f));
    CHECK(input.GetMouseDelta().y == doctest::Approx(0.0f));

    input.UpdateActions();
    CHECK(input.WasActionPerformedThisFrame("Gameplay", "Quit"));
    CHECK(input.ReadActionAxis2D("Gameplay", "Look").x == doctest::Approx(0.0f));
    CHECK(input.ReadActionAxis2D("Gameplay", "Look").y == doctest::Approx(0.0f));

    SDL_Event keyUp{};
    keyUp.type = SDL_EVENT_KEY_UP;
    keyUp.key.scancode = SDL_SCANCODE_SPACE;
    keyUp.key.down = false;
    keyUp.key.repeat = false;

    input.OnSdlEvent(keyUp);
    input.UpdateActions();

    CHECK_FALSE(input.IsKeyDown(Life::KeyCodes::Space));
    CHECK(input.WasKeyReleasedThisFrame(Life::KeyCodes::Space));
    CHECK(input.WasActionCanceledThisFrame("Gameplay", "Quit"));
}

TEST_CASE("InputSystem honors synthetic mouse warp suppression")
{
    Life::InputSystem input;
    input.NotifyMouseWarped();

    SDL_Event mouseMotion{};
    mouseMotion.type = SDL_EVENT_MOUSE_MOTION;
    mouseMotion.motion.x = 400.0f;
    mouseMotion.motion.y = 300.0f;
    mouseMotion.motion.xrel = 12.0f;
    mouseMotion.motion.yrel = -8.0f;

    input.OnSdlEvent(mouseMotion);

    CHECK(input.GetMousePosition().x == doctest::Approx(400.0f));
    CHECK(input.GetMousePosition().y == doctest::Approx(300.0f));
    CHECK(input.GetMouseDelta().x == doctest::Approx(0.0f));
    CHECK(input.GetMouseDelta().y == doctest::Approx(0.0f));
}

TEST_CASE("InputActionAssetSerializer saves and loads input actions")
{
    auto actions = Life::CreateRef<Life::InputActionAsset>();
    Life::InputActionMap& gameplayMap = actions->AddMap("Gameplay");
    Life::InputAction& moveAction = gameplayMap.AddAction("Move", Life::InputActionValueType::Axis2D);

    Life::KeyboardAxis2DBinding movementKeys{};
    movementKeys.Up = Life::KeyCodes::W;
    movementKeys.Down = Life::KeyCodes::S;
    movementKeys.Left = Life::KeyCodes::A;
    movementKeys.Right = Life::KeyCodes::D;
    moveAction.AddBinding(movementKeys);

    const std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "life_input_actions_test.json";
    const auto saveResult = Life::InputActionAssetSerializer::SaveToFile(*actions, tempPath.string());
    REQUIRE(saveResult.IsSuccess());

    const auto loadResult = Life::InputActionAssetSerializer::LoadFromFile(tempPath.string());
    REQUIRE(loadResult.IsSuccess());

    Life::Ref<Life::InputActionAsset> loadedActions = loadResult.GetValue();
    REQUIRE(loadedActions != nullptr);

    const Life::InputActionMap* loadedMap = loadedActions->FindMap("Gameplay");
    REQUIRE(loadedMap != nullptr);

    const Life::InputAction* loadedMoveAction = loadedMap->FindAction("Move");
    REQUIRE(loadedMoveAction != nullptr);
    CHECK(loadedMoveAction->GetValueType() == Life::InputActionValueType::Axis2D);
    CHECK(loadedMoveAction->GetBindingCount() == 1);

    std::error_code cleanupError;
    std::filesystem::remove(tempPath, cleanupError);
}
