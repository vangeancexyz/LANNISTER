#include "../chams.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    void **vtable;
    bool ignore_z;
    int references;
} FakeMaterial;

typedef struct {
    void *material;
    bool ignore_z;
    float color[3];
    float blend;
} DrawObservation;

static Interface_t model_render;
static Interface_t material_system;
static Interface_t render_view;
static FakeMaterial hidden_material;
static FakeMaterial visible_material;

static void *current_override;
static int override_count;
static int bad_override_argument_count;
static int original_draw_count;
static DrawObservation observations[8];
static float render_color[3] = {0.20f, 0.40f, 0.60f};
static float render_blend = 0.75f;

static void log_message(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

MsgFn EngineMsg = log_message;

static void dummy_method(void) {}

static void original_draw(Interface_t *self, const void *state,
                          const void *info, void *bones) {
    (void)self;
    (void)state;
    (void)info;
    (void)bones;

    DrawObservation *observation = &observations[original_draw_count++];
    observation->material = current_override;
    observation->ignore_z = current_override
        ? ((FakeMaterial *)current_override)->ignore_z
        : false;
    memcpy(observation->color, render_color, sizeof render_color);
    observation->blend = render_blend;
}

static void force_override(Interface_t *self, void *value, int type,
                           int overrides) {
    (void)self;
    if (type != 0 || overrides != 0)
        ++bad_override_argument_count;
    current_override = value;
    ++override_count;
}

static void set_blend(Interface_t *self, float value) {
    (void)self;
    render_blend = value;
}

static void set_color(Interface_t *self, const float *value) {
    (void)self;
    memcpy(render_color, value, sizeof render_color);
}

static void add_reference(void *self) {
    ++((FakeMaterial *)self)->references;
}

static void release_reference(void *self) {
    --((FakeMaterial *)self)->references;
}

static bool is_error_material(void *self) {
    (void)self;
    return false;
}

static void *find_material(Interface_t *self, const char *name,
                           const char *group, bool complain,
                           const char *prefix) {
    (void)self;
    (void)group;
    (void)complain;
    (void)prefix;
    const char *path = NULL;
    void *result = NULL;
    if (strcmp(name, HIDDEN_MATERIAL_NAME) == 0) {
        path = "tests/cstrike/materials/cssourcex64_chams_lannister/invisible.vmt";
        result = &hidden_material;
    } else if (strcmp(name, VISIBLE_MATERIAL_NAME) == 0) {
        path = "tests/cstrike/materials/cssourcex64_chams_lannister/visible.vmt";
        result = &visible_material;
    }
    if (path) {
        FILE *file = fopen(path, "r");
        assert(file != NULL);
        char contents[512] = {0};
        const size_t size = fread(contents, 1, sizeof contents - 1, file);
        assert(size > 0);
        fclose(file);
        const char *expected = result == &hidden_material
            ? "\"$ignorez\"     \"1\""
            : "\"$ignorez\"     \"0\"";
        assert(strstr(contents, expected) != NULL);
    }
    if (result) return result;
    return NULL;
}

static void fill_vtable(void **table, size_t count) {
    for (size_t i = 0; i < count; ++i)
        table[i] = (void *)dummy_method;
}

static void build_model_info(unsigned char info[88], int entity_index) {
    memset(info, 0, 88);
    memcpy(info + 68, &entity_index, sizeof entity_index);
}

int main(void) {
    assert(mkdir("tests/cstrike", 0755) == 0 || errno == EEXIST);
    assert(mkdir("tests/cstrike/materials", 0755) == 0 || errno == EEXIST);

    void **model_render_table = calloc(32, sizeof *model_render_table);
    void **material_system_table = calloc(80, sizeof *material_system_table);
    void **render_view_table = calloc(12, sizeof *render_view_table);
    void **material_table = calloc(48, sizeof *material_table);
    assert(model_render_table && material_system_table && render_view_table &&
           material_table);

    fill_vtable(model_render_table, 32);
    fill_vtable(material_system_table, 80);
    fill_vtable(render_view_table, 12);
    fill_vtable(material_table, 48);

    model_render_table[IDX_FORCED_MAT_OVERRIDE] = (void *)force_override;
    model_render_table[IDX_DRAW_MODEL_EXECUTE] = (void *)original_draw;
    material_system_table[IDX_FIND_MATERIAL] = (void *)find_material;
    render_view_table[IDX_RENDER_SET_BLEND] = (void *)set_blend;
    render_view_table[IDX_RENDER_SET_COLOR] = (void *)set_color;
    material_table[IDX_INCREMENT_REF_COUNT] = (void *)add_reference;
    material_table[IDX_DECREMENT_REF_COUNT] = (void *)release_reference;
    material_table[IDX_IS_ERROR_MATERIAL] = (void *)is_error_material;

    model_render.vtable = model_render_table;
    material_system.vtable = material_system_table;
    render_view.vtable = render_view_table;
    hidden_material.vtable = material_table;
    hidden_material.ignore_z = true;
    visible_material.vtable = material_table;
    visible_material.ignore_z = false;

    assert(chams_initialize(&model_render, &material_system, &render_view));
    assert(hidden_material.references == 1);
    assert(visible_material.references == 1);

    unsigned char info[88];
    build_model_info(info, 2);
    DrawModelExecuteFn hooked =
        (DrawModelExecuteFn)model_render.vtable[IDX_DRAW_MODEL_EXECUTE];
    hooked(&model_render, NULL, info, NULL);

    assert(original_draw_count == 2);
    assert(override_count == 3);
    assert(bad_override_argument_count == 0);
    assert(observations[0].material == &hidden_material);
    assert(observations[0].ignore_z);
    assert(observations[0].color[0] == 0.94f);
    assert(observations[0].color[1] == 0.27f);
    assert(observations[1].material == &visible_material);
    assert(!observations[1].ignore_z);
    assert(observations[1].color[0] == 0.13f);
    assert(observations[1].color[1] == 0.77f);
    assert(current_override == NULL);
    assert(render_color[0] == 1.0f && render_color[1] == 1.0f &&
           render_color[2] == 1.0f);
    assert(render_blend == 1.0f);

    build_model_info(info, 65);
    hooked(&model_render, NULL, info, NULL);
    assert(original_draw_count == 3);
    assert(override_count == 3);

    hooked(&model_render, NULL, (const void *)(uintptr_t)1, NULL);
    assert(original_draw_count == 4);
    assert(override_count == 3);

    assert(unlink("tests/cstrike/materials/cssourcex64_chams_lannister/invisible.vmt") == 0);
    assert(unlink("tests/cstrike/materials/cssourcex64_chams_lannister/visible.vmt") == 0);
    assert(rmdir("tests/cstrike/materials/cssourcex64_chams_lannister") == 0);
    assert(rmdir("tests/cstrike/materials") == 0);
    assert(rmdir("tests/cstrike") == 0);

    puts("chams_harness: PASS");
    return 0;
}
