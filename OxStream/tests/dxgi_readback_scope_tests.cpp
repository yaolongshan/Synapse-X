#include "DxgiReadbackScope.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
enum class Event { Unmap, Release };

struct Texture {};
struct State {
    Texture texture;
    bool frameHeld = true;
    bool mapped = false;
    bool invalidCleanup = false;
    std::array<Event, 8> events{};
    std::size_t count = 0;

    void Record(Event event) noexcept {
        if (count == events.size()) {
            invalidCleanup = true;
            return;
        }
        events[count++] = event;
    }
};

struct Duplication {
    State& state;
    long ReleaseFrame() noexcept {
        if (!state.frameHeld || state.mapped) state.invalidCleanup = true;
        state.frameHeld = false;
        state.Record(Event::Release);
        return 0;
    }
};

struct Context {
    State& state;
    void Unmap(Texture* texture, unsigned subresource) noexcept {
        if (!state.mapped || texture != &state.texture || subresource != 0)
            state.invalidCleanup = true;
        state.mapped = false;
        state.Record(Event::Unmap);
    }
};

using Scope = SynapseX::detail::DxgiReadbackScope<Duplication, Context, Texture>;
static_assert(!std::is_copy_constructible_v<Scope>);
static_assert(!std::is_copy_assignable_v<Scope>);
static_assert(!std::is_move_constructible_v<Scope>);
static_assert(!std::is_move_assignable_v<Scope>);

void ExpectReleased(const State& state, std::initializer_list<Event> expected) {
    if (state.frameHeld || state.mapped || state.invalidCleanup || state.count != expected.size())
        throw std::runtime_error("Readback resources leaked or were cleaned up more than once");
    std::size_t index = 0;
    for (const auto event : expected) {
        if (state.events[index++] != event)
            throw std::runtime_error("Mapped staging must be unmapped before releasing the held frame");
    }
}

void ExitBeforeSuccessfulMapReleasesOnlyFrame() {
    // QI/Map failure: no successful Map means Unmap must not be called.
    State state;
    Duplication duplication{state};
    Context context{state};
    {
        SynapseX::detail::DxgiReadbackScope scope(&duplication, &context, &state.texture);
    }
    ExpectReleased(state, {Event::Release});
}

struct PackingFailure {};

void PackingExceptionUnmapsThenReleasesAndPropagates() {
    State state;
    Duplication duplication{state};
    Context context{state};
    bool propagated = false;
    try {
        Scope scope(&duplication, &context, &state.texture);
        state.mapped = true;  // A successful driver Map.
        scope.MarkMapped();
        throw PackingFailure{};
    } catch (const PackingFailure&) {
        propagated = true;
    }
    if (!propagated) throw std::runtime_error("Packing exception was swallowed");
    ExpectReleased(state, {Event::Unmap, Event::Release});
}

void ExplicitCleanupIsNotRepeatedByDestructor() {
    State state;
    Duplication duplication{state};
    Context context{state};
    {
        Scope scope(&duplication, &context, &state.texture);
        state.mapped = true;
        scope.MarkMapped();
        scope.Unmap();
        scope.ReleaseFrame();
        scope.Unmap();
        scope.ReleaseFrame();
    }
    ExpectReleased(state, {Event::Unmap, Event::Release});
}

void EarlyReleaseIsNotRepeatedAfterMapSuccessOrFailure() {
    for (const bool mapSucceeded : {false, true}) {
        State state;
        Duplication duplication{state};
        Context context{state};
        {
            Scope scope(&duplication, &context, &state.texture);
            scope.ReleaseFrame();  // Host still releases before mapping its own staging texture.
            if (mapSucceeded) {
                state.mapped = true;
                scope.MarkMapped();
            }
        }
        if (mapSucceeded) ExpectReleased(state, {Event::Release, Event::Unmap});
        else ExpectReleased(state, {Event::Release});
    }
}

void ConsecutiveFramesLeaveNoOwnershipBetweenCalls() {
    State state;
    Duplication duplication{state};
    Context context{state};
    for (int frame = 0; frame < 2; ++frame) {
        if (frame != 0 && (state.frameHeld || state.mapped))
            throw std::runtime_error("Previous frame still owns DXGI resources");
        state.frameHeld = true;  // Next successful Acquire.
        Scope scope(&duplication, &context, &state.texture);
        state.mapped = true;
        scope.MarkMapped();
    }
    ExpectReleased(state, {Event::Unmap, Event::Release, Event::Unmap, Event::Release});
}
} // namespace

int main() {
    try {
        ExitBeforeSuccessfulMapReleasesOnlyFrame();
        PackingExceptionUnmapsThenReleasesAndPropagates();
        ExplicitCleanupIsNotRepeatedByDestructor();
        EarlyReleaseIsNotRepeatedAfterMapSuccessOrFailure();
        ConsecutiveFramesLeaveNoOwnershipBetweenCalls();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
