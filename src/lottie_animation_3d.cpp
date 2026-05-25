#include "lottie_animation_3d.h"

#include "lottie_common.h"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <thorvg.h>

#include <cmath>
#include <cstring>
#include <thread>

using namespace godot;

void LottieAnimation3D::_bind_methods() {
    // Playback control.
    ClassDB::bind_method(D_METHOD("play"), &LottieAnimation3D::play);
    ClassDB::bind_method(D_METHOD("stop"), &LottieAnimation3D::stop);
    ClassDB::bind_method(D_METHOD("pause"), &LottieAnimation3D::pause);
    ClassDB::bind_method(D_METHOD("seek", "frame"), &LottieAnimation3D::seek);
    ClassDB::bind_method(D_METHOD("set_frame", "frame"), &LottieAnimation3D::set_frame);
    ClassDB::bind_method(D_METHOD("get_frame"), &LottieAnimation3D::get_frame);

    // Properties.
    ClassDB::bind_method(D_METHOD("set_animation_path", "path"), &LottieAnimation3D::set_animation_path);
    ClassDB::bind_method(D_METHOD("get_animation_path"), &LottieAnimation3D::get_animation_path);
    ClassDB::bind_method(D_METHOD("set_selected_dotlottie_animation", "id_or_path"), &LottieAnimation3D::set_selected_dotlottie_animation);
    ClassDB::bind_method(D_METHOD("get_selected_dotlottie_animation"), &LottieAnimation3D::get_selected_dotlottie_animation);

    ClassDB::bind_method(D_METHOD("set_playing", "playing"), &LottieAnimation3D::set_playing);
    ClassDB::bind_method(D_METHOD("is_playing"), &LottieAnimation3D::is_playing);

    ClassDB::bind_method(D_METHOD("set_looping", "looping"), &LottieAnimation3D::set_looping);
    ClassDB::bind_method(D_METHOD("is_looping"), &LottieAnimation3D::is_looping);

    ClassDB::bind_method(D_METHOD("set_autoplay", "autoplay"), &LottieAnimation3D::set_autoplay);
    ClassDB::bind_method(D_METHOD("is_autoplay"), &LottieAnimation3D::is_autoplay);

    ClassDB::bind_method(D_METHOD("set_speed", "speed"), &LottieAnimation3D::set_speed);
    ClassDB::bind_method(D_METHOD("get_speed"), &LottieAnimation3D::get_speed);

    ClassDB::bind_method(D_METHOD("set_render_size", "size"), &LottieAnimation3D::set_render_size);
    ClassDB::bind_method(D_METHOD("get_render_size"), &LottieAnimation3D::get_render_size);

    // Read-only info.
    ClassDB::bind_method(D_METHOD("get_duration"), &LottieAnimation3D::get_duration);
    ClassDB::bind_method(D_METHOD("get_total_frames"), &LottieAnimation3D::get_total_frames);
    ClassDB::bind_method(D_METHOD("get_animation_size"), &LottieAnimation3D::get_animation_size);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "animation_path", PROPERTY_HINT_FILE, "*.json,*.lottie"),
            "set_animation_path", "get_animation_path");
    // Selection helper for .lottie bundles; stored, not shown in the inspector.
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "dotlottie/selected_animation", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_NO_EDITOR),
            "set_selected_dotlottie_animation", "get_selected_dotlottie_animation");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playing"), "set_playing", "is_playing");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autoplay"), "set_autoplay", "is_autoplay");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "looping"), "set_looping", "is_looping");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed", PROPERTY_HINT_RANGE, "0.0,10.0,0.01"),
            "set_speed", "get_speed");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "render_size"), "set_render_size", "get_render_size");

    ADD_SIGNAL(MethodInfo("animation_finished"));
    ADD_SIGNAL(MethodInfo("frame_changed", PropertyInfo(Variant::FLOAT, "frame")));
    ADD_SIGNAL(MethodInfo("animation_loaded", PropertyInfo(Variant::BOOL, "success")));
}

LottieAnimation3D::LottieAnimation3D() {
    animation_path = "";
    selected_dotlottie_animation = "";
    playing = false;
    looping = true;
    autoplay = false;
    speed = 1.0f;
    current_frame = 0.0f;
    total_frames = 0.0f;
    duration = 0.0f;

    render_size = Vector2i(512, 512);
    base_picture_size = Vector2i(512, 512);

    canvas = nullptr;
    animation = nullptr;
    picture = nullptr;
    buffer = nullptr;

    fix_alpha_border = true;
    unpremultiply_alpha = false;

    last_rendered_frame_index = -1;
    first_frame_drawn = false;

    _initialize_thorvg();
}

