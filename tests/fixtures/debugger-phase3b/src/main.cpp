#include <guidexos/ui.h>

namespace {
static volatile int g_debugProof = 0;
static gx_handle g_window = 0;
}

extern "C" gx_result GX_CALL gx_main(gx_app_context* ctx) {
    if (!ctx || !ctx->host || !ctx->host->request_window || !ctx->host->log) return GX_ERROR_INVALID_ARGUMENT;










    g_debugProof = 1;
    gx_result result = GX_ERROR_FAILED;
    if (ctx->host->request_window_ex)
        result = ctx->host->request_window_ex(ctx, "Debugger Phase 3B Fixture", 640, 360,
                                              GX_WINDOW_FLAG_RESIZABLE | GX_WINDOW_FLAG_CENTERED, &g_window);
    else
        result = ctx->host->request_window(ctx, "Debugger Phase 3B Fixture", 640, 360, &g_window);
    if (result != GX_OK) return result;
    if (ctx->host->draw_rect)
        ctx->host->draw_rect(ctx, g_window, 0, 0, 640, 360, 0x172238u);
    if (ctx->host->draw_text)
        ctx->host->draw_text(ctx, g_window, 24, 40, "Debugger Phase 3B Fixture");
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
