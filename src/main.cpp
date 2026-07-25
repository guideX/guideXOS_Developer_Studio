#include <guidexos/ui.h>

#include "developer_studio_models.h"

namespace {

using guidexos::developer_studio::InitialTargetProfile;
using guidexos::developer_studio::IsValidTargetProfile;
using guidexos::developer_studio::TargetProfile;

static const gx_rect kWindowRect = { 0, 0, 960, 640 };
static const gx_rect kCommandRect = { 0, 0, 960, 52 };
static const gx_rect kExplorerRect = { 0, 52, 224, 500 };
static const gx_rect kEditorRect = { 224, 52, 736, 390 };
static const gx_rect kOutputRect = { 0, 442, 960, 110 };
static const gx_rect kStatusRect = { 0, 552, 960, 88 };

static void clear_event(gx_event* event) {
    if (!event) return;
    event->size = 0;
    event->type = GX_EVENT_NONE;
    event->window = 0;
    event->param1 = 0;
    event->param2 = 0;
    event->param3 = 0;
    event->param4 = 0;
}

static void draw_shell(gx_app_context* ctx, gx_handle window, const TargetProfile& target) {
    gx_draw_panel(ctx, window, kWindowRect, 0x151B28u);
    gx_draw_panel(ctx, window, kCommandRect, 0x243451u);
    gx_draw_panel(ctx, window, kExplorerRect, 0x1D2636u);
    gx_draw_panel(ctx, window, kEditorRect, 0x111722u);
    gx_draw_panel(ctx, window, kOutputRect, 0x202A36u);
    gx_draw_panel(ctx, window, kStatusRect, 0x243451u);

    gx_draw_label(ctx, window, 18, 31, "guideXOS Developer Studio");
    gx_draw_label(ctx, window, 300, 31, "File    Edit    View    Build    Help");

    gx_draw_label(ctx, window, 18, 82, "WORKSPACE");
    gx_draw_label(ctx, window, 18, 116, "No workspace open");
    gx_draw_label(ctx, window, 18, 158, "PROJECTS");
    gx_draw_label(ctx, window, 18, 192, "Open a project to see files");
    gx_draw_label(ctx, window, 18, 234, "TARGET PROFILES");
    gx_draw_label(ctx, window, 18, 268, "AMD64 Hosted Native");
    gx_draw_label(ctx, window, 18, 322, "Phase 1 shell");

    gx_draw_label(ctx, window, 250, 88, "Welcome to guideXOS Developer Studio");
    gx_draw_label(ctx, window, 250, 124, "Native development environment shell");
    gx_draw_label(ctx, window, 250, 174, "This initial window proves the guideXOS App Model path.");
    gx_draw_label(ctx, window, 250, 204, "Workspace, project, target profile, and capability models are ready.");
    gx_draw_label(ctx, window, 250, 234, "The editor surface is intentionally read-only in Phase 1.");
    gx_draw_label(ctx, window, 250, 286, "Next: connect a real workspace to build and runner interfaces.");
    gx_draw_label(ctx, window, 250, 336, "No compiler, debugger, language server, or designer is active yet.");

    gx_draw_label(ctx, window, 18, 474, "OUTPUT / DIAGNOSTICS");
    gx_draw_label(ctx, window, 18, 504, "[registration] App Model manifest registered");
    gx_draw_label(ctx, window, 18, 530, "[render] Native window shell rendered successfully");

    gx_draw_label(ctx, window, 18, 580, "Target: guideXOS AMD64 Hosted — Native — Experimental IDE integration");
    gx_draw_label(ctx, window, 18, 612, target.runner);
}

} // namespace

extern "C" gx_result GX_CALL gx_main(gx_app_context* ctx) {
    if (!ctx || !ctx->host) return GX_ERROR_INVALID_ARGUMENT;
    if (!ctx->host->get_api_version || !ctx->host->log || !ctx->host->request_window) return GX_ERROR_UNSUPPORTED;

    const TargetProfile& target = InitialTargetProfile();
    if (!IsValidTargetProfile(target)) return GX_ERROR_FAILED;

    ctx->host->log(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER application_construction=PASS");
    ctx->host->log(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER target_profile=guidexos.amd64.hosted.native maturity=experimental");

    gx_handle window = 0;
    gx_result windowResult = GX_ERROR_FAILED;
    if (ctx->host->request_window_ex) {
        windowResult = ctx->host->request_window_ex(
            ctx,
            "guideXOS Developer Studio",
            kWindowRect.width,
            kWindowRect.height,
            GX_WINDOW_FLAG_RESIZABLE | GX_WINDOW_FLAG_CENTERED,
            &window);
    } else {
        windowResult = ctx->host->request_window(ctx, "guideXOS Developer Studio", kWindowRect.width, kWindowRect.height, &window);
    }
    if (windowResult != GX_OK) {
        ctx->host->log(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER main_window_creation=FAIL");
        return windowResult;
    }

    ctx->host->log(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER main_window_creation=PASS");
    draw_shell(ctx, window, target);
    ctx->host->log(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER initial_render=PASS");

    if (ctx->host->poll_event) {
        for (;;) {
            gx_event event;
            clear_event(&event);
            gx_result eventResult = ctx->host->poll_event(ctx, &event, 500);
            if (eventResult == GX_OK && event.window == window) {
                if (gx_event_is_paint(&event)) {
                    draw_shell(ctx, window, target);
                } else if (gx_event_is_close(&event)) {
                    ctx->host->log(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS");
                    break;
                } else if (gx_event_is_escape_down(&event)) {
                    ctx->host->log(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS reason=escape");
                    break;
                }
            } else if (eventResult != GX_OK && eventResult != GX_ERROR_TIMEOUT) {
                ctx->host->log(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER event_loop=FAIL");
                break;
            }
        }
    } else if (ctx->host->wait_for_close) {
        gx_result waitResult = ctx->host->wait_for_close(ctx, window, 300000);
        if (waitResult == GX_OK) {
            ctx->host->log(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS");
        } else {
            ctx->host->log(ctx, "GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=FAIL");
        }
    }

    if (ctx->host->exit) return ctx->host->exit(ctx, GX_OK);
    return GX_OK;
}