LottieAnimation3D::~LottieAnimation3D() {
    _cleanup_thorvg();
}

void LottieAnimation3D::_initialize_thorvg() {
    // ThorVG's task scheduler is process-wide; init it once. ThorVG ref-counts
    // Initializer::init(), so it is safe even if the 2D node initialised it too.
    static bool thorvg_initialized = false;
    if (!thorvg_initialized) {
        unsigned int hw_threads = std::thread::hardware_concurrency();
        unsigned int threads = hw_threads;
        if (threads == 0) threads = 4;
        if (hw_threads >= 8) threads = hw_threads + 2;

        if (tvg::Initializer::init(threads) != tvg::Result::Success) {
            UtilityFunctions::printerr("LottieAnimation3D: Failed to initialize ThorVG");
            return;
        }
        thorvg_initialized = true;
    }

    canvas = tvg::SwCanvas::gen(tvg::EngineOption::SmartRender);
    if (!canvas) {
        UtilityFunctions::printerr("LottieAnimation3D: Failed to create ThorVG canvas");
        return;
    }

    _allocate_buffer_and_target(render_size);
}

void LottieAnimation3D::_cleanup_thorvg() {
    if (picture && animation && canvas) {
        canvas->remove();
    }
    if (canvas) {
        delete canvas;
        canvas = nullptr;
    }
    if (buffer) {
        delete[] buffer;
        buffer = nullptr;
    }
    // `picture` is owned by `animation`; `animation` is released by ThorVG.
    animation = nullptr;
    picture = nullptr;
}

void LottieAnimation3D::_allocate_buffer_and_target(const Vector2i &size) {
    if (size.x <= 0 || size.y <= 0 || !canvas) {
        return;
    }
    if (buffer) {
        delete[] buffer;
        buffer = nullptr;
    }
    render_size = size;
    buffer = new uint32_t[(size_t)render_size.x * (size_t)render_size.y];
    memset(buffer, 0, (size_t)render_size.x * (size_t)render_size.y * sizeof(uint32_t));
    canvas->target(buffer, render_size.x, render_size.x, render_size.y, tvg::ColorSpace::ARGB8888S);
    pixel_bytes.resize((int64_t)render_size.x * (int64_t)render_size.y * 4);
    _create_texture();
}

void LottieAnimation3D::_create_texture() {
    image = Image::create(render_size.x, render_size.y, false, Image::FORMAT_RGBA8);
    if (image.is_valid()) {
        // Start transparent so nothing flashes before the first frame is drawn.
        image->fill(Color(0, 0, 0, 0));
    }
    lottie_texture = ImageTexture::create_from_image(image);
    // Note: binding the texture to the Sprite3D slot via set_texture() is done
    // by the caller (_ready / set_render_size), never here. _create_texture()
    // runs from the constructor too, and Sprite3D internals must not be touched
    // before the node is constructed and (ideally) in the tree.
}

void LottieAnimation3D::_bind_lottie_texture() {
    // Sprite3D connects to the texture's "changed" signal in set_texture(), so
    // once bound, every ImageTexture::update() call auto-redraws the sprite.
    // Sprite3D::set_texture() already early-outs when the texture is unchanged.
    if (lottie_texture.is_valid()) {
        set_texture(lottie_texture);
    }
}

void LottieAnimation3D::_apply_picture_transform_to_fit() {
    if (!picture) {
        return;
    }
    // Scale the Lottie picture into the render buffer, preserving aspect ratio
    // and centering it (letterboxed). Mirrors the 2D node's fit logic.
    float pw = std::max(1.0f, (float)base_picture_size.x);
    float ph = std::max(1.0f, (float)base_picture_size.y);
    float sx = (float)render_size.x / pw;
    float sy = (float)render_size.y / ph;
    float s = std::min(sx, sy);
    tvg::Matrix m;
    m.e11 = s;    m.e12 = 0.0f; m.e13 = (render_size.x - pw * s) * 0.5f;
    m.e21 = 0.0f; m.e22 = s;    m.e23 = (render_size.y - ph * s) * 0.5f;
    m.e31 = 0.0f; m.e32 = 0.0f; m.e33 = 1.0f;
    picture->transform(m);
}

