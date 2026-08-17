#include <guidexos/ui.h>

namespace {
struct Point {
    int x;
    int y;
};

struct Rectangle {
    Point origin;
    int width;
    int height;
};

struct Node {
    int value;
    Node* next;
};

volatile int g_sink = 0;
gx_handle g_window = 0;
}

static __attribute__((noinline)) int debugLoop(int callerValue) {
    Rectangle rect{};
    rect.origin = Point{10, 20};
    rect.width = 100;
    rect.height = 50;
    Rectangle* rectPtr = &rect;
    int values[4] = {1, 2, 3, 4};
    Node node{};
    node.value = 77;
    node.next = &node;
    int counter = 0;
    int total = 0;
    for (;;) {
        volatile int sample = rect.origin.x + values[2] + callerValue + counter; // Phase 15 breakpoint.
        total += sample;
        if (counter >= 2) break;
        ++counter;
    }
    g_sink = total + rectPtr->origin.x + node.value;
    return g_sink;
}

static __attribute__((noinline)) int debugCaller() {
    int outerValue = 90;
    return debugLoop(outerValue);
}

extern "C" gx_result GX_CALL gx_main(gx_app_context* ctx) {
    if (!ctx || !ctx->host || !ctx->host->request_window || !ctx->host->log)
        return GX_ERROR_INVALID_ARGUMENT;
    gx_result result = ctx->host->request_window_ex ?
        ctx->host->request_window_ex(ctx, "Debugger Phase 15 Fixture", 640, 360,
                                     GX_WINDOW_FLAG_RESIZABLE | GX_WINDOW_FLAG_CENTERED, &g_window) :
        ctx->host->request_window(ctx, "Debugger Phase 15 Fixture", 640, 360, &g_window);
    if (result != GX_OK) return result;
    const int resultValue = debugCaller();
    if (ctx->host->draw_rect) ctx->host->draw_rect(ctx, g_window, 0, 0, 640, 360, 0x172238u);
    if (ctx->host->draw_text) ctx->host->draw_text(ctx, g_window, 24, 40, "Debugger Phase 15 Fixture");
    if (ctx->host->log) ctx->host->log(ctx, resultValue == 399 ? "conditional values ready" : "conditional values failed");
    if (ctx->host->poll_event) {
        bool running = true;
        while (running) {
            gx_event event{};
            event.size = static_cast<uint32_t>(sizeof(event));
            const gx_result poll = ctx->host->poll_event(ctx, &event, 500);
            if (poll == GX_OK && event.window == g_window &&
                (event.type == GX_EVENT_WINDOW_CLOSE ||
                 (event.type == GX_EVENT_KEY && event.param1 == GX_KEY_ESCAPE && event.param2 == GX_KEY_ACTION_DOWN)))
                running = false;
            else if (poll != GX_OK && poll != GX_ERROR_TIMEOUT) return poll;
        }
    } else if (ctx->host->wait_for_close) {
        result = ctx->host->wait_for_close(ctx, g_window, 300000);
        if (result != GX_OK && result != GX_ERROR_TIMEOUT) return result;
    }
    return ctx->host->exit ? ctx->host->exit(ctx, GX_OK) : GX_OK;
}
