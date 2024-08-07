
#if defined _DEBUG
#define mop_debug
#endif

#define use_letterbox

#include "mo_basic.h"

#include "mo_platform.h"
#include "mo_memory_arena.h"
#include "mo_audio.h"

#define STBTT_assert(x) STBTT_assert_wrapper(x)
#include "stb_truetype.h"

#include "stb_image.h"

#define moui_gl_implementation
#include "mo_ui.h"

#define mop_implementation
#include "mo_platform.h"

#define moa_implementation
#include "mo_audio.h"

#include "mo_math.h"

void STBTT_assert_wrapper(b8 condition)
{
    assert(condition);
}

#define moma_implementation
#include "mo_memory_arena.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define moui_implementation
#include "mo_ui.h"

#define mos_implementation
#include "mo_string.h"

#include "mo_random_pcg.h"

typedef struct
{
    moma_arena         memory;
    mop_window         window;

    moui_default_state ui;
    moui_simple_font   font;

#ifdef use_letterbox
    box2               ui_letterbox;
#endif

    random_pcg random;

    usize memory_asset_offset;
    usize memory_reset_offset;
} program_state;

program_state global_program;

mop_hot_update_signature;

#if 1
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#else
int main(int argument_count, char *arguments[])
#endif
{
    mop_platform         platform = {0};
    mop_hot_reload_state hot_reload_state = {0};
    program_state *program = &global_program;

    mop_init(&platform);
    mop_window_init(&platform, &program->window, "template", 1280, 720);

    moui_default_init(&program->ui);
    moui_default_window ui_window = moui_get_default_platform_window(&program->ui, &platform, program->window);
    moui_default_window_init(&program->ui, &ui_window);

    moma_create(&program->memory, &platform, ((usize) 2) << 30);

    program->random = random_from_win23();

    // allocate persistant assets here

    program->memory_asset_offset = program->memory.used_count;

    // WARNING: all memory allocation from this line on is frame temporary

    program->ui.base.renderer.quad_count = 1 << 20;
    program->ui.base.renderer.vertex_count = program->ui.base.renderer.quad_count * 6;
    program->ui.base.renderer.texture_count = 64;
    program->ui.base.renderer.command_count = 1024;

    program->memory_reset_offset = program->memory.used_count;
    moui_resize_buffers(&program->ui.base, &program->memory);

#if !mo_enable_hot_reloading
    b8 did_reload = true;
#endif

    mop_window_show(&platform, &program->window);
    mop_update_delta_seconds(&platform);

    while (true)
    {
        mop_handle_messages(&platform);

    #if mo_enable_hot_reloading

        b8 did_reload = mop_hot_reload(&platform, &hot_reload_state, s("hot"));
        hot_reload_state.hot_update(&platform, sl(u8_array) { (u8 *) program, sizeof(*program) }, did_reload);

    #else

        mop_hot_update(&platform, sl(u8_array) { (u8 *) program, sizeof(*program) }, did_reload);
        did_reload = false;

    #endif

        // give update a chance to catch and override quit
        if (platform.do_quit)
            break;

        moui_default_render_begin(&program->ui, &ui_window);

    #ifdef use_letterbox
        moui_default_render_prepare_execute_viewport(&program->ui, program->ui_letterbox);
    #else
        moui_default_render_prepare_execute(&program->ui);
    #endif

        moui_execute(&program->ui.base);

        program->memory.used_count = program->memory_reset_offset;
        moui_resize_buffers(&program->ui.base, &program->memory);

        moui_default_render_end(&program->ui, &ui_window, true);
    }

    return 0;
}

mop_hot_update_signature
{
    program_state *program = (program_state *) data.base;
    moui_state *ui = &program->ui.base;
    moma_arena *tmemory = &program->memory;
    f32 delta_seconds = platform->delta_seconds;

    #if defined mop_debug
    if (did_reload)
    {

    }
    #endif

    mop_window_info window_info = mop_window_get_info(platform, &program->window);

    if (platform->do_quit || window_info.requested_close)
    {
        platform->do_quit = true;
    }

    vec2 ui_size = { (f32) window_info.size.x, (f32) window_info.size.y };
    vec2 mouse_position = { (f32) window_info.relative_mouse_position.x, (f32) window_info.relative_mouse_position.y };

    const f32 target_width_over_height = 16.0f / 9;
    f32 width_over_height = ui_size.x / ui_size.y;

#ifdef use_letterbox
    box2 letterbox;
    if (width_over_height > target_width_over_height)
    {
        f32 width = ui_size.y * target_width_over_height;
        letterbox.min.x = floorf((ui_size.x - width) * 0.5f);
        letterbox.max.x = ui_size.x - letterbox.min.x;
        letterbox.min.y = 0;
        letterbox.max.y = ui_size.y;

        ui_size.x = width;
    }
    else
    {
        f32 height = ui_size.x / target_width_over_height;
        letterbox.min.x = 0;
        letterbox.max.x = ui_size.x;
        letterbox.min.y = floorf((ui_size.y - height) * 0.5f);
        letterbox.max.y = ui_size.y - letterbox.min.y;

        ui_size.y = height;
    }

    program->ui_letterbox = letterbox;

    mouse_position = vec2_sub(mouse_position, letterbox.min);
#endif

    const f32 target_height = 1080;
    f32 ui_scale = ui_size.y / target_height;

    // auto resize font depending on resolution
    {
        s32 pixel_height = ceilf(32 * ui_scale);

        if (program->font.height != pixel_height)
        {
            s32 thickness = ceilf(4 * ui_scale);

            moui_destroy_font(&program->font);
            moui_load_outlined_font_file(&program->font, ui, platform, tmemory, s("C:/windows/fonts/consola.ttf"), 1024, 1024, pixel_height, ' ', 96, thickness, moui_rgba_white, moui_rgba_black);
        }
    }

    moui_simple_font *font = &program->font;

    moui_frame(ui, ui_size, mouse_position, platform->keys[mop_key_mouse_left].is_active << 0);

    box2 ui_box = { 0, 0, ui_size.x, ui_size.y };
    moui_box(ui, 0, moui_make_quad_colors(sl(rgba) { 0, 0.5f, 0.5f, 1 }), ui_box);

    moui_text_cursor print_cursor = moui_text_cursor_at_top(font, sl(vec2) { 0, ui_size.y });
    moui_printf(ui, font, 0, sl(rgba) { 1.0f, 1.0f, 1.0f, 1.0f }, &print_cursor, "fps: %f\n", 1.0f / platform->delta_seconds);
}