bool LottieAnimation3D::_load_animation(const String &path) {
    if (path.is_empty() || !canvas) {
        return false;
    }

    // Drop any previously loaded animation.
    if (picture) {
        canvas->remove();
        picture = nullptr;
        animation = nullptr;
    }

    animation = tvg::Animation::gen();
    picture = animation ? animation->picture() : nullptr;
    if (!picture) {
        UtilityFunctions::printerr("LottieAnimation3D: Failed to create ThorVG picture");
        animation = nullptr;
        emit_signal("animation_loaded", false);
        return false;
    }

    // Resolve the JSON to load. `.lottie` bundles are unpacked to user://.
    String source_path = path;
    if (path.to_lower().ends_with(".lottie")) {
        String extracted = lottie::extract_lottie_json_to_cache(path, selected_dotlottie_animation);
        if (extracted.is_empty()) {
            animation = nullptr;
            picture = nullptr;
            emit_signal("animation_loaded", false);
            return false;
        }
        source_path = extracted;
    }

    // Try a direct load; if that fails (e.g. file packed in the PCK, or Web),
    // mirror it to user:// and retry against an absolute path.
    auto try_load = [&](const String &p) -> bool {
        String ap = ProjectSettings::get_singleton()->globalize_path(p);
        return picture->load(ap.utf8().get_data()) == tvg::Result::Success;
    };

    bool loaded_ok = try_load(source_path);
    if (!loaded_ok) {
        String mirrored = lottie::mirror_file_to_user_cache(source_path);
        if (!mirrored.is_empty()) {
            loaded_ok = try_load(mirrored);
        }
    }
    if (!loaded_ok) {
        UtilityFunctions::printerr("LottieAnimation3D: Failed to load Lottie animation: " + source_path);
        animation = nullptr;
        picture = nullptr;
        emit_signal("animation_loaded", false);
        return false;
    }

    duration = animation->duration();
    total_frames = animation->totalFrame();
    current_frame = 0.0f;

    // Query the intrinsic Lottie size for the fit transform.
    float pw = 0.0f, ph = 0.0f;
    picture->size(&pw, &ph);
    if (pw <= 0 || ph <= 0) {
        pw = (float)render_size.x;
        ph = (float)render_size.y;
    }
    base_picture_size = Vector2i((int)std::ceil(pw), (int)std::ceil(ph));

    _apply_picture_transform_to_fit();

    if (canvas->push(picture) != tvg::Result::Success) {
        UtilityFunctions::printerr("LottieAnimation3D: Failed to push picture to canvas");
        animation = nullptr;
        picture = nullptr;
        emit_signal("animation_loaded", false);
        return false;
    }

    // Draw the first frame immediately so the node is visible even when paused.
    last_rendered_frame_index = -1;
    first_frame_drawn = false;
    _render_frame();

    emit_signal("animation_loaded", true);
    return true;
}

void LottieAnimation3D::_unload_animation() {
    playing = false;
    if (picture && canvas) {
        canvas->remove();
    }
    picture = nullptr;
    animation = nullptr;
    duration = 0.0f;
    total_frames = 0.0f;
    current_frame = 0.0f;
    last_rendered_frame_index = -1;
    first_frame_drawn = false;

    // Clear the visible texture.
    if (buffer) {
        memset(buffer, 0, (size_t)render_size.x * (size_t)render_size.y * sizeof(uint32_t));
    }
    if (image.is_valid()) {
        pixel_bytes.fill(0);
        image->set_data(render_size.x, render_size.y, false, Image::FORMAT_RGBA8, pixel_bytes);
        if (lottie_texture.is_valid()) {
            lottie_texture->update(image);
        }
    }
}

void LottieAnimation3D::_update_animation(double delta) {
    if (!playing || !animation || total_frames <= 0) {
        return;
    }

    float prev_frame = current_frame;
    current_frame += (total_frames / duration) * (float)delta * speed;

    if (current_frame >= total_frames) {
        if (looping) {
            current_frame = fmod(current_frame, total_frames);
        } else {
            current_frame = total_frames - 1;
            playing = false;
            emit_signal("animation_finished");
        }
    }

    if ((int)prev_frame != (int)current_frame) {
        emit_signal("frame_changed", current_frame);
    }
}

void LottieAnimation3D::_render_frame() {
    if (!canvas || !animation || !picture || !buffer) {
        return;
    }
    // Skip work when the visible frame would not change.
    int frame_index = (int)std::round(current_frame);
    if (first_frame_drawn && frame_index == last_rendered_frame_index) {
        return;
    }

    animation->frame(current_frame);
    canvas->update();
    canvas->draw(false);
    canvas->sync();

    if (image.is_valid()) {
        const int64_t bytes_needed = (int64_t)render_size.x * (int64_t)render_size.y * 4;
        if (pixel_bytes.size() != bytes_needed) {
            pixel_bytes.resize(bytes_needed);
        }
        lottie::convert_argb_to_rgba(buffer, pixel_bytes.ptrw(), (size_t)render_size.x * (size_t)render_size.y);
        if (unpremultiply_alpha) {
            lottie::unpremultiply_alpha_rgba(pixel_bytes.ptrw(), render_size.x, render_size.y);
        }
        if (fix_alpha_border) {
            lottie::fix_alpha_border_rgba(pixel_bytes.ptrw(), render_size.x, render_size.y);
        }
        image->set_data(render_size.x, render_size.y, false, Image::FORMAT_RGBA8, pixel_bytes);
        if (lottie_texture.is_valid()) {
            // Emits "changed"; Sprite3D re-queues its draw automatically.
            lottie_texture->update(image);
        }
    }

    last_rendered_frame_index = frame_index;
    first_frame_drawn = true;
}

void LottieAnimation3D::_ready() {
    set_process(true);

    // Bind the ThorVG-backed texture now that the node is in the tree.
    _bind_lottie_texture();

    if (!animation_path.is_empty()) {
        if (_load_animation(animation_path) && autoplay) {
            play();
        }
    }
}

void LottieAnimation3D::_process(double delta) {
    if (!playing) {
        return;
    }
    _update_animation(delta);
    _render_frame();
}

void LottieAnimation3D::play() {
    if (!animation) {
        // Lazily load if a path is set but nothing is loaded yet (e.g. play()
        // called before _ready, or a scene saved with playing = true).
        if (!animation_path.is_empty()) {
            _load_animation(animation_path);
        }
        if (!animation) {
            return; // nothing to play
        }
    }
    playing = true;
}

void LottieAnimation3D::stop() {
    playing = false;
    current_frame = 0.0f;
    _render_frame();
}

void LottieAnimation3D::pause() {
    playing = false;
}

void LottieAnimation3D::seek(float frame) {
    set_frame(frame);
}

void LottieAnimation3D::set_frame(float frame) {
    if (total_frames > 0) {
        current_frame = CLAMP(frame, 0.0f, total_frames - 1);
        _render_frame();
    }
}

float LottieAnimation3D::get_frame() const {
    return current_frame;
}

void LottieAnimation3D::set_animation_path(const String &path) {
    if (animation_path == path) {
        return;
    }
    animation_path = path;
    if (is_inside_tree()) {
        if (path.is_empty()) {
            _unload_animation();
        } else if (_load_animation(path) && autoplay) {
            play();
        }
    }
}

String LottieAnimation3D::get_animation_path() const {
    return animation_path;
}

void LottieAnimation3D::set_selected_dotlottie_animation(const String &id) {
    if (selected_dotlottie_animation == id) {
        return;
    }
    selected_dotlottie_animation = id;
    if (!animation_path.is_empty() && animation_path.to_lower().ends_with(".lottie") && is_inside_tree()) {
        bool was_playing = playing;
        if (_load_animation(animation_path) && was_playing) {
            play();
        }
    }
}

String LottieAnimation3D::get_selected_dotlottie_animation() const {
    return selected_dotlottie_animation;
}

void LottieAnimation3D::set_playing(bool p_playing) {
    if (p_playing) {
        play();
    } else {
        pause();
    }
}

bool LottieAnimation3D::is_playing() const {
    return playing;
}

void LottieAnimation3D::set_looping(bool p_looping) {
    looping = p_looping;
}

bool LottieAnimation3D::is_looping() const {
    return looping;
}

void LottieAnimation3D::set_autoplay(bool p_autoplay) {
    autoplay = p_autoplay;
}

bool LottieAnimation3D::is_autoplay() const {
    return autoplay;
}

void LottieAnimation3D::set_speed(float p_speed) {
    speed = MAX(0.0f, p_speed);
}

float LottieAnimation3D::get_speed() const {
    return speed;
}

void LottieAnimation3D::set_render_size(const Vector2i &size) {
    if (size.x <= 0 || size.y <= 0 || render_size == size) {
        return;
    }
    _allocate_buffer_and_target(size); // creates a fresh image + lottie_texture
    _bind_lottie_texture(); // rebind the new texture to the Sprite3D slot
    if (picture) {
        _apply_picture_transform_to_fit();
        last_rendered_frame_index = -1;
        first_frame_drawn = false;
        _render_frame();
    }
}

Vector2i LottieAnimation3D::get_render_size() const {
    return render_size;
}

float LottieAnimation3D::get_duration() const {
    return duration;
}

float LottieAnimation3D::get_total_frames() const {
    return total_frames;
}

Vector2i LottieAnimation3D::get_animation_size() const {
    return base_picture_size;
}
